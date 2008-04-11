# Microsoft Developer Studio Project File - Name="libsnmp" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libsnmp - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libsnmp.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libsnmp.mak" CFG="libsnmp - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libsnmp - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libsnmp - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libsnmp - Win32 Release"

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
# ADD CPP /nologo /G6 /MT /W3 /GX /O2 /I "../../../include" /I "../include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D SQUID_SNMP=1 /D _FILE_OFFSET_BITS=64 /YX /FD /c
# ADD BASE RSC /l 0x410 /d "NDEBUG"
# ADD RSC /l 0x410 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libsnmp - Win32 Debug"

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
# ADD CPP /nologo /G6 /MTd /W3 /Gm /GX /ZI /Od /I "../../../include" /I "../include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D SQUID_SNMP=1 /D _FILE_OFFSET_BITS=64 /YX /FD /GZ /c
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

# Name "libsnmp - Win32 Release"
# Name "libsnmp - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\snmplib\asn1.c
# End Source File
# Begin Source File

SOURCE=..\..\..\snmplib\coexistance.c
# End Source File
# Begin Source File

SOURCE=..\..\..\snmplib\mib.c
# End Source File
# Begin Source File

SOURCE=..\..\..\snmplib\parse.c
# End Source File
# Begin Source File

SOURCE=..\..\..\snmplib\snmp_api.c
# End Source File
# Begin Source File

SOURCE=..\..\..\snmplib\snmp_api_error.c
# End Source File
# Begin Source File

SOURCE=..\..\..\snmplib\snmp_error.c
# End Source File
# Begin Source File

SOURCE=..\..\..\snmplib\snmp_msg.c
# End Source File
# Begin Source File

SOURCE=..\..\..\snmplib\snmp_pdu.c
# End Source File
# Begin Source File

SOURCE=..\..\..\snmplib\snmp_vars.c
# End Source File
# Begin Source File

SOURCE=..\..\..\snmplib\snmplib_debug.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\..\..\include\snmp-internal.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\include\snmp-mib.h"
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_api.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_api_error.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_api_util.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_client.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_coexist.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_compat.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_debug.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_error.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_impl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_msg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_pdu.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_session.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_util.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\snmp_vars.h
# End Source File
# End Group
# End Target
# End Project
