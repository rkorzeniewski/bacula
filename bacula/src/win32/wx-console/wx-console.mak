# Microsoft Developer Studio Generated NMAKE File, Based on wx-console.dsp
!IF "$(CFG)" == ""
CFG=wx-console - Win32 Release
!MESSAGE No configuration specified. Defaulting to wx-console - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "wx-console - Win32 Release" && "$(CFG)" != "wx-console - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "wx-console.mak" CFG="wx-console - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "wx-console - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "wx-console - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "wx-console - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\wx-console.exe"


CLEAN :
        -@erase "$(INTDIR)\alist.obj"
        -@erase "$(INTDIR)\alloc.obj"
        -@erase "$(INTDIR)\attr.obj"
        -@erase "$(INTDIR)\base64.obj"
        -@erase "$(INTDIR)\bget_msg.obj"
        -@erase "$(INTDIR)\bnet.obj"
        -@erase "$(INTDIR)\bnet_pkt.obj"
        -@erase "$(INTDIR)\bnet_server.obj"
        -@erase "$(INTDIR)\bshm.obj"
        -@erase "$(INTDIR)\bsys.obj"
        -@erase "$(INTDIR)\btime.obj"
        -@erase "$(INTDIR)\cram-md5.obj"
        -@erase "$(INTDIR)\crc32.obj"
        -@erase "$(INTDIR)\daemon.obj"
        -@erase "$(INTDIR)\dlist.obj"
        -@erase "$(INTDIR)\edit.obj"
        -@erase "$(INTDIR)\fnmatch.obj"
        -@erase "$(INTDIR)\hmac.obj"
        -@erase "$(INTDIR)\htable.obj"
        -@erase "$(INTDIR)\idcache.obj"
        -@erase "$(INTDIR)\jcr.obj"
        -@erase "$(INTDIR)\lex.obj"
        -@erase "$(INTDIR)\md5.obj"
        -@erase "$(INTDIR)\mem_pool.obj"
        -@erase "$(INTDIR)\message.obj"
        -@erase "$(INTDIR)\parse_conf.obj"
        -@erase "$(INTDIR)\queue.obj"
        -@erase "$(INTDIR)\rwlock.obj"
        -@erase "$(INTDIR)\scan.obj"
        -@erase "$(INTDIR)\semlock.obj"
        -@erase "$(INTDIR)\serial.obj"
        -@erase "$(INTDIR)\sha1.obj"
        -@erase "$(INTDIR)\signal.obj"
        -@erase "$(INTDIR)\smartall.obj"
        -@erase "$(INTDIR)\timers.obj"
        -@erase "$(INTDIR)\tree.obj"
        -@erase "$(INTDIR)\util.obj"
        -@erase "$(INTDIR)\var.obj"
        -@erase "$(INTDIR)\watchdog.obj"
        -@erase "$(INTDIR)\workq.obj"
        -@erase "$(INTDIR)\compat.obj"
        -@erase "$(INTDIR)\print.obj"
        -@erase "$(INTDIR)\authenticate.obj"
        -@erase "$(INTDIR)\console_conf.obj"
        -@erase "$(INTDIR)\console_thread.obj"
        -@erase "$(INTDIR)\main.obj"
        -@erase "$(INTDIR)\wxblistctrl.obj"
        -@erase "$(INTDIR)\wxbmainframe.obj"
        -@erase "$(INTDIR)\wxbrestorepanel.obj"
        -@erase "$(INTDIR)\wxbtableparser.obj"
        -@erase "$(INTDIR)\wxbtreectrl.obj"
        -@erase "$(INTDIR)\wxbutils.obj"
        -@erase "$(OUTDIR)\wx-console.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "../compat" /I "../.." /I "../../../depkgs-win32/wx/include" /I "../../../../depkgs-win32/wx/include" /I "../../../../depkgs-win32/wx/lib/msw" /I "../../../../depkgs-win32/pthreads" /I "../../../../depkgs-win32/zlib" /I "." /D "NDEBUG" /D "WIN32" /D "__WXMSW__" /D "_CONSOLE" /D "_MBCS" /D "HAVE_WIN32" /D "HAVE_WXCONSOLE" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\winres.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\wx-console.bsc" 
BSC32_SBRS= \
        
LINK32=link.exe
LINK32_FLAGS=wxmsw.lib rpcrt4.lib oleaut32.lib ole32.lib uuid.lib winspool.lib winmm.lib \
  comctl32.lib comdlg32.lib Shell32.lib AdvAPI32.lib User32.lib Gdi32.lib wsock32.lib \
  wldap32.lib pthreadVCE.lib zlib.lib /nodefaultlib:libcmt.lib \
  /nologo /subsystem:windows /machine:I386 /out:"$(OUTDIR)\wx-console.exe" /libpath:"../../../../depkgs-win32/wx/lib" /libpath:"../../../../depkgs-win32/pthreads" /libpath:"../../../../depkgs-win32/zlib" 
LINK32_OBJS= \
        "$(INTDIR)\alist.obj" \
        "$(INTDIR)\alloc.obj" \
        "$(INTDIR)\attr.obj" \
        "$(INTDIR)\base64.obj" \
        "$(INTDIR)\bget_msg.obj" \
        "$(INTDIR)\bnet.obj" \
        "$(INTDIR)\bnet_pkt.obj" \
        "$(INTDIR)\bnet_server.obj" \
        "$(INTDIR)\bshm.obj" \
        "$(INTDIR)\bsys.obj" \
        "$(INTDIR)\btime.obj" \
        "$(INTDIR)\cram-md5.obj" \
        "$(INTDIR)\crc32.obj" \
        "$(INTDIR)\daemon.obj" \
        "$(INTDIR)\dlist.obj" \
        "$(INTDIR)\edit.obj" \
        "$(INTDIR)\fnmatch.obj" \
        "$(INTDIR)\hmac.obj" \
        "$(INTDIR)\htable.obj" \
        "$(INTDIR)\idcache.obj" \
        "$(INTDIR)\jcr.obj" \
        "$(INTDIR)\lex.obj" \
        "$(INTDIR)\md5.obj" \
        "$(INTDIR)\mem_pool.obj" \
        "$(INTDIR)\message.obj" \
        "$(INTDIR)\parse_conf.obj" \
        "$(INTDIR)\queue.obj" \
        "$(INTDIR)\rwlock.obj" \
        "$(INTDIR)\scan.obj" \
        "$(INTDIR)\semlock.obj" \
        "$(INTDIR)\serial.obj" \
        "$(INTDIR)\sha1.obj" \
        "$(INTDIR)\signal.obj" \
        "$(INTDIR)\smartall.obj" \
        "$(INTDIR)\timers.obj" \
        "$(INTDIR)\tree.obj" \
        "$(INTDIR)\util.obj" \
        "$(INTDIR)\var.obj" \
        "$(INTDIR)\watchdog.obj" \
        "$(INTDIR)\workq.obj" \
        "$(INTDIR)\compat.obj" \
        "$(INTDIR)\print.obj" \
        "$(INTDIR)\authenticate.obj" \
        "$(INTDIR)\console_conf.obj" \
        "$(INTDIR)\console_thread.obj" \
        "$(INTDIR)\main.obj" \
        "$(INTDIR)\wxblistctrl.obj" \
        "$(INTDIR)\wxbmainframe.obj" \
        "$(INTDIR)\wxbrestorepanel.obj" \
        "$(INTDIR)\wxbtableparser.obj" \
        "$(INTDIR)\wxbtreectrl.obj" \
        "$(INTDIR)\wxbutils.obj" \
        "$(INTDIR)\wx-console_private.res"

"$(OUTDIR)\wx-console.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "wx-console - Win32 Debug"

# Don't make debug...

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("wx-console.dep")
!INCLUDE "wx-console.dep"
!ELSE 
!MESSAGE Warning: cannot find "wx-console.dep"
!ENDIF 
!ENDIF 

SOURCE=..\..\wx-console\wx-console_private.rc

"$(INTDIR)\wx-console_private.res" : $(SOURCE) "$(INTDIR)"
        $(RSC) /l 0x409 /fo"$(INTDIR)\wx-console_private.res" /d "NDEBUG" $(SOURCE)

FILENAME=alist
SOURCE=..\lib\alist.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=alloc
SOURCE=..\lib\alloc.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=attr
SOURCE=..\lib\attr.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=base64
SOURCE=..\lib\base64.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=bget_msg
SOURCE=..\lib\bget_msg.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=bnet
SOURCE=..\lib\bnet.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=bnet_pkt
SOURCE=..\lib\bnet_pkt.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=bnet_server
SOURCE=..\lib\bnet_server.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=bshm
SOURCE=..\lib\bshm.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=bsys
SOURCE=..\lib\bsys.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=btime
SOURCE=..\lib\btime.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=cram-md5
SOURCE=..\lib\cram-md5.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=crc32
SOURCE=..\lib\crc32.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=daemon
SOURCE=..\lib\daemon.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=dlist
SOURCE=..\lib\dlist.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=edit
SOURCE=..\lib\edit.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=fnmatch
SOURCE=..\lib\fnmatch.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=hmac
SOURCE=..\lib\hmac.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=htable
SOURCE=..\lib\htable.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=idcache
SOURCE=..\lib\idcache.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=jcr
SOURCE=..\lib\jcr.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=lex
SOURCE=..\lib\lex.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=md5
SOURCE=..\lib\md5.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=mem_pool
SOURCE=..\lib\mem_pool.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=message
SOURCE=..\lib\message.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=parse_conf
SOURCE=..\lib\parse_conf.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=queue
SOURCE=..\lib\queue.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=rwlock
SOURCE=..\lib\rwlock.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=scan
SOURCE=..\lib\scan.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=semlock
SOURCE=..\lib\semlock.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=serial
SOURCE=..\lib\serial.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=sha1
SOURCE=..\lib\sha1.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=signal
SOURCE=..\lib\signal.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=smartall
SOURCE=..\lib\smartall.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=timers
SOURCE=..\lib\timers.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=tree
SOURCE=..\lib\tree.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=util
SOURCE=..\lib\util.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=var
SOURCE=..\lib\var.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=watchdog
SOURCE=..\lib\watchdog.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=workq
SOURCE=..\lib\workq.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=compat
SOURCE=..\compat\compat.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=print
SOURCE=..\compat\print.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=authenticate
SOURCE=.\authenticate.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=console_conf
SOURCE=.\console_conf.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=console_thread
SOURCE=..\..\wx-console\console_thread.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=main
SOURCE=..\..\wx-console\main.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=wxblistctrl
SOURCE=..\..\wx-console\wxblistctrl.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=wxbmainframe
SOURCE=..\..\wx-console\wxbmainframe.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=wxbrestorepanel
SOURCE=..\..\wx-console\wxbrestorepanel.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=wxbtableparser
SOURCE=..\..\wx-console\wxbtableparser.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=wxbtreectrl
SOURCE=..\..\wx-console\wxbtreectrl.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)

FILENAME=wxbutils
SOURCE=..\..\wx-console\wxbutils.cpp
"$(INTDIR)\$(FILENAME).obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) $(CPP_PROJ) $(SOURCE)
