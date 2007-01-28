# Microsoft Developer Studio Project File - Name="libmiscutil" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libmiscutil - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libmiscutil.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libmiscutil.mak" CFG="libmiscutil - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libmiscutil - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libmiscutil - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libmiscutil - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /G6 /MT /W3 /GX /O2 /I "../../../include" /I "../include" /I "../../../src" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /D _FILE_OFFSET_BITS=64 /YX /FD /c
# ADD BASE RSC /l 0x410 /d "NDEBUG"
# ADD RSC /l 0x410 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libmiscutil - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /G6 /MTd /W3 /Gm /GX /ZI /Od /I "../../../include" /I "../include" /I "../../../src" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /D _FILE_OFFSET_BITS=64 /YX /FD /GZ /c
# ADD BASE RSC /l 0x410 /d "_DEBUG"
# ADD RSC /l 0x410 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "libmiscutil - Win32 Release"
# Name "libmiscutil - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\lib\Array.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\base64.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\drand48.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\getfullhostname.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\getopt.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\hash.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\heap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\html_quote.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\initgroups.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\iso3307.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\md5.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\radix.c
# End Source File
# Begin Source File

SOURCE=..\src\readdir.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\rfc1035.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\rfc1123.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\rfc1738.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\rfc2617.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\safe_inet_addr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\snprintf.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\splay.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\Stack.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\stub_memaccount.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\util.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\uudecode.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\win32lib.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\include\heap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\md5.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\parse.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\radix.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snprintf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\util.h
# End Source File
# End Group
# End Target
# End Project
