# Microsoft Developer Studio Project File - Name="modules" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Generic Project" 0x010a

CFG=modules - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "modules.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "modules.mak" CFG="modules - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "modules - Win32 Release" (based on "Win32 (x86) Generic Project")
!MESSAGE "modules - Win32 Debug" (based on "Win32 (x86) Generic Project")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
MTL=midl.exe

!IF  "$(CFG)" == "modules - Win32 Release"

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

!ELSEIF  "$(CFG)" == "modules - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "modules___Win32_Debug"
# PROP BASE Intermediate_Dir "modules___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\src"
# PROP Intermediate_Dir "."
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "modules - Win32 Release"
# Name "modules - Win32 Debug"
# Begin Source File

SOURCE=.\auth_modules.cmd

!IF  "$(CFG)" == "modules - Win32 Release"

# PROP Intermediate_Dir "."
# PROP Ignore_Default_Tool 1
USERDEP__AUTH_="squid_version.mak"	"squid_mswin.mak"	
# Begin Custom Build
OutDir=.\..\..\src
ProjDir=.
InputPath=.\auth_modules.cmd

"$(OutDir)\auth_modules.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nmake /f $(ProjDir)\squid_mswin.mak auth_modules.c

# End Custom Build

!ELSEIF  "$(CFG)" == "modules - Win32 Debug"

# PROP Intermediate_Dir "."
# PROP Ignore_Default_Tool 1
USERDEP__AUTH_="squid_version.mak"	"squid_mswin.mak"	
# Begin Custom Build
OutDir=.\..\..\src
ProjDir=.
InputPath=.\auth_modules.cmd

"$(OutDir)\auth_modules.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nmake /f $(ProjDir)\squid_mswin.mak auth_modules.c

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\default_config_file.cmd

!IF  "$(CFG)" == "modules - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__DEFAU="squid_version.mak"	"squid_mswin.mak"	
# Begin Custom Build
ProjDir=.
InputPath=.\default_config_file.cmd

"$(ProjDir)\include\default_config_file.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nmake /f $(ProjDir)\squid_mswin.mak default_config_file.h

# End Custom Build

!ELSEIF  "$(CFG)" == "modules - Win32 Debug"

# PROP Ignore_Default_Tool 1
USERDEP__DEFAU="squid_version.mak"	"squid_mswin.mak"	
# Begin Custom Build
ProjDir=.
InputPath=.\default_config_file.cmd

"$(ProjDir)\include\default_config_file.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nmake /f $(ProjDir)\squid_mswin.mak default_config_file.h

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\repl_modules.cmd

!IF  "$(CFG)" == "modules - Win32 Release"

# PROP Intermediate_Dir "."
# PROP Ignore_Default_Tool 1
USERDEP__REPL_="squid_version.mak"	"squid_mswin.mak"	
# Begin Custom Build
OutDir=.\..\..\src
ProjDir=.
InputPath=.\repl_modules.cmd

"$(OutDir)\repl_modules.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nmake /f $(ProjDir)\squid_mswin.mak repl_modules.c

# End Custom Build

!ELSEIF  "$(CFG)" == "modules - Win32 Debug"

# PROP Intermediate_Dir "."
# PROP Ignore_Default_Tool 1
USERDEP__REPL_="squid_version.mak"	"squid_mswin.mak"	
# Begin Custom Build
OutDir=.\..\..\src
ProjDir=.
InputPath=.\repl_modules.cmd

"$(OutDir)\repl_modules.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nmake /f $(ProjDir)\squid_mswin.mak repl_modules.c

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\squid_version.cmd

!IF  "$(CFG)" == "modules - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__SQUID="..\..\configure.in"	"squid_mswin.mak"	
# Begin Custom Build
ProjDir=.
InputPath=.\squid_version.cmd

"$(ProjDir)\squid_version.mak" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(ProjDir)\squid_version.cmd > $(ProjDir)\squid_version.mak

# End Custom Build

!ELSEIF  "$(CFG)" == "modules - Win32 Debug"

# PROP Ignore_Default_Tool 1
USERDEP__SQUID="..\..\configure.in"	"squid_mswin.mak"	
# Begin Custom Build
ProjDir=.
InputPath=.\squid_version.cmd

"$(ProjDir)\squid_version.mak" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(ProjDir)\squid_version.cmd > $(ProjDir)\squid_version.mak

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\store_modules.cmd

!IF  "$(CFG)" == "modules - Win32 Release"

# PROP Intermediate_Dir "."
# PROP Ignore_Default_Tool 1
USERDEP__STORE="squid_version.mak"	"squid_mswin.mak"	
# Begin Custom Build
OutDir=.\..\..\src
ProjDir=.
InputPath=.\store_modules.cmd

"$(OutDir)\store_modules.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nmake /f $(ProjDir)\squid_mswin.mak store_modules.c

# End Custom Build

!ELSEIF  "$(CFG)" == "modules - Win32 Debug"

# PROP Intermediate_Dir "."
# PROP Ignore_Default_Tool 1
USERDEP__STORE="squid_version.mak"	"squid_mswin.mak"	
# Begin Custom Build
OutDir=.\..\..\src
ProjDir=.
InputPath=.\store_modules.cmd

"$(OutDir)\store_modules.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nmake /f $(ProjDir)\squid_mswin.mak store_modules.c

# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
