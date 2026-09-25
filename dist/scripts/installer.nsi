; MithenView Installer Script
; Uses NSIS (Nullsoft Scriptable Install System)
; Run from the repository root: makensis dist\scripts\installer.nsi

!include "MUI2.nsh"
!include "LogicLib.nsh"

; Application info
Name "MithenView"
OutFile "..\..\res\MithenView-setup.exe"
InstallDir "$PROGRAMFILES\MithenView"
InstallDirRegKey HKLM "Software\mithen-view" "InstallDir"

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
    ; Let the user pick the installation language
    !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Section "Install"
    SetOutPath "$INSTDIR"

    ; Copy all files from bin directory (includes Qt DLLs from windeployqt)
    File /r "..\..\bin\*.*"

    ; Copy license
    File "..\..\LICENSE"

    ; Register the Explorer thumbnail provider (machine-wide; falls back to per-user if denied)
    IfFileExists "$INSTDIR\MithenViewThumbnail.dll" 0 +2
    RegDLL "$INSTDIR\MithenViewThumbnail.dll"

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

    ; Offer to replace existing settings (custom shortcuts, options, ...) with the new defaults
    EnumRegKey $0 HKCU "Software\mithen-view\mithen-view-JDP" 0
    StrCmp $0 "" checkOldSettings
        MessageBox MB_YESNO|MB_ICONQUESTION "Replace all existing MithenView settings with the new defaults?$\n$\nChoose No to keep your current settings and shortcuts." IDNO checkOldSettings
        DeleteRegKey HKCU "Software\mithen-view\mithen-view-JDP"
    checkOldSettings:
    EnumRegKey $0 HKCU "Software\wView\wView-JDP" 0
    StrCmp $0 "" doneSettingsReset
        MessageBox MB_YESNO|MB_ICONQUESTION "Replace all existing wView settings with the new defaults?$\n$\nChoose No to keep your current settings and shortcuts." IDNO doneSettingsReset
        DeleteRegKey HKCU "Software\wView\wView-JDP"
    doneSettingsReset:

    ; Create start menu shortcuts
    CreateDirectory "$SMPROGRAMS\MithenView"
    CreateShortCut "$SMPROGRAMS\MithenView\MithenView.lnk" "$INSTDIR\mithen-view.exe"
    CreateShortCut "$SMPROGRAMS\MithenView\Uninstall.lnk" "$INSTDIR\uninstall.exe"

    ; Write registry keys
    WriteRegStr HKLM "Software\mithen-view" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "DisplayName" "MithenView"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "DisplayVersion" "1.1.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "Publisher" "MithenView contributors"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView" "NoRepair" 1
SectionEnd

Section "Uninstall"
    ; Unregister the Explorer thumbnail provider before removing files
    IfFileExists "$INSTDIR\MithenViewThumbnail.dll" 0 +2
    UnRegDLL "$INSTDIR\MithenViewThumbnail.dll"

    ; Remove all installed files
    RMDir /r "$INSTDIR"

    ; Remove shortcuts
    RMDir /r "$SMPROGRAMS\MithenView"

    ; Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MithenView"
    DeleteRegKey HKLM "Software\mithen-view"
SectionEnd
