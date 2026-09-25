; MithenView Installer Script
; Uses NSIS (Nullsoft Scriptable Install System)
; Run from the repository root: makensis dist\scripts\installer.nsi

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

; Application info
Name "MithenView"
OutFile "..\..\res\MithenView-setup.exe"
; The application and the thumbnail provider are 64-bit, so install into the native
; Program Files. $PROGRAMFILES64 is correct even though the NSIS stub itself is 32-bit.
InstallDir "$PROGRAMFILES64\MithenView"
; Remember the chosen directory in HKCU: unlike HKLM, HKCU\Software is not subject to
; WOW64 redirection, so the value is read consistently. InstallDirRegKey is evaluated
; before .onInit, so an HKLM value recorded by an older installer would otherwise pin
; $INSTDIR to the legacy Program Files (x86) location before SetRegView 64 can apply.
; /D= still takes precedence over this default.
InstallDirRegKey HKCU "Software\mithen-view" "InstallDir"

; Request admin privileges
RequestExecutionLevel admin

; Version info
VIProductVersion "1.1.0.0"
VIAddVersionKey "ProductName" "MithenView"
VIAddVersionKey "FileDescription" "MithenView Image Viewer"
VIAddVersionKey "LegalCopyright" "Copyright 2026 MithenView contributors"
VIAddVersionKey "FileVersion" "1.1.0.0"
VIAddVersionKey "ProductVersion" "1.1.0.0"

; MUI settings
!define MUI_ABORTWARNING
!define MUI_ICON "..\win\mithen-view.ico"
!define MUI_UNICON "..\win\mithen-view.ico"

; Show every available language in the language selection dialog
!define MUI_LANGDLL_ALLLANGUAGES

; Welcome page
!insertmacro MUI_PAGE_WELCOME

; License page
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"

; Directory page
!insertmacro MUI_PAGE_DIRECTORY

; Instfiles page
!insertmacro MUI_PAGE_INSTFILES

; Finish page
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Languages. The language selected here is also the language installed for the application.
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "Japanese"
!insertmacro MUI_LANGUAGE "Korean"
!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "SimpChinese"

Function .onInit
    ; The application and thumbnail provider are 64-bit only
    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "MithenView requires 64-bit Windows."
        Abort
    ${EndIf}

    ; Use the native registry view so this 32-bit installer does not place the
    ; uninstall entry and install location under WOW6432Node
    SetRegView 64

    ; Optional language override for silent installs: /LANG=<code>, where code is
    ; one of en, de, es, fr, ja, ko, ru, zh. The language dialog is skipped when
    ; silent, so without this a silent install would only ever install English.
    ${GetParameters} $R0
    ${GetOptions} $R0 "/LANG=" $R1
    ${If} $R1 == "de"
        StrCpy $LANGUAGE ${LANG_GERMAN}
    ${ElseIf} $R1 == "es"
        StrCpy $LANGUAGE ${LANG_SPANISH}
    ${ElseIf} $R1 == "fr"
        StrCpy $LANGUAGE ${LANG_FRENCH}
    ${ElseIf} $R1 == "ja"
        StrCpy $LANGUAGE ${LANG_JAPANESE}
    ${ElseIf} $R1 == "ko"
        StrCpy $LANGUAGE ${LANG_KOREAN}
    ${ElseIf} $R1 == "ru"
        StrCpy $LANGUAGE ${LANG_RUSSIAN}
    ${ElseIf} $R1 == "zh"
        StrCpy $LANGUAGE ${LANG_SIMPCHINESE}
    ${EndIf}

    ; Let the user pick the installation language (skipped automatically when silent)
    !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Section "Install"
    ; A previous install made by an older installer sits in the 32-bit Program Files
    ; (Program Files (x86)) and recorded its location in the 32-bit registry view. The
    ; first version of this 64-bit-aware installer recorded the same legacy path in the
    ; native view. Remove that copy so upgrading does not leave two installs behind.
    SetRegView 32
    ReadRegStr $R9 HKLM "Software\mithen-view" "InstallDir"
    SetRegView 64
    ReadRegStr $R8 HKLM "Software\mithen-view" "InstallDir"
    ${If} $R9 == "$PROGRAMFILES32\MithenView"
    ${OrIf} $R8 == "$PROGRAMFILES32\MithenView"
        ; Only remove it when it really is an install (guards against a stale record)
        IfFileExists "$PROGRAMFILES32\MithenView\mithen-view.exe" 0 previousInstallGone
        IfFileExists "$PROGRAMFILES32\MithenView\uninstall.exe" 0 previousInstallGone
            RMDir /r "$PROGRAMFILES32\MithenView"
            RMDir /r "$SMPROGRAMS\MithenView"
        previousInstallGone:
        ; Drop the stale legacy registry records (the native view is written below)
        SetRegView 32
        DeleteRegKey HKLM "Software\mithen-view"
        DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView"
        SetRegView 64
    ${EndIf}

    SetOutPath "$INSTDIR"

    ; Copy all files from bin directory (includes Qt DLLs from windeployqt)
    File /r "..\..\bin\*.*"

    ; Copy license
    File "..\..\LICENSE"

    ; Register the Explorer thumbnail provider with the 64-bit regsvr32.
    ; RegDLL cannot be used here: this installer is 32-bit and cannot load the
    ; 64-bit DLL, so call the native regsvr32 through Sysnative (inside a 32-bit
    ; process System32 is redirected to SysWOW64, Sysnative reaches the real one).
    IfFileExists "$INSTDIR\MithenViewThumbnail.dll" 0 thumbnailRegistered
        nsExec::Exec '"$WINDIR\Sysnative\regsvr32.exe" /s "$INSTDIR\MithenViewThumbnail.dll"'
        Pop $0
    thumbnailRegistered:

    ; Application language: install only the catalog for the language chosen above
    ${If} $LANGUAGE == ${LANG_GERMAN}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\mithen-view_de.qm"
        File "..\win\translations\qtbase_de.qm"
    ${ElseIf} $LANGUAGE == ${LANG_SPANISH}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\mithen-view_es.qm"
        File "..\win\translations\qtbase_es.qm"
    ${ElseIf} $LANGUAGE == ${LANG_FRENCH}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\mithen-view_fr.qm"
        File "..\win\translations\qtbase_fr.qm"
    ${ElseIf} $LANGUAGE == ${LANG_JAPANESE}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\mithen-view_ja.qm"
        File "..\win\translations\qtbase_ja.qm"
    ${ElseIf} $LANGUAGE == ${LANG_KOREAN}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\mithen-view_ko.qm"
        File "..\win\translations\qtbase_ko.qm"
    ${ElseIf} $LANGUAGE == ${LANG_RUSSIAN}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\mithen-view_ru.qm"
        File "..\win\translations\qtbase_ru.qm"
    ${ElseIf} $LANGUAGE == ${LANG_SIMPCHINESE}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\mithen-view_zh_Hans.qm"
        File "..\win\translations\qtbase_zh_CN.qm"
    ${EndIf}
    SetOutPath "$INSTDIR"

    ; Create uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; Offer to replace existing settings (custom shortcuts, options, ...) with the new defaults.
    ; In silent installs the answer defaults to No (/SD IDNO) so an unattended install
    ; never wipes existing settings.
    EnumRegKey $0 HKCU "Software\mithen-view\mithen-view-JDP" 0
    StrCmp $0 "" checkOldSettings
        MessageBox MB_YESNO|MB_ICONQUESTION "Replace all existing MithenView settings with the new defaults?$\n$\nChoose No to keep your current settings and shortcuts." /SD IDNO IDNO checkOldSettings
        DeleteRegKey HKCU "Software\mithen-view\mithen-view-JDP"
    checkOldSettings:
    EnumRegKey $0 HKCU "Software\wView\wView-JDP" 0
    StrCmp $0 "" doneSettingsReset
        MessageBox MB_YESNO|MB_ICONQUESTION "Replace all existing wView settings with the new defaults?$\n$\nChoose No to keep your current settings and shortcuts." /SD IDNO IDNO doneSettingsReset
        DeleteRegKey HKCU "Software\wView\wView-JDP"
    doneSettingsReset:

    ; Create start menu shortcuts
    CreateDirectory "$SMPROGRAMS\MithenView"
    CreateShortCut "$SMPROGRAMS\MithenView\MithenView.lnk" "$INSTDIR\mithen-view.exe"
    CreateShortCut "$SMPROGRAMS\MithenView\Uninstall.lnk" "$INSTDIR\uninstall.exe"

    ; Write registry keys
    WriteRegStr HKCU "Software\mithen-view" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\mithen-view" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "DisplayName" "MithenView"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "DisplayVersion" "1.1.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "Publisher" "MithenView contributors"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "NoRepair" 1
SectionEnd

Section "Uninstall"
    ; Close a running instance so its executable and DLLs are not locked
    nsExec::Exec 'taskkill /IM mithen-view.exe'
    Pop $0
    Sleep 500
    nsExec::Exec 'taskkill /IM mithen-view.exe /F'
    Pop $0

    ; Unregister the Explorer thumbnail provider with the 64-bit regsvr32 before
    ; removing files (see the note in the Install section for why not UnRegDLL)
    IfFileExists "$INSTDIR\MithenViewThumbnail.dll" 0 thumbnailUnregistered
        nsExec::Exec '"$WINDIR\Sysnative\regsvr32.exe" /s /u "$INSTDIR\MithenViewThumbnail.dll"'
        Pop $0
    thumbnailUnregistered:

    ; Remove all installed files
    RMDir /r "$INSTDIR"

    ; Remove shortcuts
    RMDir /r "$SMPROGRAMS\MithenView"

    ; Remove registry keys (native view)
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView"
    DeleteRegKey HKLM "Software\mithen-view"

    ; Also remove any legacy 32-bit-view entries
    SetRegView 32
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView"
    DeleteRegKey HKLM "Software\mithen-view"
    SetRegView 64

    ; Always remove the user configuration, no prompt
    DeleteRegKey HKCU "Software\mithen-view"
    DeleteRegKey HKCU "Software\wView"
SectionEnd

Function un.onInit
    ; Keep the uninstaller in the same (native) registry view as the installer
    SetRegView 64
FunctionEnd
