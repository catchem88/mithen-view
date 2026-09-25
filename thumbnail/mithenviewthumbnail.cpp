// MithenView Shell Thumbnail Provider
// In-process COM server that supplies Windows Explorer with native thumbnails for image files.
// Uses Windows Imaging Component directly so it has no dependency on the Qt runtime.

#include <windows.h>
#include <thumbcache.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <aclapi.h>
#include <stdio.h>
#include <new>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")

namespace
{

// {7A2E5C31-9B4D-4E8A-9C1F-3D6B7E0A4F52}
const CLSID CLSID_MithenViewThumbnailProvider =
    {0x7a2e5c31, 0x9b4d, 0x4e8a, {0x9c, 0x1f, 0x3d, 0x6b, 0x7e, 0x0a, 0x4f, 0x52}};

// {E357FCCD-A995-4576-B01F-234630154E96} - the shell's IThumbnailProvider handler key
const wchar_t *const kThumbnailProviderHandlerKey = L"{e357fccd-a995-4576-b01f-234630154e96}";

// Value name used to remember a competing per-user handler that this provider replaced
const wchar_t *const kThumbnailProviderBackupValue = L"MithenViewPreviousThumbnailHandler";

// Value name under which the original DACL of a protected handler key is stored for restore
const wchar_t *const kThumbnailProviderOriginalDaclValue = L"MithenViewOriginalDacl";

// Rights denied to the current user on a protected handler key so that other software
// (e.g. Google Drive, which re-asserts its broken handler every ~45s) cannot overwrite it
const REGSAM kProtectedKeyDeniedRights = KEY_SET_VALUE | KEY_CREATE_SUB_KEY | DELETE;

// Extensions handled by this provider (all lowercase, including the dot)
const wchar_t *const kSupportedExtensions[] = {
    L".png", L".jpg", L".jpeg", L".jpe", L".jfif", L".bmp", L".dib", L".gif",
    L".tif", L".tiff", L".ico", L".webp", L".heic", L".heif", L".avif", L".wdp", L".jxr"
};

// Handlers shipped by Windows itself; never shadow these with an override
const wchar_t *const kBuiltInHandlerClsids[] = {
    L"{C7657C4A-9F68-40FA-A4DF-96BC08EB3551}", // Shell image thumbnail handler
    L"{9DBD2C50-62AD-11D0-B806-00C04FD706EC}"  // Shell media/video thumbnail handler
};

HINSTANCE g_hInstance = nullptr;
LONG g_cRefDll = 0;

void CalculateThumbnailSize(const UINT srcWidth, const UINT srcHeight, const UINT requestedSize, UINT *dstWidth, UINT *dstHeight)
{
    UINT target = requestedSize > 0 ? requestedSize : 256;

    if (srcWidth >= srcHeight)
    {
        if (target > srcWidth)
            target = srcWidth;
        *dstWidth = target;
        *dstHeight = static_cast<UINT>(static_cast<UINT64>(srcHeight) * target / srcWidth);
    }
    else
    {
        if (target > srcHeight)
            target = srcHeight;
        *dstHeight = target;
        *dstWidth = static_cast<UINT>(static_cast<UINT64>(srcWidth) * target / srcHeight);
    }

    if (*dstWidth == 0)
        *dstWidth = 1;
    if (*dstHeight == 0)
        *dstHeight = 1;
}

// Maps the EXIF orientation tag onto a WIC transform so photos are not shown sideways
WICBitmapTransformOptions GetOrientationTransform(IWICBitmapFrameDecode *pFrame)
{
    WICBitmapTransformOptions options = WICBitmapTransformRotate0;

    IWICMetadataQueryReader *pReader = nullptr;
    if (SUCCEEDED(pFrame->GetMetadataQueryReader(&pReader)) && pReader)
    {
        PROPVARIANT prop;
        PropVariantInit(&prop);
        if (SUCCEEDED(pReader->GetMetadataByName(L"/app1/ifd/{ushort=274}", &prop)) && prop.vt == VT_UI2)
        {
            switch (prop.uiVal)
            {
            case 3:
                options = WICBitmapTransformRotate180;
                break;
            case 6:
                options = WICBitmapTransformRotate90;
                break;
            case 8:
                options = WICBitmapTransformRotate270;
                break;
            default:
                break;
            }
        }
        PropVariantClear(&prop);
        pReader->Release();
    }

    return options;
}

class CThumbnailProvider : public IThumbnailProvider,
                           public IInitializeWithStream,
                           public IInitializeWithFile
{
public:
    CThumbnailProvider() : m_cRef(1), m_pStream(nullptr)
    {
        InterlockedIncrement(&g_cRefDll);
    }

    ~CThumbnailProvider()
    {
        if (m_pStream)
            m_pStream->Release();
        InterlockedDecrement(&g_cRefDll);
    }

    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override
    {
        if (!ppv)
            return E_POINTER;

        *ppv = nullptr;

        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_IThumbnailProvider))
        {
            *ppv = static_cast<IThumbnailProvider *>(this);
        }
        else if (IsEqualIID(riid, IID_IInitializeWithStream))
        {
            *ppv = static_cast<IInitializeWithStream *>(this);
        }
        else if (IsEqualIID(riid, IID_IInitializeWithFile))
        {
            *ppv = static_cast<IInitializeWithFile *>(this);
        }
        else
        {
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return InterlockedIncrement(&m_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release() override
    {
        const ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
            delete this;
        return cRef;
    }

    IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode) override
    {
        UNREFERENCED_PARAMETER(grfMode);

        if (!pStream)
            return E_INVALIDARG;

        if (m_pStream)
            return E_UNEXPECTED;

        m_pStream = pStream;
        m_pStream->AddRef();
        return S_OK;
    }

    IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override
    {
        UNREFERENCED_PARAMETER(grfMode);

        if (!pszFilePath)
            return E_INVALIDARG;

        if (m_pStream)
            return E_UNEXPECTED;

        return SHCreateStreamOnFileEx(pszFilePath, STGM_READ, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &m_pStream);
    }

    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) override
    {
        if (!phbmp || !pdwAlpha)
            return E_POINTER;

        *phbmp = nullptr;
        *pdwAlpha = WTSAT_UNKNOWN;

        if (!m_pStream)
            return E_UNEXPECTED;

        IWICImagingFactory *pFactory = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
        if (FAILED(hr) || !pFactory)
            return hr;

        IWICBitmapDecoder *pDecoder = nullptr;
        hr = pFactory->CreateDecoderFromStream(m_pStream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder);
        if (SUCCEEDED(hr) && pDecoder)
        {
            IWICBitmapFrameDecode *pFrame = nullptr;
            hr = pDecoder->GetFrame(0, &pFrame);
            if (SUCCEEDED(hr) && pFrame)
            {
                hr = CreateThumbnailBitmap(pFactory, pFrame, cx, phbmp, pdwAlpha);
                pFrame->Release();
            }
            pDecoder->Release();
        }

        pFactory->Release();
        return hr;
    }

private:
    HRESULT CreateThumbnailBitmap(IWICImagingFactory *pFactory, IWICBitmapFrameDecode *pFrame, UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
    {
        HRESULT hr = S_OK;

        // Apply the EXIF orientation before measuring, so the aspect ratio is correct
        IWICBitmapSource *pSource = nullptr;
        const WICBitmapTransformOptions orientation = GetOrientationTransform(pFrame);
        if (orientation != WICBitmapTransformRotate0)
        {
            IWICBitmapFlipRotator *pRotator = nullptr;
            hr = pFactory->CreateBitmapFlipRotator(&pRotator);
            if (SUCCEEDED(hr) && pRotator)
            {
                hr = pRotator->Initialize(pFrame, orientation);
                if (SUCCEEDED(hr))
                    pSource = pRotator;
                else
                    pRotator->Release();
            }
        }

        if (!pSource)
        {
            pSource = pFrame;
            pSource->AddRef();
            hr = S_OK;
        }

        UINT srcWidth = 0;
        UINT srcHeight = 0;
        hr = pSource->GetSize(&srcWidth, &srcHeight);
        if (SUCCEEDED(hr) && srcWidth > 0 && srcHeight > 0)
        {
            UINT dstWidth = 0;
            UINT dstHeight = 0;
            CalculateThumbnailSize(srcWidth, srcHeight, cx, &dstWidth, &dstHeight);

            IWICFormatConverter *pConverter = nullptr;
            hr = pFactory->CreateFormatConverter(&pConverter);
            if (SUCCEEDED(hr) && pConverter)
            {
                hr = pConverter->Initialize(pSource, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                                            nullptr, 0.0, WICBitmapPaletteTypeCustom);
                if (SUCCEEDED(hr))
                {
                    IWICBitmapSource *pScaledSource = nullptr;
                    if (dstWidth == srcWidth && dstHeight == srcHeight)
                    {
                        pScaledSource = pConverter;
                        pScaledSource->AddRef();
                    }
                    else
                    {
                        IWICBitmapScaler *pScaler = nullptr;
                        hr = pFactory->CreateBitmapScaler(&pScaler);
                        if (SUCCEEDED(hr) && pScaler)
                        {
                            hr = pScaler->Initialize(pConverter, dstWidth, dstHeight, WICBitmapInterpolationModeFant);
                            if (SUCCEEDED(hr))
                                pScaledSource = pScaler;
                            else
                                pScaler->Release();
                        }
                    }

                    if (SUCCEEDED(hr) && pScaledSource)
                    {
                        hr = CreateHBITMAPFromSource(pScaledSource, dstWidth, dstHeight, phbmp);
                        if (SUCCEEDED(hr))
                            *pdwAlpha = WTSAT_ARGB;
                        pScaledSource->Release();
                    }
                }
                pConverter->Release();
            }
        }

        pSource->Release();
        return hr;
    }

    HRESULT CreateHBITMAPFromSource(IWICBitmapSource *pSource, const UINT width, const UINT height, HBITMAP *phbmp)
    {
        BITMAPINFO bitmapInfo = {};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = static_cast<LONG>(width);
        bitmapInfo.bmiHeader.biHeight = -static_cast<LONG>(height); // top-down, matching WIC's row order
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        void *pBits = nullptr;
        const HDC hdc = GetDC(nullptr);
        HBITMAP hBitmap = CreateDIBSection(hdc, &bitmapInfo, DIB_RGB_COLORS, &pBits, nullptr, 0);
        ReleaseDC(nullptr, hdc);

        if (!hBitmap || !pBits)
        {
            if (hBitmap)
                DeleteObject(hBitmap);
            return E_FAIL;
        }

        const UINT stride = width * 4;
        const WICRect rect = {0, 0, static_cast<INT>(width), static_cast<INT>(height)};
        const HRESULT hr = pSource->CopyPixels(&rect, stride, stride * height, static_cast<BYTE *>(pBits));

        if (FAILED(hr))
        {
            DeleteObject(hBitmap);
            return hr;
        }

        *phbmp = hBitmap;
        return S_OK;
    }

    LONG m_cRef;
    IStream *m_pStream;
};

class CClassFactory : public IClassFactory
{
public:
    CClassFactory() : m_cRef(1)
    {
        InterlockedIncrement(&g_cRefDll);
    }

    ~CClassFactory()
    {
        InterlockedDecrement(&g_cRefDll);
    }

    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override
    {
        if (!ppv)
            return E_POINTER;

        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
        {
            *ppv = static_cast<IClassFactory *>(this);
            AddRef();
            return S_OK;
        }

        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return InterlockedIncrement(&m_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release() override
    {
        const ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
            delete this;
        return cRef;
    }

    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) override
    {
        if (pUnkOuter)
            return CLASS_E_NOAGGREGATION;

        CThumbnailProvider *pProvider = new (std::nothrow) CThumbnailProvider();
        if (!pProvider)
            return E_OUTOFMEMORY;

        const HRESULT hr = pProvider->QueryInterface(riid, ppv);
        pProvider->Release();
        return hr;
    }

    IFACEMETHODIMP LockServer(BOOL fLock) override
    {
        if (fLock)
            InterlockedIncrement(&g_cRefDll);
        else
            InterlockedDecrement(&g_cRefDll);
        return S_OK;
    }

private:
    LONG m_cRef;
};

HRESULT SetStringValue(HKEY hKey, const wchar_t *subKey, const wchar_t *valueName, const wchar_t *value)
{
    HKEY hSubKey = nullptr;
    const LSTATUS status = RegCreateKeyExW(hKey, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                                           KEY_WRITE, nullptr, &hSubKey, nullptr);
    if (status != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(status);

    const DWORD size = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
    const LSTATUS setStatus = RegSetValueExW(hSubKey, valueName, 0, REG_SZ,
                                             reinterpret_cast<const BYTE *>(value), size);
    RegCloseKey(hSubKey);

    return setStatus == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(setStatus);
}

HRESULT RegisterInprocServer(HKEY rootKey, const CLSID &clsid, const wchar_t *dllPath, const wchar_t *friendlyName)
{
    wchar_t clsidString[64] = {};
    if (StringFromGUID2(clsid, clsidString, ARRAYSIZE(clsidString)) == 0)
        return E_FAIL;

    wchar_t subKey[MAX_PATH] = {};
    swprintf_s(subKey, ARRAYSIZE(subKey), L"SOFTWARE\\Classes\\CLSID\\%s", clsidString);

    HRESULT hr = SetStringValue(rootKey, subKey, nullptr, friendlyName);
    if (FAILED(hr))
        return hr;

    wchar_t inprocKey[MAX_PATH] = {};
    swprintf_s(inprocKey, ARRAYSIZE(inprocKey), L"%s\\InprocServer32", subKey);

    hr = SetStringValue(rootKey, inprocKey, nullptr, dllPath);
    if (FAILED(hr))
        return hr;

    return SetStringValue(rootKey, inprocKey, L"ThreadingModel", L"Both");
}

HRESULT RegisterShellExtension(HKEY rootKey, const CLSID &clsid, const wchar_t *extension)
{
    wchar_t clsidString[64] = {};
    if (StringFromGUID2(clsid, clsidString, ARRAYSIZE(clsidString)) == 0)
        return E_FAIL;

    wchar_t subKey[MAX_PATH] = {};
    swprintf_s(subKey, ARRAYSIZE(subKey),
               L"SOFTWARE\\Classes\\SystemFileAssociations\\%s\\ShellEx\\%s",
               extension, kThumbnailProviderHandlerKey);

    return SetStringValue(rootKey, subKey, nullptr, clsidString);
}

// Registers under HKEY_LOCAL_MACHINE when elevated, otherwise falls back to the per-user hive
HRESULT RegisterAll(const HKEY rootKey)
{
    wchar_t modulePath[MAX_PATH] = {};
    if (GetModuleFileNameW(g_hInstance, modulePath, ARRAYSIZE(modulePath)) == 0)
        return HRESULT_FROM_WIN32(GetLastError());

    HRESULT hr = RegisterInprocServer(rootKey, CLSID_MithenViewThumbnailProvider, modulePath, L"MithenView Thumbnail Provider");
    if (FAILED(hr))
        return hr;

    for (size_t i = 0; i < ARRAYSIZE(kSupportedExtensions); i++)
    {
        hr = RegisterShellExtension(rootKey, CLSID_MithenViewThumbnailProvider, kSupportedExtensions[i]);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

void UnregisterAll(const HKEY rootKey)
{
    wchar_t clsidString[64] = {};
    if (StringFromGUID2(CLSID_MithenViewThumbnailProvider, clsidString, ARRAYSIZE(clsidString)) == 0)
        return;

    wchar_t subKey[MAX_PATH] = {};
    swprintf_s(subKey, ARRAYSIZE(subKey), L"SOFTWARE\\Classes\\CLSID\\%s", clsidString);
    RegDeleteTreeW(rootKey, subKey);

    for (size_t i = 0; i < ARRAYSIZE(kSupportedExtensions); i++)
    {
        swprintf_s(subKey, ARRAYSIZE(subKey),
                   L"SOFTWARE\\Classes\\SystemFileAssociations\\%s\\ShellEx\\%s",
                   kSupportedExtensions[i], kThumbnailProviderHandlerKey);
        RegDeleteTreeW(rootKey, subKey);
    }
}

// Locks a handler key we just claimed so competing software cannot overwrite it. The original
// DACL is saved first so the uninstaller can restore the exact prior state.
HRESULT ProtectHandlerKey(const wchar_t *hiveSubKey)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, hiveSubKey, 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(GetLastError());

    PSECURITY_DESCRIPTOR pSecurityDescriptor = nullptr;
    PACL pOriginalDacl = nullptr;
    DWORD dwResult = GetSecurityInfo(hKey, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION,
                                     nullptr, nullptr, &pOriginalDacl, nullptr, &pSecurityDescriptor);

    if (dwResult == ERROR_SUCCESS && pOriginalDacl && pOriginalDacl->AclSize > 0)
    {
        // Save the pre-protection DACL once (only if not already saved)
        DWORD existingType = 0;
        DWORD existingSize = 0;
        if (RegQueryValueExW(hKey, kThumbnailProviderOriginalDaclValue, nullptr, &existingType, nullptr, &existingSize) != ERROR_SUCCESS)
        {
            RegSetValueExW(hKey, kThumbnailProviderOriginalDaclValue, 0, REG_BINARY,
                           reinterpret_cast<const BYTE *>(pOriginalDacl), pOriginalDacl->AclSize);
        }
    }

    PACL pNewDacl = nullptr;
    if (dwResult == ERROR_SUCCESS && pOriginalDacl)
    {
        // Deny value writes, subkey creation and deletion to the current user
        BYTE tokenBuffer[256] = {};
        DWORD tokenSize = sizeof(tokenBuffer);
        PSID pUserSid = nullptr;
        if (GetTokenInformation(GetCurrentProcessToken(), TokenUser, tokenBuffer, tokenSize, &tokenSize))
            pUserSid = reinterpret_cast<TOKEN_USER *>(tokenBuffer)->User.Sid;

        if (pUserSid)
        {
            EXPLICIT_ACCESS explicitAccess = {};
            explicitAccess.grfAccessPermissions = kProtectedKeyDeniedRights;
            explicitAccess.grfAccessMode = DENY_ACCESS;
            explicitAccess.grfInheritance = NO_INHERITANCE;
            explicitAccess.Trustee.TrusteeForm = TRUSTEE_IS_SID;
            explicitAccess.Trustee.ptstrName = reinterpret_cast<LPWSTR>(pUserSid);

            dwResult = SetEntriesInAcl(1, &explicitAccess, pOriginalDacl, &pNewDacl);
            if (dwResult == ERROR_SUCCESS && pNewDacl)
            {
                dwResult = SetSecurityInfo(hKey, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION,
                                           nullptr, nullptr, pNewDacl, nullptr);
                LocalFree(pNewDacl);
            }
        }
    }

    if (pSecurityDescriptor)
        LocalFree(pSecurityDescriptor);
    RegCloseKey(hKey);

    return dwResult == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(dwResult);
}

// Reverts the protection applied by ProtectHandlerKey using the saved original DACL.
void RestoreHandlerKey(const wchar_t *hiveSubKey)
{
    // Deliberately do not request SET_VALUE/DELETE, which are denied while protected
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, hiveSubKey, 0, KEY_QUERY_VALUE | READ_CONTROL | WRITE_DAC, &hKey) != ERROR_SUCCESS)
        return;

    DWORD cbDacl = 0;
    if (RegQueryValueExW(hKey, kThumbnailProviderOriginalDaclValue, nullptr, nullptr, nullptr, &cbDacl) == ERROR_SUCCESS && cbDacl > 0)
    {
        BYTE *pDaclBytes = new (std::nothrow) BYTE[cbDacl];
        if (pDaclBytes)
        {
            if (RegQueryValueExW(hKey, kThumbnailProviderOriginalDaclValue, nullptr, nullptr, pDaclBytes, &cbDacl) == ERROR_SUCCESS)
            {
                SetSecurityInfo(hKey, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION,
                                nullptr, nullptr, reinterpret_cast<PACL>(pDaclBytes), nullptr);
            }
            delete[] pDaclBytes;
        }
    }

    RegCloseKey(hKey);

    // The protection is gone now, so the saved DACL value can be removed
    HKEY hClean = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, hiveSubKey, 0, KEY_SET_VALUE, &hClean) == ERROR_SUCCESS)
    {
        RegDeleteValueW(hClean, kThumbnailProviderOriginalDaclValue);
        RegCloseKey(hClean);
    }
}

// Extension-level registrations take precedence over SystemFileAssociations, so a competing
// handler (e.g. Google Drive's) registered directly under .ext\ShellEx would keep winning.
// Override it per-user only, keeping the machine-wide registration of other apps untouched and
// remembering their value so it can be restored on uninstall.
void OverrideExtensionLevelHandlers()
{
    wchar_t clsidString[64] = {};
    if (StringFromGUID2(CLSID_MithenViewThumbnailProvider, clsidString, ARRAYSIZE(clsidString)) == 0)
        return;

    for (size_t i = 0; i < ARRAYSIZE(kSupportedExtensions); i++)
    {
        // HKEY_CLASSES_ROOT is the merged view and takes a bare ".ext\ShellEx\..." path,
        // while a specific hive needs the "SOFTWARE\Classes\..." prefix
        wchar_t classSubKey[MAX_PATH] = {};
        swprintf_s(classSubKey, ARRAYSIZE(classSubKey), L"%s\\ShellEx\\%s",
                   kSupportedExtensions[i], kThumbnailProviderHandlerKey);

        wchar_t hiveSubKey[MAX_PATH] = {};
        swprintf_s(hiveSubKey, ARRAYSIZE(hiveSubKey), L"SOFTWARE\\Classes\\%s", classSubKey);

        wchar_t existing[MAX_PATH] = {};
        DWORD size = sizeof(existing);
        if (RegGetValueW(HKEY_CLASSES_ROOT, classSubKey, nullptr, RRF_RT_REG_SZ, nullptr, existing, &size) != ERROR_SUCCESS)
            continue;

        if (_wcsicmp(existing, clsidString) == 0)
            continue;

        // Leave Windows' own handlers alone
        bool isBuiltInHandler = false;
        for (size_t j = 0; j < ARRAYSIZE(kBuiltInHandlerClsids); j++)
        {
            if (_wcsicmp(existing, kBuiltInHandlerClsids[j]) == 0)
            {
                isBuiltInHandler = true;
                break;
            }
        }
        if (isBuiltInHandler)
            continue;

        // Preserve the handler we are replacing so it can be restored on uninstall
        SetStringValue(HKEY_CURRENT_USER, hiveSubKey, kThumbnailProviderBackupValue, existing);
        SetStringValue(HKEY_CURRENT_USER, hiveSubKey, nullptr, clsidString);

        // Lock the key so the competitor cannot overwrite our handler
        ProtectHandlerKey(hiveSubKey);
    }
}

void UnregisterExtensionLevelOverrides()
{
    wchar_t clsidString[64] = {};
    if (StringFromGUID2(CLSID_MithenViewThumbnailProvider, clsidString, ARRAYSIZE(clsidString)) == 0)
        return;

    for (size_t i = 0; i < ARRAYSIZE(kSupportedExtensions); i++)
    {
        wchar_t classSubKey[MAX_PATH] = {};
        swprintf_s(classSubKey, ARRAYSIZE(classSubKey), L"%s\\ShellEx\\%s",
                   kSupportedExtensions[i], kThumbnailProviderHandlerKey);

        wchar_t hiveSubKey[MAX_PATH] = {};
        swprintf_s(hiveSubKey, ARRAYSIZE(hiveSubKey), L"SOFTWARE\\Classes\\%s", classSubKey);

        // Lift the write protection before reading/removing our entry
        RestoreHandlerKey(hiveSubKey);

        wchar_t existing[MAX_PATH] = {};
        DWORD size = sizeof(existing);
        if (RegGetValueW(HKEY_CURRENT_USER, hiveSubKey, nullptr, RRF_RT_REG_SZ, nullptr, existing, &size) != ERROR_SUCCESS ||
            _wcsicmp(existing, clsidString) != 0)
        {
            continue;
        }

        // Put back whatever handler we replaced, otherwise remove our own entry
        wchar_t backup[MAX_PATH] = {};
        size = sizeof(backup);
        if (RegGetValueW(HKEY_CURRENT_USER, hiveSubKey, kThumbnailProviderBackupValue,
                         RRF_RT_REG_SZ, nullptr, backup, &size) == ERROR_SUCCESS)
        {
            SetStringValue(HKEY_CURRENT_USER, hiveSubKey, nullptr, backup);

            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, hiveSubKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
            {
                RegDeleteValueW(hKey, kThumbnailProviderBackupValue);
                RegCloseKey(hKey);
            }
        }
        else
        {
            RegDeleteTreeW(HKEY_CURRENT_USER, hiveSubKey);
        }
    }
}

} // namespace

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    if (!IsEqualCLSID(rclsid, CLSID_MithenViewThumbnailProvider))
        return CLASS_E_CLASSNOTAVAILABLE;

    CClassFactory *pFactory = new (std::nothrow) CClassFactory();
    if (!pFactory)
        return E_OUTOFMEMORY;

    const HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return g_cRefDll > 0 ? S_FALSE : S_OK;
}

STDAPI DllRegisterServer()
{
    HRESULT hr = RegisterAll(HKEY_LOCAL_MACHINE);
    if (FAILED(hr))
        hr = RegisterAll(HKEY_CURRENT_USER);

    if (SUCCEEDED(hr))
        OverrideExtensionLevelHandlers();

    if (SUCCEEDED(hr))
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    return hr;
}

STDAPI DllUnregisterServer()
{
    UnregisterAll(HKEY_LOCAL_MACHINE);
    UnregisterAll(HKEY_CURRENT_USER);
    UnregisterExtensionLevelOverrides();

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hInstance = hInstance;
        DisableThreadLibraryCalls(hInstance);
    }

    return TRUE;
}
