@echo off
rem Builds the tests from ..\tests into ..\out\tests and runs them. Needs what build.bat needs.
rem Arguments, all optional, go to SaveToolTests: STEAMID FOLDER... also checks copies of the saves in those folders.
setlocal
cd /d "%~dp0.."
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (echo vswhere.exe not found - install Visual Studio or its Build Tools & exit /b 1)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if "%VSDIR%"=="" (echo No Visual Studio with C++ x64 tools found & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
for /f "usebackq tokens=1,2 delims==" %%a in ("versions.txt") do set "VER_%%a=%%b"
set T=out\tests
if exist %T% rmdir /s /q %T%
mkdir %T% || exit /b 1
set CL_FLAGS=/nologo /O2 /MT /W4 /WX /EHsc /std:c++17 /Fo:%T%\
rem Stand-ins: tile mods that export the chain marker (version 1, and 0 = no known version), and .asi mods for the loader.
cl %CL_FLAGS% /LD tests\ChainTile.cpp /Fe:%T%\ChainTileA.dll || exit /b 1
cl %CL_FLAGS% /LD tests\ChainTile.cpp /Fe:%T%\ChainTileB.dll || exit /b 1
cl %CL_FLAGS% /LD /DCHAIN_VERSION=0 tests\ChainTile.cpp /Fe:%T%\ChainTileOld.dll || exit /b 1
cl %CL_FLAGS% /LD /DMODE_OK tests\TestMod.cpp /Fe:%T%\TestModOk.dll || exit /b 1
cl %CL_FLAGS% /LD /DMODE_CRASH tests\TestMod.cpp /Fe:%T%\TestModCrash.dll || exit /b 1
cl %CL_FLAGS% /LD /DMODE_NOINIT tests\TestMod.cpp /Fe:%T%\TestModNoInit.dll || exit /b 1
rem The shared headers' test is named as Skill Unlock: the tile code knows tile mods by their file names.
cl %CL_FLAGS% tests\CommonTests.cpp /Fe:%T%\SkillUnlock-CommonTests.exe || exit /b 1
cl %CL_FLAGS% tests\QuickStartTests.cpp /Fe:%T%\QuickStartTests.exe || exit /b 1
cl %CL_FLAGS% tests\SkillUnlockTests.cpp /Fe:%T%\SkillUnlockTests.exe || exit /b 1
cl %CL_FLAGS% tests\CollectionUnlockTests.cpp /Fe:%T%\CollectionUnlockTests.exe || exit /b 1
cl %CL_FLAGS% tests\ModLoaderTests.cpp /Fe:%T%\ModLoaderTests.exe || exit /b 1
> %T%\BuildInfo.g.cs echo [assembly: System.Reflection.AssemblyVersion("%VER_SaveTool%.0.0")]
>> %T%\BuildInfo.g.cs echo static class BuildInfo { public const string Version = "%VER_SaveTool%"; }
"%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /warnaserror /target:exe /optimize /main:SaveToolTests /out:%T%\SaveToolTests.exe src\SaveTool\AvengersSaveTool.cs %T%\BuildInfo.g.cs tests\SaveToolTests.cs || exit /b 1
cd %T%
set FAILED=0
.\SkillUnlock-CommonTests.exe || set FAILED=1
.\QuickStartTests.exe || set FAILED=1
.\SkillUnlockTests.exe || set FAILED=1
.\CollectionUnlockTests.exe || set FAILED=1
.\ModLoaderTests.exe || set FAILED=1
.\SaveToolTests.exe %* || set FAILED=1
if %FAILED%==1 (echo TESTS FAILED & exit /b 1)
echo TESTS OK
