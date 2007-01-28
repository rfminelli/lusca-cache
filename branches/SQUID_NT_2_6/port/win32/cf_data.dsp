# Microsoft Developer Studio Project File - Name="cf_data" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Generic Project" 0x010a

CFG=cf_data - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "cf_data.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "cf_data.mak" CFG="cf_data - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "cf_data - Win32 Release" (based on "Win32 (x86) Generic Project")
!MESSAGE "cf_data - Win32 Debug" (based on "Win32 (x86) Generic Project")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
MTL=midl.exe

!IF  "$(CFG)" == "cf_data - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "."
# PROP Intermediate_Dir "."
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "cf_data - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "cf_data___Win32_Debug"
# PROP BASE Intermediate_Dir "cf_data___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "."
# PROP Intermediate_Dir "."
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "cf_data - Win32 Release"
# Name "cf_data - Win32 Debug"
# Begin Source File

SOURCE=..\..\src\cf.data.pre

!IF  "$(CFG)" == "cf_data - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__CF_DA="squid_mswin.mak"	
# Begin Custom Build
InputDir=\WORK\CYGWIN-SVC-2_5\src
ProjDir=.
InputPath=..\..\src\cf.data.pre

BuildCmds= \
	nmake /f $(ProjDir)\squid_mswin.mak cf.data \
	nmake /f $(ProjDir)\squid_mswin.mak cf_gen_defines.h \
	

"$(InputDir)\cf.data" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\cf_gen_defines.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "cf_data - Win32 Debug"

# PROP Ignore_Default_Tool 1
USERDEP__CF_DA="squid_mswin.mak"	
# Begin Custom Build
InputDir=\WORK\CYGWIN-SVC-2_5\src
ProjDir=.
InputPath=..\..\src\cf.data.pre

BuildCmds= \
	nmake /f $(ProjDir)\squid_mswin.mak cf.data \
	nmake /f $(ProjDir)\squid_mswin.mak cf_gen_defines.h \
	

"$(InputDir)\cf.data" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\cf_gen_defines.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
