Function EnterConfigPage2
  Call IsClientSelected
  Pop $R0

  Call IsStorageSelected
  Pop $R1

  Call IsDirectorSelected
  Pop $R2

  Call IsConsoleSelected
  Pop $R3

  ${If} $R0 = 0
  ${AndIf} $R1 = 0
  ${AndIf} $R2 = 0
  ${AndIf} $R3 = 0
    Abort
  ${EndIf}
  
  FileOpen $R5 "$PLUGINSDIR\ConfigPage2.ini" w

  StrCpy $R6 1  ; Field Number
  StrCpy $R7 0  ; Top
  
  ${If} $R2 = 1
    IntOp $R8 $R7 + 92
  FileWrite $R5 '[Field $R6]$\r$\nType="GroupBox"$\r$\nText="Director"$\r$\nLeft=0$\r$\nTop=$R7$\r$\nRight=300$\r$\nBottom=$R8$\r$\n$\r$\n'
  ${Else}
    ${If} $R3 = 1
      IntOp $R8 $R7 + 54
    ${Else}
      IntOp $R8 $R7 + 26
    ${EndIf}
    FileWrite $R5 '[Field $R6]$\r$\nType="GroupBox"$\r$\nText="Remote Director"$\r$\nLeft=0$\r$\nTop=$R7$\r$\nRight=300$\r$\nBottom=$R8$\r$\n$\r$\n'
  ${EndIf}

  IntOp $R6 $R6 + 1
  IntOp $R7 $R7 + 12

  IntOp $R8 $R7 + 8
  FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Name"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=26$\r$\nBottom=$R8$\r$\n$\r$\n'
  IntOp $R6 $R6 + 1
  IntOp $R7 $R7 - 2

  ${If} $R2 = 1
    ${If} "$ConfigDirectorName" == ""
      StrCpy $ConfigDirectorName "$HostName-dir"
    ${EndIf}
    ${If} "$ConfigDirectorPassword" == ""
      StrCpy $ConfigDirectorPassword "$LocalDirectorPassword"
    ${EndIf}
  ${Else}
    ${If} "$ConfigDirectorName" == "$HostName-dir"
      StrCpy $ConfigDirectorName ""
    ${EndIf}
  ${EndIf}
  
  IntOp $R8 $R8 + 2
  FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigDirectorName$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=158$\r$\nBottom=$R8$\r$\n$\r$\n'
  IntOp $R6 $R6 + 1

  ${If} $R2 = 1
  ${OrIf} $R3 = 1
    IntOp $R7 $R7 + 2
    IntOp $R8 $R8 - 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Port"$\r$\nLeft=172$\r$\nTop=$R7$\r$\nRight=188$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2

    IntOp $R8 $R8 + 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nFlags="ONLY_NUMBERS"$\r$\nState=$ConfigDirectorPort$\r$\nLeft=190$\r$\nTop=$R7$\r$\nRight=218$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
  ${EndIf}

  ${If} $R2 = 1
    IntOp $R7 $R7 + 2
    IntOp $R8 $R8 - 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Max Jobs"$\r$\nLeft=238$\r$\nTop=$R7$\r$\nRight=270$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2

    IntOp $R8 $R8 + 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nFlags="ONLY_NUMBERS"$\r$\nState=$ConfigDirectorMaxJobs$\r$\nLeft=274$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
  ${EndIf}

  IntOp $R7 $R7 + 14

  ${If} $R2 = 1
  ${OrIf} $R3 = 1
    IntOp $R7 $R7 + 2
    IntOp $R8 $R7 + 8

    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Password"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=38$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2
    IntOp $R8 $R8 + 2

    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigDirectorPassword$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 14
  ${EndIf}

  ${If} $R2 = 1
    IntOp $R7 $R7 + 2
    IntOp $R8 $R7 + 8

    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Mail Server"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=48$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2
    IntOp $R8 $R8 + 2

    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigDirectorMailServer$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 16
    IntOp $R8 $R7 + 8

    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Mail Address"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=48$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2
    IntOp $R8 $R8 + 2

    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigDirectorMailAddress$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 16
    IntOp $R8 $R7 + 8

    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Database"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=38$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2
    IntOp $R8 $R8 + 2

    ${If} $ConfigDirectorDB = 1
      StrCpy $R9 1
    ${Else}
      StrCpy $R9 0
    ${EndIf}

    FileWrite $R5 '[Field $R6]$\r$\nType="RadioButton"$\r$\nState=$R9$\r$\nText="MySQL"$\r$\nFlags="GROUP"$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=90$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1

    ${If} $ConfigDirectorDB = 2
      StrCpy $R9 1
    ${Else}
      StrCpy $R9 0
    ${EndIf}

    FileWrite $R5 '[Field $R6]$\r$\nType="RadioButton"$\r$\nState=$R9$\r$\nText="PostgreSQL"$\r$\nFlags="NOTABSTOP"$\r$\nLeft=94$\r$\nTop=$R7$\r$\nRight=146$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1

    ${If} $ConfigDirectorDB = 3
      StrCpy $R9 1
    ${Else}
      StrCpy $R9 0
    ${EndIf}

    FileWrite $R5 '[Field $R6]$\r$\nType="RadioButton"$\r$\nState=$R9$\r$\nText="Sqlite"$\r$\nFlags="NOTABSTOP"$\r$\nLeft=150$\r$\nTop=$R7$\r$\nRight=182$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1

    ${If} $ConfigDirectorDB = 4
      StrCpy $R9 1
    ${Else}
      StrCpy $R9 0
    ${EndIf}

    FileWrite $R5 '[Field $R6]$\r$\nType="RadioButton"$\r$\nState=$R9$\r$\nText="Builtin"$\r$\nFlags="NOTABSTOP"$\r$\nLeft=186$\r$\nTop=$R7$\r$\nRight=222$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 12
    IntOp $R8 $R7 + 10

    FileWrite $R5 '[Field $R6]$\r$\nType="Checkbox"$\r$\nState=$ConfigDirectorInstallService$\r$\nText="Install as service"$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=118$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1

    FileWrite $R5 '[Field $R6]$\r$\nType="Checkbox"$\r$\nState=$ConfigDirectorStartService$\r$\nText="Start after install"$\r$\nLeft=190$\r$\nTop=$R7$\r$\nRight=260$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 12
  ${ElseIf} $R3 = 1
    IntOp $R7 $R7 + 2
    IntOp $R8 $R7 + 8

    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Address"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=48$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2
    IntOp $R8 $R8 + 2

    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigDirectorAddress$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 14
    IntOp $R8 $R7 + 8
  ${EndIf}

  IntOp $R7 $R7 + 4
  
  ${If} $R0 = 1
    ${OrIf} $R1 = 1
    ${OrIf} $R2 = 1

    IntOp $R8 $R7 + 42

    FileWrite $R5 '[Field $R6]$\r$\nType="GroupBox"$\r$\nText="Monitor"$\r$\nLeft=0$\r$\nTop=$R7$\r$\nRight=300$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 12

    IntOp $R8 $R7 + 8
    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Name"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=26$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2

    IntOp $R8 $R8 + 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigMonitorName$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=150$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 16
    IntOp $R8 $R7 + 8

    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Password"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=38$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2
    IntOp $R8 $R8 + 2

    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigMonitorPassword$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 20
  ${EndIf}

  IntOp $R6 $R6 - 1
  FileWrite $R5 "[Settings]$\r$\nNumFields=$R6$\r$\n"

  FileClose $R5

  ${If} $R0 = 0
  ${AndIf} $R1 = 0
    !insertmacro MUI_HEADER_TEXT "$(TITLE_ConfigPage1)" "$(SUBTITLE_ConfigPage1)"
  ${Else}
    !insertmacro MUI_HEADER_TEXT "$(TITLE_ConfigPage2)" "$(SUBTITLE_ConfigPage2)"
  ${EndIf}
  !insertmacro MUI_INSTALLOPTIONS_INITDIALOG "ConfigPage2.ini"
  Pop $HDLG ;HWND of dialog

  ; Initialize Controls
  StrCpy $R6 3  ; Field Number

  ; Name
  !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage2.ini" "Field $R6" "HWND"
  SendMessage $HCTL ${EM_LIMITTEXT} 30 0
  IntOp $R6 $R6 + 1

  ${If} $R2 = 1
  ${OrIf} $R3 = 1
    IntOp $R6 $R6 + 1
    ; Port Number
    !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage2.ini" "Field $R6" "HWND"
    SendMessage $HCTL ${EM_LIMITTEXT} 5 0
    IntOp $R6 $R6 + 1
  ${EndIf}

  ${If} $R2 = 1
    IntOp $R6 $R6 + 1
    ; Max Jobs
    !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage2.ini" "Field $R6" "HWND"
    SendMessage $HCTL ${EM_LIMITTEXT} 3 0

    IntOp $R6 $R6 + 1
  ${EndIf}

  ${If} $R2 = 1
  ${OrIf} $R3 = 1
    IntOp $R6 $R6 + 2
  ${EndIf}

  ${If} $R2 = 1
    IntOp $R6 $R6 + 11
  ${ElseIf} $R3 = 1
    IntOp $R6 $R6 + 2
  ${EndIf}

  ${If} $R0 = 1
    ${OrIf} $R1 = 1
    ${OrIf} $R2 = 1
    IntOp $R6 $R6 + 2
    !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage2.ini" "Field $R6" "HWND"
    SendMessage $HCTL ${EM_LIMITTEXT} 30 0
    IntOp $R6 $R6 + 2
  ${EndIf}

  Push $R0
  !insertmacro MUI_INSTALLOPTIONS_SHOW
  Pop $R0

  ;
  ; Process results
  ;
  StrCpy $R6 3

  !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorName "ConfigPage2.ini" "Field $R6" "State"
  IntOp $R6 $R6 + 1

  ${If} $R2 = 1
  ${OrIf} $R3 = 1
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorPort "ConfigPage2.ini" "Field $R6" "State"
    IntOp $R6 $R6 + 1
  ${EndIf}

  ${If} $R2 = 1
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorMaxJobs "ConfigPage2.ini" "Field $R6" "State"
    IntOp $R6 $R6 + 1
  ${EndIf}

  ${If} $R2 = 1
  ${OrIf} $R3 = 1
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorPassword "ConfigPage2.ini" "Field $R6" "State"
    IntOp $R6 $R6 + 1
  ${EndIf}

  ${If} $R2 = 1
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorMailServer "ConfigPage2.ini" "Field $R6" "State"
    IntOp $R6 $R6 + 2
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorMailAddress "ConfigPage2.ini" "Field $R6" "State"
    IntOp $R6 $R6 + 2
    !insertmacro MUI_INSTALLOPTIONS_READ $R5 "ConfigPage2.ini" "Field $R6" "State"
    ${If} $R5 = 1
      StrCpy $ConfigDirectorDB 1
    ${Endif}
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $R5 "ConfigPage2.ini" "Field $R6" "State"
    ${If} $R5 = 1
      StrCpy $ConfigDirectorDB 2
    ${Endif}
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $R5 "ConfigPage2.ini" "Field $R6" "State"
    ${If} $R5 = 1
      StrCpy $ConfigDirectorDB 3
    ${Endif}
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $R5 "ConfigPage2.ini" "Field $R6" "State"
    ${If} $R5 = 1
      StrCpy $ConfigDirectorDB 4
    ${Endif}
    IntOp $R6 $R6 + 1

    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorInstallService "ConfigPage2.ini" "Field $R6" "State"
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorStartService "ConfigPage2.ini" "Field $R6" "State"
    IntOp $R6 $R6 + 1
  ${ElseIf} $R3 = 1
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorAddress "ConfigPage2.ini" "Field $R6" "State"
    IntOp $R6 $R6 + 1
  ${EndIf}

  ${If} $R0 = 1
    ${OrIf} $R1 = 1
    ${OrIf} $R2 = 1

    IntOp $R6 $R6 + 2
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigMonitorName "ConfigPage2.ini" "Field $R6" "State"
    IntOp $R6 $R6 + 2
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigMonitorPassword "ConfigPage2.ini" "Field $R6" "State"
  ${EndIf}
FunctionEnd

Function LeaveConfigPage2
  StrCpy $R6 4

  ${If} $R2 = 1
  ${OrIf} $R3 = 1
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $R0 "ConfigPage2.ini" "Field $R6" "State"
    ${If} $R0 < 1024
    ${OrIf} $R0 > 65535
      MessageBox MB_OK "Port must be between 1024 and 65535 inclusive."
      Abort
    ${EndIf}
    IntOp $R6 $R6 + 1
  ${EndIf}

  ${If} $R2 = 1
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $R0 "ConfigPage2.ini" "Field $R6" "State"
    ${If} $R0 < 1
    ${OrIf} $R0 > 99
      MessageBox MB_OK "Max Jobs must be between 1 and 99 inclusive."
      Abort
    ${EndIf}
    IntOp $R6 $R6 + 1
  ${EndIf}
FunctionEnd
