; installer.nsi
!ifndef VERSION
  !define VERSION "0.0.0"
!endif
!ifndef SRCDIR
  !define SRCDIR "dist"
!endif

!include "MUI2.nsh"

Name "RLMM Toolkit"
OutFile "RLMMToolkit-${VERSION}-setup.exe"
InstallDir "$PROGRAMFILES64\RLMMToolkit"
InstallDirRegKey HKLM "Software\RLMMToolkit" "InstallDir"
RequestExecutionLevel admin
Unicode true

!define MUI_ABORTWARNING

!define MUI_FINISHPAGE_RUN "$INSTDIR\RLMMToolkit.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Run RLMM Toolkit"

!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Create desktop shortcut"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION CreateDesktopShortcut

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\RLMMToolkit"

Function CreateDesktopShortcut
  CreateShortcut "$DESKTOP\RLMM Toolkit.lnk" "$INSTDIR\RLMMToolkit.exe"
FunctionEnd

Section "Install"
  SetOutPath "$INSTDIR"
  File /r "${SRCDIR}\*.*"

  CreateDirectory "$SMPROGRAMS\RLMMToolkit"
  CreateShortcut  "$SMPROGRAMS\RLMMToolkit\RLMM Toolkit.lnk" "$INSTDIR\RLMMToolkit.exe"
  CreateShortcut  "$SMPROGRAMS\RLMMToolkit\Uninstall.lnk"    "$INSTDIR\uninstall.exe"

  WriteUninstaller "$INSTDIR\uninstall.exe"

  WriteRegStr HKLM "Software\RLMMToolkit" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName"     "RLMM Toolkit"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion"  "${VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  ; Stop the updater if it's running so RMDir /r doesn't fail on a locked exe
  nsExec::Exec 'taskkill /F /IM RLMMUpdater.exe'
  nsExec::Exec 'taskkill /F /IM RLMMToolkit.exe'

  Delete "$DESKTOP\RLMM Toolkit.lnk"
  Delete "$SMPROGRAMS\RLMMToolkit\RLMM Toolkit.lnk"
  Delete "$SMPROGRAMS\RLMMToolkit\Uninstall.lnk"
  RMDir  "$SMPROGRAMS\RLMMToolkit"

  RMDir /r "$INSTDIR"

  DeleteRegKey HKLM "${UNINST_KEY}"
  DeleteRegKey HKLM "Software\RLMMToolkit"
SectionEnd