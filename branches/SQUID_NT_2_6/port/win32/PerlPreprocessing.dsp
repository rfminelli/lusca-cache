# Microsoft Developer Studio Project File - Name="PerlPreprocessing" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Generic Project" 0x010a

CFG=PerlPreprocessing - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "PerlPreprocessing.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "PerlPreprocessing.mak" CFG="PerlPreprocessing - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "PerlPreprocessing - Win32 Release" (based on "Win32 (x86) Generic Project")
!MESSAGE "PerlPreprocessing - Win32 Debug" (based on "Win32 (x86) Generic Project")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
MTL=midl.exe

!IF  "$(CFG)" == "PerlPreprocessing - Win32 Release"

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

!ELSEIF  "$(CFG)" == "PerlPreprocessing - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "."
# PROP Intermediate_Dir "."
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "PerlPreprocessing - Win32 Release"
# Name "PerlPreprocessing - Win32 Debug"
# Begin Source File

SOURCE=..\..\src\enums.h

!IF  "$(CFG)" == "PerlPreprocessing - Win32 Release"

# PROP Intermediate_Dir "."
# PROP Ignore_Default_Tool 1
# Begin Custom Build
InputDir=\WORK\CYGWIN-SVC-2_5\src
InputPath=..\..\src\enums.h

"$(InputDir)\string_arrays.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl $(InputDir)\mk-string-arrays.pl <$(InputPath) >$(InputDir)\string_arrays.c

# End Custom Build

!ELSEIF  "$(CFG)" == "PerlPreprocessing - Win32 Debug"

# PROP Intermediate_Dir "."
# PROP Ignore_Default_Tool 1
# Begin Custom Build
InputDir=\WORK\CYGWIN-SVC-2_5\src
InputPath=..\..\src\enums.h

"$(InputDir)\string_arrays.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl $(InputDir)\mk-string-arrays.pl <$(InputPath) >$(InputDir)\string_arrays.c

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\src\globals.h

!IF  "$(CFG)" == "PerlPreprocessing - Win32 Release"

# PROP Intermediate_Dir "."
# PROP Ignore_Default_Tool 1
# Begin Custom Build
InputDir=\WORK\CYGWIN-SVC-2_5\src
InputPath=..\..\src\globals.h

"$(InputDir)\globals.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl $(InputDir)\mk-globals-c.pl <$(InputPath) >$(InputDir)\globals.c

# End Custom Build

!ELSEIF  "$(CFG)" == "PerlPreprocessing - Win32 Debug"

# PROP Intermediate_Dir "."
# PROP Ignore_Default_Tool 1
# Begin Custom Build
InputDir=\WORK\CYGWIN-SVC-2_5\src
InputPath=..\..\src\globals.h

"$(InputDir)\globals.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl $(InputDir)\mk-globals-c.pl <$(InputPath) >$(InputDir)\globals.c

# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
