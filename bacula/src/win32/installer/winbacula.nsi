; winbacula.nsi
;
; Began as a version written by Michel Meyers (michel@tcnnet.dyndns.org)
;
; Adapted by Kern Sibbald for native Win32 Bacula
;    added a number of elements from Christopher Hull's installer
;
; D. Scott Barninger Nov 13 2004
; added configuration editing for bconsole.conf and wx-console.conf
; better explanation in dialog boxes for editing config files
; added Start Menu items
; fix uninstall of config files to do all not just bacula-fd.conf
;
; D. Scott Barninger Dec 05 2004
; added specification of default permissions for bacula-fd.conf
;   - thanks to Jamie Ffolliott for pointing me at cacls
; added removal of working-dir files if user selects to remove config
; uninstall is now 100% clean
;
; D. Scott Barninger Apr 17 2005
; 1.36.3 release docs update
; add pdf manual and menu shortcut
;
; Robert Nelson May 15 2006
; Pretty much rewritten
; Use LogicLib.nsh
; Added Bacula-SD and Bacula-DIR
; Replaced ParameterGiven with standard GetOptions

;
; Command line options:
;
; /cygwin     -  do cygwin install into c:\cygwin\bacula
; /service    - 
; /start

!define PRODUCT "Bacula"

;
; Include the Modern UI
;
!include "MUI.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "Sections.nsh"
!include "StrFunc.nsh"

;
; Basics
;
  Name "Bacula"
  OutFile "winbacula-${VERSION}.exe"
  SetCompressor lzma
  InstallDir "$PROGRAMFILES\Bacula"
  InstallDirRegKey HKLM Software\Bacula InstallLocation

  InstType "Client"
  InstType "Server"
  InstType "Full"

  ${StrCase}
  ${StrRep}
  ${StrTrimNewLines}

;
; Pull in pages
;

!define      MUI_COMPONENTSPAGE_SMALLDESC
!define      MUI_FINISHPAGE_NOAUTOCLOSE

!InsertMacro MUI_PAGE_WELCOME
;  !InsertMacro MUI_PAGE_LICENSE "..\..\LICENSE"

!InsertMacro MUI_PAGE_COMPONENTS
!InsertMacro MUI_PAGE_DIRECTORY
Page custom EnterConfigPage1 LeaveConfigPage1
Page custom EnterConfigPage2 LeaveConfigPage2
!InsertMacro MUI_PAGE_INSTFILES
!InsertMacro MUI_PAGE_FINISH

!InsertMacro MUI_UNPAGE_WELCOME
!InsertMacro MUI_UNPAGE_CONFIRM
!InsertMacro MUI_UNPAGE_INSTFILES
!InsertMacro MUI_UNPAGE_FINISH

!define      MUI_ABORTWARNING

!InsertMacro MUI_LANGUAGE "English"

!InsertMacro GetParameters
!InsertMacro GetOptions

DirText "Setup will install Bacula ${VERSION} to the directory specified below. To install in a different folder, click Browse and select another folder."

!InsertMacro MUI_RESERVEFILE_INSTALLOPTIONS
;
; Global Variables
;
Var OptCygwin
Var OptService
Var OptStart
Var OptSilent

Var DependenciesDone
Var DatabaseDone

Var OsIsNT

Var HostName

Var ConfigClientName
Var ConfigClientPort
Var ConfigClientMaxJobs
Var ConfigClientPassword
Var ConfigClientInstallService
Var ConfigClientStartService

Var ConfigStorageName
Var ConfigStoragePort
Var ConfigStorageMaxJobs
Var ConfigStoragePassword
Var ConfigStorageInstallService
Var ConfigStorageStartService

Var ConfigDirectorName
Var ConfigDirectorPort
Var ConfigDirectorMaxJobs
Var ConfigDirectorPassword
Var ConfigDirectorAddress
Var ConfigDirectorMailServer
Var ConfigDirectorMailAddress
Var ConfigDirectorDB
Var ConfigDirectorInstallService
Var ConfigDirectorStartService

Var ConfigMonitorName
Var ConfigMonitorPassword

Var LocalDirectorPassword
Var LocalHostAddress

Var HDLG
Var HCTL

Function .onInit
  Push $R0
  Push $R1

  ; Process Command Line Options
  StrCpy $OptCygwin 0
  StrCpy $OptService 1
  StrCpy $OptStart 1
  StrCpy $OptSilent 0
  StrCpy $DependenciesDone 0
  StrCpy $DatabaseDone 0
  StrCpy $OsIsNT 0

  ${GetParameters} $R0

  ClearErrors
  ${GetOptions} $R0 "/cygwin" $R1
  IfErrors +2
    StrCpy $OptCygwin 1

  ClearErrors
  ${GetOptions} $R0 "/noservice" $R1
  IfErrors +2
    StrCpy $OptService 0

  ClearErrors
  ${GetOptions} $R0 "/nostart" $R1
  IfErrors +2
    StrCpy $OptStart 0

  IfSilent 0 +2
    StrCpy $OptSilent 1

  ${If} $OptCygwin = 1
    StrCpy $INSTDIR "C:\cygwin\bacula"
  ${EndIf}

  ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  ${If} $R0 != ""
    StrCpy $OsIsNT 1
  ${EndIf}

;  ${If} $OsIsNT = 0
;    Call DisableServerSections
;  ${EndIf}

  Call GetComputerName
  Pop $HostName

  Call GetHostName
  Pop $LocalHostAddress
  
  Call GetUserName
  Pop $ConfigDirectorMailAddress

  ; Configuration Defaults
  ;
  StrCpy $ConfigClientName              "$HostName-fd"
  StrCpy $ConfigClientPort              "9102"
  StrCpy $ConfigClientMaxJobs           "2"
  ;StrCpy $ConfigClientPassword
  StrCpy $ConfigClientInstallService    "$OptService"
  StrCpy $ConfigClientStartService      "$OptStart"

  StrCpy $ConfigStorageName             "$HostName-sd"
  StrCpy $ConfigStoragePort             "9103"
  StrCpy $ConfigStorageMaxJobs          "10"
  ;StrCpy $ConfigStoragePassword
  StrCpy $ConfigStorageInstallService   "$OptService"
  StrCpy $ConfigStorageStartService     "$OptStart"

  ;StrCpy $ConfigDirectorName            "$HostName-dir"
  StrCpy $ConfigDirectorPort            "9101"
  StrCpy $ConfigDirectorMaxJobs         "1"
  ;StrCpy $ConfigDirectorPassword
  StrCpy $ConfigDirectorDB              "1"
  StrCpy $ConfigDirectorInstallService  "$OptService"
  StrCpy $ConfigDirectorStartService    "$OptStart"

  StrCpy $ConfigMonitorName            "$HostName-mon"
  ;StrCpy $ConfigMonitorPassword

  InitPluginsDir
  File "/oname=$PLUGINSDIR\openssl.exe"  "${DEPKGS_BIN}\openssl.exe"
  File "/oname=$PLUGINSDIR\libeay32.dll" "${DEPKGS_BIN}\libeay32.dll"
  File "/oname=$PLUGINSDIR\ssleay32.dll" "${DEPKGS_BIN}\ssleay32.dll"
  File "/oname=$PLUGINSDIR\sed.exe" "${DEPKGS_BIN}\sed.exe"

  SetPluginUnload alwaysoff

  nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
  pop $R0
  ${If} $R0 = 0
   FileOpen $R1 "$PLUGINSDIR\pw.txt" r
   IfErrors +4
     FileRead $R1 $R0
     ${StrTrimNewLines} $ConfigClientPassword $R0
     FileClose $R1
  ${EndIf}

  nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
  pop $R0
  ${If} $R0 = 0
   FileOpen $R1 "$PLUGINSDIR\pw.txt" r
   IfErrors +4
     FileRead $R1 $R0
     ${StrTrimNewLines} $ConfigStoragePassword $R0
     FileClose $R1
  ${EndIf}

  nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
  pop $R0
  ${If} $R0 = 0
   FileOpen $R1 "$PLUGINSDIR\pw.txt" r
   IfErrors +4
     FileRead $R1 $R0
     ${StrTrimNewLines} $LocalDirectorPassword $R0
     FileClose $R1
  ${EndIf}

  SetPluginUnload manual

  nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
  pop $R0
  ${If} $R0 = 0
   FileOpen $R1 "$PLUGINSDIR\pw.txt" r
   IfErrors +4
     FileRead $R1 $R0
     ${StrTrimNewLines} $ConfigMonitorPassword $R0
     FileClose $R1
  ${EndIf}

  Pop $R1
  Pop $R0
FunctionEnd

Function CopyDependencies
  SetOutPath "$INSTDIR\bin"

  ${If} $DependenciesDone = 0
!if "${BUILD_TOOLS}" == "VC8"
    File "${VC_REDIST_DIR}\msvcm80.dll"
    File "${VC_REDIST_DIR}\msvcp80.dll"
    File "${VC_REDIST_DIR}\msvcr80.dll"
    File "${VC_REDIST_DIR}\Microsoft.VC80.CRT.manifest"
    File "${DEPKGS_BIN}\pthreadVCE.dll"
!endif
!if "${BUILD_TOOLS}" == "VC8_DEBUG"
    File "${VC_REDIST_DIR}\msvcm80d.dll"
    File "${VC_REDIST_DIR}\msvcp80d.dll"
    File "${VC_REDIST_DIR}\msvcr80d.dll"
    File "${VC_REDIST_DIR}\Microsoft.VC80.DebugCRT.manifest"
    File "${DEPKGS_BIN}\pthreadVCE.dll"
!endif
!if "${BUILD_TOOLS}" == "MinGW"
    File "${MINGW_BIN}\..\mingw32\bin\mingwm10.dll"
    File "${DEPKGS_BIN}\pthreadGCE.dll"
!endif
    File "${DEPKGS_BIN}\libeay32.dll"
    File "${DEPKGS_BIN}\ssleay32.dll"
    File "${DEPKGS_BIN}\zlib1.dll"
!if "${BUILD_TOOLS}" == "VC8"
    File "${DEPKGS_BIN}\zlib1.dll.manifest"
!endif
!If "${BUILD_TOOLS}" == "VC8_DEBUG"
    File "${DEPKGS_BIN}\zlib1.dll.manifest"
!endif
    File "${DEPKGS_BIN}\openssl.exe"
    File "${BACULA_BIN}\bacula.dll"
    StrCpy $DependenciesDone 1
  ${EndIf}
FunctionEnd

Function InstallDatabase
  SetOutPath "$INSTDIR\bin"

  ${If} $DatabaseDone = 0
    ${If} $ConfigDirectorDB = 1
      File /oname=bacula_cats.dll "${BACULA_BIN}\cats_mysql.dll"
      File "${DEPKGS_BIN}\libmysql.dll"
    ${ElseIf} $ConfigDirectorDB = 2
      File /oname=bacula_cats.dll "${BACULA_BIN}\cats_pgsql.dll"
      File "${DEPKGS_BIN}\libpq.dll"
!if "${BUILD_TOOLS}" == "VC8"
      File "${DEPKGS_BIN}\comerr32.dll"
      File "${DEPKGS_BIN}\libintl-2.dll"
      File "${DEPKGS_BIN}\libiconv-2.dll"
      File "${DEPKGS_BIN}\krb5_32.dll"
!endif
!If "${BUILD_TOOLS}" == "VC8_DEBUG"
      File "${DEPKGS_BIN}\comerr32.dll"
      File "${DEPKGS_BIN}\libintl-2.dll"
      File "${DEPKGS_BIN}\libiconv-2.dll"
      File "${DEPKGS_BIN}\krb5_32.dll"
!endif
    ${ElseIf} $ConfigDirectorDB = 4
      File /oname=bacula_cats.dll "${BACULA_BIN}\cats_bdb.dll"
    ${EndIf}

    StrCpy $DatabaseDone 1
  ${EndIf}
FunctionEnd

Section "-Initialize"
  ; Create Start Menu Directory

  WriteRegStr HKLM Software\Bacula InstallLocation "$INSTDIR"

  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\Bacula"

  CreateDirectory "$INSTDIR"
  CreateDirectory "$INSTDIR\bin"
  CreateDirectory "$APPDATA\Bacula"
  CreateDirectory "$APPDATA\Bacula\Work"
  CreateDirectory "$APPDATA\Bacula\Spool"

  File "..\..\..\LICENSE"
  Delete /REBOOTOK "$INSTDIR\bin\License.txt"

  FileOpen $R1 $PLUGINSDIR\config.sed w
  FileWrite $R1 "s;@VERSION@;${VERSION};$\r$\n"
  FileWrite $R1 "s;@DATE@;${__DATE__};$\r$\n"
  FileWrite $R1 "s;@DISTNAME@;Windows;$\r$\n"

!If "$BUILD_TOOLS" == "MinGW"
  StrCpy $R2 "MinGW32"
!Else
  StrCpy $R2 "MVS"
!EndIf

  Call GetHostName
  Exch $R3
  Pop $R3

  FileWrite $R1 "s;@DISTVER@;$R2;$\r$\n"

  ${StrRep} $R2 "$APPDATA\Bacula\Work" "\" "\\\\"
  FileWrite $R1 's;@working_dir@;$R2;$\r$\n'

  ${StrRep} $R2 "$INSTDIR\bin" "\" "\\\\"
  FileWrite $R1 's;@bin_dir@;$R2;$\r$\n'

  FileWrite $R1 's;@TAPEDRIVE@;Tape0;$\r$\n'

  Call IsDirectorSelected
  Pop $R2

  ${If} $R2 = 1
    FileWrite $R1 "s;@director_address@;$LocalHostAddress;$\r$\n"
  ${Else}
    ${If} "$ConfigDirectorAddress" != ""
      FileWrite $R1 "s;@director_address@;$ConfigDirectorAddress;$\r$\n"
    ${EndIf}
  ${EndIf}

  FileWrite $R1 "s;@client_address@;$LocalHostAddress;$\r$\n"
  FileWrite $R1 "s;@storage_address@;$LocalHostAddress;$\r$\n"

  ${If} "$ConfigClientName" != ""
    FileWrite $R1 "s;@client_name@;$ConfigClientName;$\r$\n"
  ${EndIf}
  ${If} "$ConfigClientPort" != ""
    FileWrite $R1 "s;@client_port@;$ConfigClientPort;$\r$\n"
  ${EndIf}
  ${If} "$ConfigClientMaxJobs" != ""
    FileWrite $R1 "s;@client_maxjobs@;$ConfigClientMaxJobs;$\r$\n"
  ${EndIf}
  ${If} "$ConfigClientPassword" != ""
    FileWrite $R1 "s;@client_password@;$ConfigClientPassword;$\r$\n"
  ${EndIf}
  ${If} "$ConfigStorageName" != ""
    FileWrite $R1 "s;@storage_name@;$ConfigStorageName;$\r$\n"
  ${EndIf}
  ${If} "$ConfigStoragePort" != ""
    FileWrite $R1 "s;@storage_port@;$ConfigStoragePort;$\r$\n"
  ${EndIf}
  ${If} "$ConfigStorageMaxJobs" != ""
    FileWrite $R1 "s;@storage_maxjobs@;$ConfigStorageMaxJobs;$\r$\n"
  ${EndIf}
  ${If} "$ConfigStoragePassword" != ""
    FileWrite $R1 "s;@storage_password@;$ConfigStoragePassword;$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorName" != ""
    FileWrite $R1 "s;@director_name@;$ConfigDirectorName;$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorPort" != ""
    FileWrite $R1 "s;@director_port@;$ConfigDirectorPort;$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorMaxJobs" != ""
    FileWrite $R1 "s;@director_maxjobs@;$ConfigDirectorMaxJobs;$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorPassword" != ""
    FileWrite $R1 "s;@director_password@;$ConfigDirectorPassword;$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorMailServer" != ""
    FileWrite $R1 "s;@smtp_host@;$ConfigDirectorMailServer;$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorMailAddress" != ""
    FileWrite $R1 "s;@job_email@;$ConfigDirectorMailAddress;$\r$\n"
  ${EndIf}
  ${If} "$ConfigMonitorName" != ""
    FileWrite $R1 "s;@monitor_name@;$ConfigMonitorName;$\r$\n"
  ${EndIf}
  ${If} "$ConfigMonitorPassword" != ""
    FileWrite $R1 "s;@monitor_password@;$ConfigMonitorPassword;$\r$\n"
  ${EndIf}

  FileClose $R1

SectionEnd

SectionGroup "Client"

Section "File Service" SecFileDaemon
  SectionIn 1 2 3

  SetOutPath "$INSTDIR\bin"

  File "${BACULA_BIN}\bacula-fd.exe"

  StrCpy $R0 0
  StrCpy $R1 "$APPDATA\Bacula\bacula-fd.conf"
  IfFileExists $R1 0 +3
    StrCpy $R0 1
    StrCpy $R1 "$R1.new"

  File "/oname=$PLUGINSDIR\bacula-fd.conf.in" "bacula-fd.conf.in"

  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i.bak "$PLUGINSDIR\bacula-fd.conf.in"'
  CopyFiles "$PLUGINSDIR\bacula-fd.conf.in" "$R1"

  ${If} $OsIsNT = 1
    nsExec::ExecToLog 'cmd.exe /C echo Y|cacls "$R1" /G SYSTEM:F Administrators:F'
  ${EndIf}

  StrCpy $0 bacula-fd
  StrCpy $1 "File Service"
  StrCpy $2 $ConfigClientInstallService
  StrCpy $3 $ConfigClientStartService
  
  Call InstallDaemon

  CreateShortCut "$SMPROGRAMS\Bacula\Edit Client Configuration.lnk" "write.exe" '"$APPDATA\Bacula\bacula-fd.conf"'
SectionEnd

SectionGroupEnd

SectionGroup "Server"

Section "Storage Service" SecStorageDaemon
  SectionIn 2 3

  SetOutPath "$INSTDIR\bin"

  File "${DEPKGS_BIN}\loaderinfo.exe"
  File "${DEPKGS_BIN}\mt.exe"
  File "${DEPKGS_BIN}\mtx.exe"
  File "${DEPKGS_BIN}\scsitape.exe"
  File "${DEPKGS_BIN}\tapeinfo.exe"
  File "${BACULA_BIN}\bacula-sd.exe"
  File "${BACULA_BIN}\bcopy.exe"
  File "${BACULA_BIN}\bextract.exe"
  File "${BACULA_BIN}\bls.exe"
  File "${BACULA_BIN}\bscan.exe"
  File "${BACULA_BIN}\btape.exe"
  File "${BACULA_BIN}\scsilist.exe"
  File /oname=mtx-changer.cmd ${SCRIPT_DIR}\mtx-changer.cmd

  StrCpy $R0 0
  StrCpy $R1 "$APPDATA\Bacula\bacula-sd.conf"
  IfFileExists $R1 0 +3
    StrCpy $R0 1
    StrCpy $R1 "$R1.new"

  File "/oname=$PLUGINSDIR\bacula-sd.conf.in" "bacula-sd.conf.in"

  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i.bak "$PLUGINSDIR\bacula-sd.conf.in"'
  CopyFiles "$PLUGINSDIR\bacula-sd.conf.in" "$R1"

  ${If} $OsIsNT = 1
    nsExec::ExecToLog 'cmd.exe /C echo Y|cacls "$R1" /G SYSTEM:F Administrators:F'
  ${EndIf}

  StrCpy $0 bacula-sd
  StrCpy $1 "Storage Service"
  StrCpy $2 $ConfigStorageInstallService
  StrCpy $3 $ConfigStorageStartService
  Call InstallDaemon

  CreateShortCut "$SMPROGRAMS\Bacula\List Devices.lnk" "$INSTDIR\bin\scsilist.exe" "/pause"
  CreateShortCut "$SMPROGRAMS\Bacula\Edit Storage Configuration.lnk" "write.exe" '"$APPDATA\Bacula\bacula-sd.conf"'
SectionEnd

Section "Director Service" SecDirectorDaemon
  SectionIn 2 3

  SetOutPath "$INSTDIR\bin"

  Call InstallDatabase
  File "${BACULA_BIN}\bacula-dir.exe"
  File "${BACULA_BIN}\dbcheck.exe"

  ${If} $ConfigDirectorDB = 1
    File /oname=create_database.cmd ${CATS_DIR}\create_mysql_database.cmd
    File /oname=drop_database.cmd ${CATS_DIR}\drop_mysql_database.cmd
    File /oname=make_tables.cmd ${CATS_DIR}\make_mysql_tables.cmd
    File ${CATS_DIR}\make_mysql_tables.sql
    File /oname=drop_tables.cmd ${CATS_DIR}\drop_mysql_tables.cmd
    File ${CATS_DIR}\drop_mysql_tables.sql
    File /oname=update_tables.cmd ${CATS_DIR}\update_mysql_tables.cmd
    File ${CATS_DIR}\update_mysql_tables.sql
    File /oname=grant_privileges.cmd ${CATS_DIR}\grant_mysql_privileges.cmd
    File ${CATS_DIR}\grant_mysql_privileges.sql
  ${ElseIf} $ConfigDirectorDB = 2
    File /oname=create_database.cmd ${CATS_DIR}\create_postgresql_database.cmd
    File /oname=drop_database.cmd ${CATS_DIR}\drop_postgresql_database.cmd
    File /oname=make_tables.cmd ${CATS_DIR}\make_postgresql_tables.cmd
    File ${CATS_DIR}\make_postgresql_tables.sql
    File /oname=drop_tables.cmd ${CATS_DIR}\drop_postgresql_tables.cmd
    File ${CATS_DIR}\drop_postgresql_tables.sql
    File /oname=update_tables.cmd ${CATS_DIR}\update_postgresql_tables.cmd
    File ${CATS_DIR}\update_postgresql_tables.sql
    File /oname=grant_privileges.cmd ${CATS_DIR}\grant_postgresql_privileges.cmd
    File ${CATS_DIR}\grant_postgresql_privileges.sql
  ${ElseIf} $ConfigDirectorDB = 3
    File /oname=create_database.cmd ${CATS_DIR}\create_bdb_database.cmd
    File /oname=drop_database.cmd ${CATS_DIR}\drop_bdb_database.cmd
    File /oname=make_tables.cmd ${CATS_DIR}\make_bdb_tables.cmd
    File /oname=drop_tables.cmd ${CATS_DIR}\drop_bdb_tables.cmd
    File /oname=update_tables.cmd ${CATS_DIR}\update_bdb_tables.cmd
    File /oname=grant_privileges.cmd ${CATS_DIR}\grant_bdb_privileges.cmd
  ${EndIf}
  File ${CATS_DIR}\make_catalog_backup.cmd
  File ${CATS_DIR}\delete_catalog_backup.cmd

  StrCpy $R0 0
  StrCpy $R1 "$APPDATA\Bacula\bacula-dir.conf"
  IfFileExists $R1 0 +3
    StrCpy $R0 1
    StrCpy $R1 "$R1.new"

  File "/oname=$PLUGINSDIR\bacula-dir.conf.in" "bacula-dir.conf.in"

  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i.bak "$PLUGINSDIR\bacula-dir.conf.in"'
  CopyFiles "$PLUGINSDIR\bacula-dir.conf.in" "$R1"

  ${If} $OsIsNT = 1
    nsExec::ExecToLog 'cmd.exe /C echo Y|cacls "$R1" /G SYSTEM:F Administrators:F'
  ${EndIf}

  StrCpy $0 bacula-dir
  StrCpy $1 "Director Service"
  StrCpy $2 $ConfigDirectorInstallService
  StrCpy $3 $ConfigDirectorStartService
  Call InstallDaemon

  CreateShortCut "$SMPROGRAMS\Bacula\Edit Director Configuration.lnk" "write.exe" '"$APPDATA\Bacula\bacula-dir.conf"'
SectionEnd

SectionGroupEnd

SectionGroup "Consoles"

Section "Command Console" SecConsole
  SectionIn 1 2 3

  SetOutPath "$INSTDIR\bin"

  File "${BACULA_BIN}\bconsole.exe"
  Call CopyDependencies

  StrCpy $R0 0
  StrCpy $R1 "$APPDATA\Bacula\bconsole.conf"
  IfFileExists $R1 0 +3
    StrCpy $R0 1
    StrCpy $R1 "$R1.new"

  File "/oname=$PLUGINSDIR\bconsole.conf.in" "bconsole.conf.in"

  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i.bak "$PLUGINSDIR\bconsole.conf.in"'
  CopyFiles "$PLUGINSDIR\bconsole.conf.in" "$R1"

  ${If} $OsIsNT = 1
    nsExec::ExecToLog 'cmd.exe /C echo Y|cacls "$R1" /G SYSTEM:F Administrators:F'
  ${EndIf}

  CreateShortCut "$SMPROGRAMS\Bacula\Edit Command Console Configuration.lnk" "write.exe" '"$APPDATA\Bacula\bconsole.conf"'

SectionEnd

Section "Graphical Console" SecWxConsole
  SectionIn 1 2 3
  
  SetOutPath "$INSTDIR\bin"

  Call CopyDependencies
!if "${BUILD_TOOLS}" == "VC8"
  File "${DEPKGS_BIN}\wxbase270_vc_bacula.dll"
  File "${DEPKGS_BIN}\wxmsw270_core_vc_bacula.dll"
!endif
!If "${BUILD_TOOLS}" == "VC8_DEBUG"
  File "${DEPKGS_BIN}\wxbase270_vc_bacula.dll"
  File "${DEPKGS_BIN}\wxmsw270_core_vc_bacula.dll"
!endif
!if "${BUILD_TOOLS}" == "MinGW"
  File "${DEPKGS_BIN}\wxbase26_gcc_bacula.dll"
  File "${DEPKGS_BIN}\wxmsw26_core_gcc_bacula.dll"
!endif

  File "${BACULA_BIN}\wx-console.exe"

  StrCpy $R0 0
  StrCpy $R1 "$APPDATA\Bacula\wx-console.conf"
  IfFileExists $R1 0 +3
    StrCpy $R0 1
    StrCpy $R1 "$R1.new"

  File "/oname=$PLUGINSDIR\wx-console.conf.in" "wx-console.conf.in"

  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i.bak "$PLUGINSDIR\wx-console.conf.in"'
  CopyFiles "$PLUGINSDIR\wx-console.conf.in" "$R1"

  ${If} $OsIsNT = 1
    nsExec::ExecToLog 'cmd.exe /C echo Y|cacls "$R1" /G SYSTEM:F Administrators:F'
  ${EndIf}

  ; Create Start Menu entry
  CreateShortCut "$SMPROGRAMS\Bacula\Console.lnk" "$INSTDIR\bin\wx-console.exe" '-c "$APPDATA\Bacula\wx-console.conf"' "$INSTDIR\bin\wx-console.exe" 0
  CreateShortCut "$SMPROGRAMS\Bacula\Edit Graphical Console Configuration.lnk" "write.exe" '"$APPDATA\Bacula\wx-console.conf"'
SectionEnd

SectionGroupEnd

SectionGroup "Documentation"

Section "Documentation (Acrobat Format)" SecDocPdf
  SectionIn 1 2 3

  SetOutPath "$INSTDIR\doc"
  CreateDirectory "$INSTDIR\doc"

  File "${DOC_DIR}\manual\bacula.pdf"
  CreateShortCut "$SMPROGRAMS\Bacula\Manual.lnk" '"$INSTDIR\doc\bacula.pdf"'
SectionEnd

Section "Documentation (HTML Format)" SecDocHtml
  SectionIn 3

  SetOutPath "$INSTDIR\doc"
  CreateDirectory "$INSTDIR\doc"

  File "${DOC_DIR}\manual\bacula\*.html"
  File "${DOC_DIR}\manual\bacula\*.png"
  File "${DOC_DIR}\manual\bacula\*.css"
  CreateShortCut "$SMPROGRAMS\Bacula\Manual (HTML).lnk" '"$INSTDIR\doc\bacula.html"'
SectionEnd

SectionGroupEnd

Section "-Write Uninstaller"
  ; Write the uninstall keys for Windows & create Start Menu entry
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "DisplayName" "Bacula"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortCut "$SMPROGRAMS\Bacula\Uninstall Bacula.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
SectionEnd

;
; Extra Page descriptions
;

  LangString DESC_SecFileDaemon ${LANG_ENGLISH} "Install Bacula File Daemon on this system."
  LangString DESC_SecStorageDaemon ${LANG_ENGLISH} "Install Bacula Storage Daemon on this system."
  LangString DESC_SecDirectorDaemon ${LANG_ENGLISH} "Install Bacula Director Daemon on this system."
  LangString DESC_SecConsole ${LANG_ENGLISH} "Install command console program on this system."
  LangString DESC_SecWxConsole ${LANG_ENGLISH} "Install graphical console program on this system."
  LangString DESC_SecDocPdf ${LANG_ENGLISH} "Install documentation in Acrobat format on this system."
  LangString DESC_SecDocHtml ${LANG_ENGLISH} "Install documentation in HTML format on this system."

  LangString TITLE_ConfigPage1 ${LANG_ENGLISH} "Configuration"
  LangString SUBTITLE_ConfigPage1 ${LANG_ENGLISH} "Set installation configuration."

  LangString TITLE_ConfigPage2 ${LANG_ENGLISH} "Configuration (continued)"
  LangString SUBTITLE_ConfigPage2 ${LANG_ENGLISH} "Set installation configuration."

  !InsertMacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !InsertMacro MUI_DESCRIPTION_TEXT ${SecFileDaemon} $(DESC_SecFileDaemon)
    !InsertMacro MUI_DESCRIPTION_TEXT ${SecStorageDaemon} $(DESC_SecStorageDaemon)
    !InsertMacro MUI_DESCRIPTION_TEXT ${SecDirectorDaemon} $(DESC_SecDirectorDaemon)
    !InsertMacro MUI_DESCRIPTION_TEXT ${SecConsole} $(DESC_SecConsole)
    !InsertMacro MUI_DESCRIPTION_TEXT ${SecWxConsole} $(DESC_SecWxConsole)
    !InsertMacro MUI_DESCRIPTION_TEXT ${SecDocPdf} $(DESC_SecDocPdf)
    !InsertMacro MUI_DESCRIPTION_TEXT ${SecDocHtml} $(DESC_SecDocHtml)
  !InsertMacro MUI_FUNCTION_DESCRIPTION_END

; Uninstall section

UninstallText "This will uninstall Bacula. Hit next to continue."

Section "Uninstall"
  ; Shutdown any baculum that could be running
  nsExec::ExecToLog '"$INSTDIR\bin\bacula-fd.exe" /kill'
  nsExec::ExecToLog '"$INSTDIR\bin\bacula-sd.exe" /kill'
  nsExec::ExecToLog '"$INSTDIR\bin\bacula-dir.exe" /kill'
  Sleep 3000


  ReadRegDWORD $R0 HKLM "Software\Bacula" "Service_Bacula-fd"
  ${If} $R0 = 1
    ; Remove bacula service
    nsExec::ExecToLog '"$INSTDIR\bin\bacula-fd.exe" /remove'
  ${EndIf}
  
  ReadRegDWORD $R0 HKLM "Software\Bacula" "Service_Bacula-sd"
  ${If} $R0 = 1
    ; Remove bacula service
    nsExec::ExecToLog '"$INSTDIR\bin\bacula-sd.exe" /remove'
  ${EndIf}
  
  ReadRegDWORD $R0 HKLM "Software\Bacula" "Service_Bacula-dir"
  ${If} $R0 = 1
    ; Remove bacula service
    nsExec::ExecToLog '"$INSTDIR\bin\bacula-dir.exe" /remove'
  ${EndIf}
  
  ; remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula"
  DeleteRegKey HKLM "Software\Bacula"

  ; remove start menu items
  SetShellVarContext all
  Delete /REBOOTOK "$SMPROGRAMS\Bacula\*"
  RMDir "$SMPROGRAMS\Bacula"

  ; remove files and uninstaller (preserving config for now)
  Delete /REBOOTOK "$INSTDIR\bin\*.*"
  Delete /REBOOTOK "$INSTDIR\doc\*.*"
  Delete /REBOOTOK "$INSTDIR\Uninstall.exe"

  ; Check for existing installation
  MessageBox MB_YESNO|MB_ICONQUESTION \
  "Would you like to delete the current configuration files and the working state file?" IDNO +3
    Delete /REBOOTOK "$APPDATA\Bacula\*"
    RMDir "$APPDATA\Bacula"

  ; remove directories used
  RMDir "$INSTDIR\bin"
  RMDir "$INSTDIR\doc"
  RMDir "$INSTDIR"
SectionEnd

;
; $0 - Service Name (ie Bacula-FD)
; $1 - Service Description (ie Bacula File Daemon)
; $2 - Install as Service
; $3 - Start Service now
;
Function InstallDaemon
  Call CopyDependencies

  IfFileExists "$APPDATA\Bacula\$0.conf" 0 +4
    nsExec::ExecToLog '"$INSTDIR\bin\$0.exe" /silent /kill'     ; Shutdown any bacula that could be running
    nsExec::ExecToLog '"$INSTDIR\bin\$0.exe" /silent /remove'   ; Remove existing service
    Sleep 3000

  WriteRegDWORD HKLM "Software\Bacula" "Service_$0" $2
  
  ${If} $2 = 1
    nsExec::ExecToLog '"$INSTDIR\bin\$0.exe" /silent /install -c "$APPDATA\Bacula\$0.conf"'

    ${If} $OsIsNT <> 1
      File "Start.bat"
      File "Stop.bat"
    ${EndIf}

    ; Start the service? (default skipped if silent, use /start to force starting)

    ${If} $3 = 1  
      ${If} $OsIsNT = 1
        nsExec::ExecToLog 'net start $0'
      ${Else}
        Exec '"$INSTDIR\bin\$0.exe" -c "$APPDATA\Bacula\$0.conf"'
      ${EndIf}
    ${EndIf}
  ${Else}
    CreateShortCut "$SMPROGRAMS\Bacula\Start $1.lnk" "$INSTDIR\bin\$0.exe" '-c "$APPDATA\Bacula\$0.conf"' "$INSTDIR\bin\$0.exe" 0
  ${EndIf}
FunctionEnd

Function GetComputerName
  Push $0
  Push $1
  Push $2

  System::Call "kernel32::GetComputerNameA(t .r0, *i ${NSIS_MAX_STRLEN} r1) i.r2"

  StrCpy $2 $0
  ${StrCase} $0 $2 "L"

  Pop $2
  Pop $1
  Exch $0
FunctionEnd

!define ComputerNameDnsFullyQualified   3

Function GetHostName
  Push $0
  Push $1
  Push $2

  ${If} $OsIsNT = 1
    System::Call "kernel32::GetComputerNameExA(i ${ComputerNameDnsFullyQualified}, t .r0, *i ${NSIS_MAX_STRLEN} r1) i.r2 ?e"
    ${If} $2 = 0
      Pop $2
      DetailPrint "GetComputerNameExA failed - LastError = $2"
      Call GetComputerName
      Pop $0
    ${Else}
      Pop $2
    ${EndIf}
  ${Else}
    Call GetComputerName
    Pop $0
  ${EndIf}

  Pop $2
  Pop $1
  Exch $0
FunctionEnd

!define NameUserPrincipal 8

Function GetUserName
  Push $0
  Push $1
  Push $2

  ${If} $OsIsNT = 1
    System::Call "secur32::GetUserNameExA(i ${NameUserPrincipal}, t .r0, *i ${NSIS_MAX_STRLEN} r1) i.r2 ?e"
    ${If} $2 = 0
      Pop $2
      DetailPrint "GetUserNameExA failed - LastError = $2"
      Pop $0
      StrCpy $0 ""
    ${Else}
      Pop $2
    ${EndIf}
  ${Else}
      StrCpy $0 ""
  ${EndIf}

  ${If} $0 == ""
    System::Call "advapi32::GetUserNameA(t .r0, *i ${NSIS_MAX_STRLEN} r1) i.r2 ?e"
    ${If} $2 = 0
      Pop $2
      DetailPrint "GetUserNameA failed - LastError = $2"
      StrCpy $0 ""
    ${Else}
      Pop $2
    ${EndIf}
  ${EndIf}

  Pop $2
  Pop $1
  Exch $0
FunctionEnd

Function IsClientSelected
  Push $0
  SectionGetFlags ${SecFileDaemon} $0
  IntOp $0 $0 & ${SF_SELECTED}
  Exch $0
FunctionEnd

Function IsDirectorSelected
  Push $0
  SectionGetFlags ${SecDirectorDaemon} $0
  IntOp $0 $0 & ${SF_SELECTED}
  Exch $0
FunctionEnd

Function IsStorageSelected
  Push $0
  SectionGetFlags ${SecStorageDaemon} $0
  IntOp $R0 $R0 & ${SF_SELECTED}
  Exch $0
FunctionEnd

Function IsConsoleSelected
  Push $0
  Push $1
  SectionGetFlags ${SecConsole} $0
  SectionGetFlags ${SecWxConsole} $1
  IntOp $0 $0 | $1
  IntOp $0 $0 & ${SF_SELECTED}
  Pop $1
  Exch $0
FunctionEnd

!If 0 = 1
Function DisableServerSections
  !InsertMacro UnselectSection ${SecStorageDaemon}
  !InsertMacro SetSectionFlag ${SecStorageDaemon} SF_RO
  !InsertMacro UnselectSection ${SecDirectorDaemon}
  !InsertMacro SetSectionFlag ${SecDirectorDaemon} SF_RO
FunctionEnd
!EndIf

!include "ConfigPage1.nsh"
!include "ConfigPage2.nsh"
