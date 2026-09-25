#include "qvwin32functions.h"

#include "ShlObj_core.h"
#include "shobjidl.h"
#include "shlwapi.h"
#include "Objbase.h"
#include "appmodel.h"
#include "servprov.h"
#include "shlguid.h"

#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QCollator>
#include <QVersionNumber>
#include <QXmlStreamReader>
#include <QWindow>

#include <QDebug>

QList<OpenWith::OpenWithItem> QVWin32Functions::getOpenWithItems(const QString &filePath, const bool loadIcons)
{
    QList<OpenWith::OpenWithItem> listOfOpenWithItems;

    QFileInfo info(filePath);
    QString extension = "." + info.suffix();

    // Get default program first
    WCHAR assocString[MAX_PATH];
    DWORD assocStringSize = MAX_PATH;
    AssocQueryStringW(0, ASSOCSTR_FRIENDLYAPPNAME, qUtf16Printable(extension), L"open", assocString, &assocStringSize);

    QString defaultProgramName = QString::fromWCharArray(assocString);

    // Get the big list of recommended assochandlers for this file type
    IEnumAssocHandlers *assocHandlers = 0;
    if (!SUCCEEDED(SHAssocEnumHandlers(qUtf16Printable(extension), ASSOC_FILTER_RECOMMENDED, &assocHandlers)))
    {
        qDebug() << "Failed to retrieve enum handlers";
        return listOfOpenWithItems;
    }

    ULONG retrieved = 0;
    IAssocHandler *retrievedHandlers = 0; // this is an array with one element
    while (assocHandlers->Next(1, &retrievedHandlers, &retrieved) == S_OK)
    {
        IAssocHandler &assocHandler = retrievedHandlers[0];

        // Get UI name
        WCHAR *uiName = 0;
        if (!SUCCEEDED(assocHandler.GetUIName(&uiName)))
            continue;

        // Get "name" (this is exec)
        WCHAR *name = 0;
        if (!SUCCEEDED(assocHandler.GetName(&name)))
            continue;

        // Get icon path and index (index is not used in this program)
        WCHAR *icon = 0;
        int iconIndex = 0;
        if (!SUCCEEDED(assocHandler.GetIconLocation(&icon, &iconIndex)))
            continue;

        // Set OpenWithItem fields
        OpenWith::OpenWithItem openWithItem;
        openWithItem.name = QString::fromWCharArray(uiName);
        openWithItem.exec = QString::fromWCharArray(name);
        openWithItem.isDefault = openWithItem.name == defaultProgramName;
        openWithItem.winAssocHandler = &assocHandler;

        QString iconLocation = QString::fromWCharArray(icon);
        bool isAppx = iconLocation.contains("ms-resource");

        // Don't include MithenView in open with menu
        if (openWithItem.name == "MithenView")
            continue;

        // Validity check
        if (!QFile(openWithItem.exec).exists() && !isAppx)
        {
            qDebug() << openWithItem.name << "is an invalid openwith entry";
            continue;
        }

        // Replace ampersands with escaped ampersands for menu items
        openWithItem.name.replace("&", "&&");

        if (loadIcons)
        {
            // Set an icon
            if (isAppx)
            {
                WCHAR realIconPath[MAX_PATH];
                SHLoadIndirectString(icon, realIconPath, MAX_PATH, NULL);
                openWithItem.icon = QIcon(QString::fromWCharArray(realIconPath));
            }
            else
            {
                QFileIconProvider iconProvider;
                openWithItem.icon = iconProvider.icon(QFileInfo(iconLocation));
            }
        }

        listOfOpenWithItems.append(openWithItem);
    }

    return listOfOpenWithItems;
}

void QVWin32Functions::openWithInvokeAssocHandler(const QString &filePath, void *winAssocHandler)
{
    const QString &nativeFilePath = QDir::toNativeSeparators(filePath);

    // Create IShellItem
    IShellItem *shellItem = 0;
    HRESULT result = SHCreateItemFromParsingName(qUtf16Printable(nativeFilePath), NULL, IID_IShellItem, (void**)&shellItem);
    if (!SUCCEEDED(result))
    {
        qDebug() << "Failed to create IShellItem for " << filePath;
        return;
    }

    // Get IDataObject from that
    IDataObject *dataObject = 0;
    if (!SUCCEEDED(shellItem->BindToHandler(NULL, BHID_DataObject, IID_IDataObject, (void**)&dataObject)))
    {
        qDebug() << "Failed to get IDataObject from IShellItem";
        return;
    }

    // Cast passed IAssocHandler and invoke
    IAssocHandler *assocHandler = static_cast<IAssocHandler*>(winAssocHandler);
    assocHandler->Invoke(dataObject);
}

void QVWin32Functions::showOpenWithDialog(const QString &filePath, const QWindow *parent)
{
    const QString &nativeFilePath = QDir::toNativeSeparators(filePath);
    const OPENASINFO info = {
        qUtf16Printable(nativeFilePath),
        0,
        OAIF_EXEC
    };

    HWND winId = reinterpret_cast<HWND>(parent->winId());
    if (!SUCCEEDED(SHOpenWithDialog(winId, &info)))
        qDebug() << "Failed launching open with dialog";
}

static QString getLongOrShortPath(const QString &path, std::function<DWORD(LPCWSTR, LPWSTR, DWORD)> getPathNameFunction)
{
    const bool isUnc = path.startsWith(R"(\\)");
    const QString inputString = isUnc ? (R"(\\?\UNC\)" + path.mid(2)) : (R"(\\?\)" + path);
    const wchar_t *input = reinterpret_cast<const wchar_t*>(inputString.utf16());
    const DWORD outputSize = getPathNameFunction(input, nullptr, 0);
    if (outputSize == 0)
        return {};
    QVarLengthArray<wchar_t, MAX_PATH> output(outputSize);
    if (getPathNameFunction(input, output.data(), output.size()) == 0)
        return {};
    const QString outputString = QString::fromWCharArray(output.data());
    return isUnc ? (R"(\\)" + outputString.mid(8)) : outputString.mid(4);
}

QString QVWin32Functions::getLongPath(const QString &path)
{
    return getLongOrShortPath(path, GetLongPathNameW);
}

QString QVWin32Functions::getShortPath(const QString &path)
{
    return getLongOrShortPath(path, GetShortPathNameW);
}

bool QVWin32Functions::showInExplorer(const QString &path)
{
    bool result = false;
    LPITEMIDLIST pIdl;
    if (SUCCEEDED(SHParseDisplayName(reinterpret_cast<const wchar_t*>(path.utf16()), nullptr, &pIdl, 0, nullptr)))
    {
        result = SUCCEEDED(SHOpenFolderAndSelectItems(pIdl, 0, nullptr, 0));
        ILFree(pIdl);
    }
    return result;
}

QByteArray QVWin32Functions::getIccProfileForWindow(const QWindow *window)
{
    QByteArray result;
    const HWND hWnd = reinterpret_cast<HWND>(window->winId());
    const HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    if (hMonitor)
    {
        MONITORINFOEXW monitorInfo;
        monitorInfo.cbSize = sizeof(MONITORINFOEXW);
        if (GetMonitorInfoW(hMonitor, &monitorInfo))
        {
            const HDC hDC = CreateICW(monitorInfo.szDevice, monitorInfo.szDevice, NULL, NULL);
            if (hDC)
            {
                WCHAR profilePathBuff[MAX_PATH];
                DWORD profilePathSize = MAX_PATH;
                if (GetICMProfileW(hDC, &profilePathSize, profilePathBuff))
                {
                    QString profilePath = QString::fromWCharArray(profilePathBuff);
                    QFile file(profilePath);
                    if (file.open(QIODevice::ReadOnly))
                    {
                        result = file.readAll();
                        file.close();
                    }
                }
                DeleteDC(hDC);
            }
        }
    }
    return result;
}

QStringList QVWin32Functions::getExplorerSortOrder(const QString &folderPath)
{
    QStringList sortOrder;

    // Initialize COM - S_FALSE means already initialized on this thread
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool comInitialized = SUCCEEDED(hr);

    // Get IShellWindows
    IShellWindows *pShellWindows = nullptr;
    hr = CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_IShellWindows, (void**)&pShellWindows);
    if (FAILED(hr) || !pShellWindows)
    {
        if (comInitialized)
            CoUninitialize();
        return sortOrder;
    }

    // Get count of windows
    long count = 0;
    pShellWindows->get_Count(&count);

    // Normalize the target folder path for comparison
    const QString normalizedFolderPath = QDir::toNativeSeparators(folderPath).toLower();

    // Iterate through windows to find one showing our folder
    for (long i = 0; i < count; i++)
    {
        VARIANT vIndex;
        VariantInit(&vIndex);
        vIndex.vt = VT_I4;
        vIndex.lVal = i;

        IDispatch *pDispatch = nullptr;
        hr = pShellWindows->Item(vIndex, &pDispatch);
        VariantClear(&vIndex);

        if (FAILED(hr) || !pDispatch)
            continue;

        // Query for IWebBrowser2
        IWebBrowser2 *pBrowser = nullptr;
        hr = pDispatch->QueryInterface(IID_IWebBrowser2, (void**)&pBrowser);
        if (FAILED(hr) || !pBrowser)
        {
            pDispatch->Release();
            continue;
        }

        // Get the document
        IDispatch *pDocDispatch = nullptr;
        hr = pBrowser->get_Document(&pDocDispatch);
        if (FAILED(hr) || !pDocDispatch)
        {
            pBrowser->Release();
            pDispatch->Release();
            continue;
        }

        // Get the shell browser via IServiceProvider; the document does not expose IShellBrowser directly
        IServiceProvider *pServiceProvider = nullptr;
        hr = pDocDispatch->QueryInterface(IID_IServiceProvider, (void**)&pServiceProvider);
        IShellBrowser *pShellBrowser = nullptr;
        if (SUCCEEDED(hr) && pServiceProvider)
        {
            hr = pServiceProvider->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&pShellBrowser);
            pServiceProvider->Release();
        }
        if (SUCCEEDED(hr) && pShellBrowser)
        {
            // Get the current folder view from the active shell view; the top-level browser
            // itself does not expose IFolderView on Windows 11
            IShellView *pShellView = nullptr;
            hr = pShellBrowser->QueryActiveShellView(&pShellView);
            IFolderView *pFolderView = nullptr;
            if (SUCCEEDED(hr) && pShellView)
            {
                hr = pShellView->QueryInterface(IID_IFolderView, (void**)&pFolderView);
                pShellView->Release();
            }
            if (SUCCEEDED(hr) && pFolderView)
            {
                // Get the current folder
                IPersistFolder2 *pPersistFolder = nullptr;
                hr = pFolderView->GetFolder(IID_IPersistFolder2, (void**)&pPersistFolder);
                if (SUCCEEDED(hr) && pPersistFolder)
                {
                    LPITEMIDLIST pidl = nullptr;
                    hr = pPersistFolder->GetCurFolder(&pidl);
                    if (SUCCEEDED(hr) && pidl)
                    {
                        // Get the folder path
                        WCHAR folderPathBuf[MAX_PATH];
                        if (SHGetPathFromIDListW(pidl, folderPathBuf))
                        {
                            QString explorerPath = QString::fromWCharArray(folderPathBuf);
                            // Normalize paths for comparison
                            QString normalizedExplorerPath = QDir::toNativeSeparators(explorerPath).toLower();

                            if (normalizedExplorerPath == normalizedFolderPath)
                            {
                                // Found the matching window, get the sort order. IFolderView::Items()
                                // can report a stale order (items added while the view is open are
                                // appended until it re-sorts), so query items by index instead, which
                                // reflects the order actually shown in Explorer.
                                int itemCount = 0;
                                hr = pFolderView->ItemCount(SVGIO_ALLVIEW, &itemCount);
                                if (SUCCEEDED(hr))
                                {
                                    for (int index = 0; index < itemCount; index++)
                                    {
                                        LPITEMIDLIST pidlItem = nullptr;
                                        if (FAILED(pFolderView->Item(index, &pidlItem)) || !pidlItem)
                                            continue;

                                        //Combine with the folder PIDL since the view may return simple item PIDLs
                                        //which would otherwise be misresolved against the desktop namespace
                                        LPITEMIDLIST pidlFull = ILCombine(pidl, pidlItem);
                                        const LPITEMIDLIST pidlToResolve = pidlFull ? pidlFull : pidlItem;
                                        WCHAR filePath[MAX_PATH];
                                        if (SHGetPathFromIDListW(pidlToResolve, filePath))
                                        {
                                            sortOrder.append(QString::fromWCharArray(filePath));
                                        }
                                        if (pidlFull)
                                            CoTaskMemFree(pidlFull);
                                        CoTaskMemFree(pidlItem);
                                    }
                                }
                            }
                        }
                        CoTaskMemFree(pidl);
                    }
                    pPersistFolder->Release();
                }
                pFolderView->Release();
            }
            pShellBrowser->Release();
        }

        pDocDispatch->Release();
        pBrowser->Release();
        pDispatch->Release();

        if (!sortOrder.isEmpty())
            break;
    }

    pShellWindows->Release();
    if (comInitialized)
        CoUninitialize();

    return sortOrder;
}
