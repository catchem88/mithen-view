; wView Installer Script
; Uses NSIS (Nullsoft Scriptable Install System)
; Run from the repository root: makensis dist\scripts\installer.nsi

!include "MUI2.nsh"
!include "LogicLib.nsh"

; Application info
Name "wView"
OutFile "..\..\res\wView-setup.exe"
InstallDir "$PROGRAMFILES\wView"
InstallDirRegKey HKLM "Software\wView" "InstallDir"

; Request admin privileges
RequestExecutionLevel admin

; Version info
VIProductVersion "1.1.0.0"
VIAddVersionKey "ProductName" "wView"
VIAddVersionKey "FileDescription" "wView Image Viewer"
VIAddVersionKey "LegalCopyright" "Copyright 2026 wView contributors"
VIAddVersionKey "FileVersion" "1.1.0.0"
VIAddVersionKey "ProductVersion" "1.1.0.0"

; MUI settings
!define MUI_ABORTWARNING
!define MUI_ICON "..\win\wView.ico"
!define MUI_UNICON "..\win\wView.ico"

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
    IfFileExists "$INSTDIR\wViewThumbnail.dll" 0 +2
    RegDLL "$INSTDIR\wViewThumbnail.dll"

    ; Application language: install only the catalog for the language chosen above
    ${If} $LANGUAGE == ${LANG_GERMAN}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\wView_de.qm"
        File "..\win\translations\qtbase_de.qm"
    ${ElseIf} $LANGUAGE == ${LANG_SPANISH}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\wView_es.qm"
        File "..\win\translations\qtbase_es.qm"
    ${ElseIf} $LANGUAGE == ${LANG_FRENCH}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\wView_fr.qm"
        File "..\win\translations\qtbase_fr.qm"
    ${ElseIf} $LANGUAGE == ${LANG_JAPANESE}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\wView_ja.qm"
        File "..\win\translations\qtbase_ja.qm"
    ${ElseIf} $LANGUAGE == ${LANG_KOREAN}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\wView_ko.qm"
        File "..\win\translations\qtbase_ko.qm"
    ${ElseIf} $LANGUAGE == ${LANG_RUSSIAN}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\wView_ru.qm"
        File "..\win\translations\qtbase_ru.qm"
    ${ElseIf} $LANGUAGE == ${LANG_SIMPCHINESE}
        SetOutPath "$INSTDIR\translations"
        File "..\win\translations\wView_zh_Hans.qm"
        File "..\win\translations\qtbase_zh_CN.qm"
    ${EndIf}
    SetOutPath "$INSTDIR"

    ; Create uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; Offer to replace existing settings (custom shortcuts, options, ...) with the new defaults
    EnumRegKey $0 HKCU "Software\wView\wView-JDP" 0
    StrCmp $0 "" doneSettingsReset
        MessageBox MB_YESNO|MB_ICONQUESTION "Replace all existing wView settings with the new defaults?$\n$\nChoose No to keep your current settings and shortcuts." IDNO doneSettingsReset
        DeleteRegKey HKCU "Software\wView\wView-JDP"
    doneSettingsReset:

    ; Create start menu shortcuts
    CreateDirectory "$SMPROGRAMS\wView"
    CreateShortCut "$SMPROGRAMS\wView\wView.lnk" "$INSTDIR\wView.exe"
    CreateShortCut "$SMPROGRAMS\wView\Uninstall.lnk" "$INSTDIR\uninstall.exe"

    ; Write registry keys
    WriteRegStr HKLM "Software\wView" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\wView" "DisplayName" "wView"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\wView" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\wView" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\wView" "DisplayVersion" "1.1.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\wView" "Publisher" "wView contributors"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\wView" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\wView" "NoRepair" 1
SectionEnd

Section "Uninstall"
    ; Unregister the Explorer thumbnail provider before removing files
    IfFileExists "$INSTDIR\wViewThumbnail.dll" 0 +2
    UnRegDLL "$INSTDIR\wViewThumbnail.dll"

    ; Remove all installed files
    RMDir /r "$INSTDIR"

    ; Remove shortcuts
    RMDir /r "$SMPROGRAMS\wView"

    ; Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\wView"
    DeleteRegKey HKLM "Software\wView"
SectionEnd
