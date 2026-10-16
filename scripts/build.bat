@echo off
rem Builds every tool from ..\src into ..\out
rem Needs Visual Studio 2019/2022 (any edition, or Build Tools) with the C++ x64 tools.
setlocal
cd /d "%~dp0.."
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (echo vswhere.exe not found - install Visual Studio or its Build Tools & exit /b 1)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if "%VSDIR%"=="" (echo No Visual Studio with C++ x64 tools found & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist out mkdir out
cl /nologo /O2 /MT /W4 /WX /EHsc /LD /std:c++17 src\ModLoader\ModLoader.cpp /Fo:out\ /Fe:out\version.dll /link /NOLOGO || exit /b 1
cl /nologo /O2 /MT /W4 /WX /EHsc /LD /std:c++17 src\SkillUnlock\SkillUnlock.cpp /Fo:out\ /Fe:out\SkillUnlock.asi /link /NOLOGO || exit /b 1
rem The save tool uses the C# compiler that ships with Windows (.NET Framework 4.x, C# 5).
"%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /warnaserror /target:winexe /optimize /out:out\AvengersSaveTool.exe src\SaveTool\AvengersSaveTool.cs || exit /b 1
del /q out\*.obj out\*.lib out\*.exp 2>nul
echo BUILD OK - files are in %CD%\out
