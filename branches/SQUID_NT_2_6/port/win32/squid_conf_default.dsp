# Microsoft Developer Studio Project File - Name="squid_conf_default" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Generic Project" 0x010a

CFG=squid_conf_default - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "squid_conf_default.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "squid_conf_default.mak" CFG="squid_conf_default - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "squid_conf_default - Win32 Release" (based on "Win32 (x86) Generic Project")
!MESSAGE "squid_conf_default - Win32 Debug" (based on "Win32 (x86) Generic Project")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
MTL=midl.exe

!IF  "$(CFG)" == "squid_conf_default - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\src"
# PROP Intermediate_Dir "."
# PROP Target_Dir ""
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=del ..\..\src\squid.conf.default	move squid.conf.default ..\..\src\squid.conf.default	del ..\..\src\cf_parser.h	move cf_parser.h ..\..\src\cf_parser.h
# End Special Build Tool

!ELSEIF  "$(CFG)" == "squid_conf_default - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "squid_conf_default___Win32_Debug"
# PROP BASE Intermediate_Dir "squid_conf_default___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\src"
# PROP Intermediate_Dir "."
# PROP Target_Dir ""
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=del ..\..\src\squid.conf.default	move squid.conf.default ..\..\src\squid.conf.default	del ..\..\src\cf_parser.h	move cf_parser.h ..\..\src\cf_parser.h
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "squid_conf_default - Win32 Release"
# Name "squid_conf_default - Win32 Debug"
# Begin Source File

SOURCE=..\..\src\cf.data

!IF  "$(CFG)" == "squid_conf_default - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\..\..\src
ProjDir=.
InputPath=..\..\src\cf.data

BuildCmds= \
	$(ProjDir)\cf_gen\Release\cf_gen $(InputPath)

"$(OutDir)\squid.conf.default" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(OutDir)\cf_parser.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "squid_conf_default - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\..\..\src
ProjDir=.
InputPath=..\..\src\cf.data

BuildCmds= \
	$(ProjDir)\cf_gen\debug\cf_gen $(InputPath)

"$(OutDir)\squid.conf.default" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(OutDir)\cf_parser.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
