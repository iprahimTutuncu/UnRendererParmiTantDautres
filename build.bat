@echo off
setlocal

:: Find latest Visual Studio installation
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do (
    set VSINSTALL=%%i
)

if not defined VSINSTALL (
    echo ERROR: Visual Studio not found.
    exit /b 1
)

:: Find latest MSVC toolset
set MSVC_TOOLS_DIR=%VSINSTALL%\VC\Tools\MSVC
for /f %%v in ('dir /b /ad "%MSVC_TOOLS_DIR%" ^| sort /r') do (
    set MSVC_VER=%%v
    goto :found_version
)

:found_version
if not defined MSVC_VER (
    echo ERROR: No MSVC toolset found in %MSVC_TOOLS_DIR%.
    exit /b 1
)

echo Using Visual Studio from %VSINSTALL%
echo Using MSVC version %MSVC_VER%

:: Set up environment with vcvars64.bat
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=%MSVC_VER%
if errorlevel 1 (
    echo ERROR: Failed to initialize VS build environment.
    exit /b 1
)

:: Run CMake configure and build
cmake --preset Release
if errorlevel 1 (
    echo ERROR: CMake configure failed.
    exit /b 1
)

cmake --build --preset Release
if errorlevel 1 (
    echo ERROR: CMake build failed.
    exit /b 1
)

echo Build completed successfully.
endlocal
