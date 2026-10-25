@echo off
rem Builds every tool from ..\src into ..\out
rem Needs Visual Studio 2019/2022 (any edition, or Build Tools) with the C++ x64 tools.
rem The versions come from ..\versions.txt (checked against the docs first by check-versions.ps1).
rem /Brepro: the same sources and compiler give the same files. /Zi with /PDBALTPATH: a .pdb for each binary in out\,
rem with no local path written into the binary; the .pdb files are never packaged.
setlocal
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check-versions.ps1 || exit /b 1
for /f "usebackq tokens=1,2 delims==" %%a in ("versions.txt") do set "VER_%%a=%%b"
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (echo vswhere.exe not found - install Visual Studio or its Build Tools & exit /b 1)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if "%VSDIR%"=="" (echo No Visual Studio with C++ x64 tools found & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
echo MSVC toolset %VCToolsVersion%
if not exist out mkdir out
set CL_FLAGS=/nologo /O2 /MT /W4 /WX /EHsc /LD /std:c++17 /Zi /Brepro
set LINK_FLAGS=/NOLOGO /DEBUG /OPT:REF /OPT:ICF /Brepro /PDBALTPATH:%%_PDB%%
cl %CL_FLAGS% /DMOD_VERSION=\"%VER_ModLoader%\" src\ModLoader\ModLoader.cpp /Fo:out\ /Fd:out\ModLoader.cl.pdb /Fe:out\version.dll /link %LINK_FLAGS% /PDB:out\version.pdb || exit /b 1
cl %CL_FLAGS% /DMOD_VERSION=\"%VER_SkillUnlock%\" src\SkillUnlock\SkillUnlock.cpp /Fo:out\ /Fd:out\SkillUnlock.cl.pdb /Fe:out\SkillUnlock.asi /link %LINK_FLAGS% /PDB:out\SkillUnlock.pdb || exit /b 1
cl %CL_FLAGS% /DMOD_VERSION=\"%VER_CollectionUnlock%\" src\CollectionUnlock\CollectionUnlock.cpp /Fo:out\ /Fd:out\CollectionUnlock.cl.pdb /Fe:out\CollectionUnlock.asi /link %LINK_FLAGS% /PDB:out\CollectionUnlock.pdb || exit /b 1
cl %CL_FLAGS% /DMOD_VERSION=\"%VER_QuickStart%\" src\QuickStart\QuickStart.cpp /Fo:out\ /Fd:out\QuickStart.cl.pdb /Fe:out\QuickStart.asi /link %LINK_FLAGS% /PDB:out\QuickStart.pdb || exit /b 1
rem The save tool uses the C# compiler that ships with Windows (.NET Framework 4.5 or later, C# 5); its version is a
rem generated file.
> out\BuildInfo.g.cs echo [assembly: System.Reflection.AssemblyVersion("%VER_SaveTool%.0.0")]
>> out\BuildInfo.g.cs echo static class BuildInfo { public const string Version = "%VER_SaveTool%"; }
"%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /warnaserror /target:winexe /optimize /out:out\AvengersSaveTool.exe src\SaveTool\AvengersSaveTool.cs out\BuildInfo.g.cs || exit /b 1
del /q out\*.obj out\*.lib out\*.exp out\*.cl.pdb 2>nul
echo BUILD OK - files are in %CD%\out
