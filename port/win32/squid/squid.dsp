# Microsoft Developer Studio Project File - Name="squid" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=squid - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "squid.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "squid.mak" CFG="squid - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "squid - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "squid - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "squid - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MT /W3 /GX /Ob1 /I "../../" /I "../include" /I "../../../include" /I "../../../src" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /D _FILE_OFFSET_BITS=64 /YX /FD /c
# ADD BASE RSC /l 0x410 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 ws2_32.lib advapi32.lib psapi.lib Iphlpapi.lib libeay32MT.lib ssleay32MT.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "squid - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /G6 /MTd /W3 /Gm /GX /ZI /I "../include" /I "../../../include" /I "../../../src" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /D _FILE_OFFSET_BITS=64 /YX /FD /GZ /c
# ADD BASE RSC /l 0x410 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ws2_32.lib advapi32.lib psapi.lib Iphlpapi.lib libeay32MTd.lib ssleay32MTd.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "squid - Win32 Release"
# Name "squid - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\src\access_log.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\acl.c
# ADD CPP /D "AUTH_ON_ACCELERATION"
# End Source File
# Begin Source File

SOURCE=..\..\..\src\asn.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\auth_modules.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\authenticate.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\cache_cf.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\cache_manager.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\CacheDigest.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\carp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\cbdata.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\client_db.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\client_side.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\comm.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\comm_select_win32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\debug.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\delay_pools.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\disk.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\dns.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\dns_internal.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\errormap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\errorpage.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\event.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\external_acl.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\fd.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\filemap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\forward.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\fqdncache.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\ftp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\globals.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\gopher.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\helper.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\htcp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\http.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\HttpBody.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\HttpHdrCc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\HttpHdrContRange.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\HttpHdrRange.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\HttpHeader.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\HttpHeaderTools.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\HttpMsg.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\HttpReply.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\HttpRequest.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\HttpStatusLine.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\icmp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\icp_v2.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\icp_v3.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\ident.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\internal.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\ipc_win32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\ipcache.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\leakfinder.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\locrewrite.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\logfile.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\main.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\mem.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\MemBuf.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\MemPool.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\mime.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\multicast.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\neighbors.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\net_db.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\Packer.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\pconn.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\peer_digest.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\peer_monitor.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\peer_select.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\peer_sourcehash.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\peer_userhash.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\redirect.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\referer.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\refresh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\repl_modules.c
# End Source File
# Begin Source File

SOURCE="..\..\..\src\send-announce.c"
# End Source File
# Begin Source File

SOURCE=..\..\..\src\snmp_agent.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\snmp_core.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\ssl.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\ssl_support.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\stat.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\StatHist.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\stmem.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\store.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\store_client.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\store_digest.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\store_dir.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\store_io.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\store_key_md5.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\store_log.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\store_modules.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\store_rebuild.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\store_swapin.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\store_swapmeta.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\store_swapout.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\String.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\string_arrays.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\tools.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\unlinkd.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\url.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\urn.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\useragent.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\wais.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\wccp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\whois.c
# End Source File
# Begin Source File

SOURCE=..\..\..\src\win32.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\src\defines.h
# End Source File
# Begin Source File

SOURCE=..\..\..\src\enums.h
# End Source File
# Begin Source File

SOURCE=..\..\..\src\protos.h
# End Source File
# Begin Source File

SOURCE=..\..\..\src\squid.h
# End Source File
# Begin Source File

SOURCE=..\..\..\src\structs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\src\typedefs.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\buildver.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\squid.rc
# End Source File
# End Group
# Begin Source File

SOURCE=.\buildcount.dsm
# End Source File
# End Target
# End Project
