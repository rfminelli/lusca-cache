# Microsoft Developer Studio Project File - Name="doc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Generic Project" 0x010a

CFG=doc - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "doc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "doc.mak" CFG="doc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "doc - Win32 Release" (based on "Win32 (x86) Generic Project")
!MESSAGE "doc - Win32 Debug" (based on "Win32 (x86) Generic Project")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
MTL=midl.exe

!IF  "$(CFG)" == "doc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "doc___Win32_Release"
# PROP BASE Intermediate_Dir "doc___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "doc___Win32_Release"
# PROP Intermediate_Dir "doc___Win32_Release"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "doc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "doc___Win32_Debug"
# PROP BASE Intermediate_Dir "doc___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "doc___Win32_Debug"
# PROP Intermediate_Dir "doc___Win32_Debug"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "doc - Win32 Release"
# Name "doc - Win32 Debug"
# Begin Source File

SOURCE=..\..\doc\cachemgr.cgi.8.in

!IF  "$(CFG)" == "doc - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__CACHE="squid_mswin.mak"	"squid_version.mak"	
# Begin Custom Build
InputDir=\work\nt-2.6\doc
ProjDir=.
InputPath=..\..\doc\cachemgr.cgi.8.in

"$(InputDir)\cachemgr.cgi.8" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nmake /f $(ProjDir)\squid_mswin.mak cachemgr.cgi.8

# End Custom Build

!ELSEIF  "$(CFG)" == "doc - Win32 Debug"

# PROP Ignore_Default_Tool 1
USERDEP__CACHE="squid_mswin.mak"	"squid_version.mak"	
# Begin Custom Build
InputDir=\work\nt-2.6\doc
ProjDir=.
InputPath=..\..\doc\cachemgr.cgi.8.in

"$(InputDir)\cachemgr.cgi.8" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nmake /f $(ProjDir)\squid_mswin.mak cachemgr.cgi.8

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\doc\squid.8.in

!IF  "$(CFG)" == "doc - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__SQUID="squid_mswin.mak"	"squid_version.mak"	
# Begin Custom Build
InputDir=\work\nt-2.6\doc
ProjDir=.
InputPath=..\..\doc\squid.8.in

"$(InputDir)\squid.8" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nmake /f $(ProjDir)\squid_mswin.mak squid.8

# End Custom Build

!ELSEIF  "$(CFG)" == "doc - Win32 Debug"

# PROP Ignore_Default_Tool 1
USERDEP__SQUID="squid_mswin.mak"	"squid_version.mak"	
# Begin Custom Build
InputDir=\work\nt-2.6\doc
ProjDir=.
InputPath=..\..\doc\squid.8.in

"$(InputDir)\squid.8" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nmake /f $(ProjDir)\squid_mswin.mak squid.8

# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
