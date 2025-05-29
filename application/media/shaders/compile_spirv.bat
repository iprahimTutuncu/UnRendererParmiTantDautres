@echo off
setlocal enabledelayedexpansion

:: Set paths
set GLSLC=glslc.exe
set SRC_DIR=sources\SPIRV
set OUT_DIR=compiled\SPIRV

:: Step 1: Clear the compiled output folder
if exist "%OUT_DIR%" (
    echo Deleting contents of %OUT_DIR%...
    del /q "%OUT_DIR%\*.*"
) else (
    mkdir "%OUT_DIR%"
)

:: Step 2: Compile each shader in the source folder
echo Compiling shaders from %SRC_DIR%...
for %%f in (%SRC_DIR%\*) do (
    set FILE=%%~nxf
    set EXT=%%~xf
    set OUT_FILE=%%~nxf.spv

    :: Detect shader stage by extension
    if /I "!EXT!"==".vert" (
        %GLSLC% "%%f" -o "%OUT_DIR%\!OUT_FILE!"
    ) else if /I "!EXT!"==".frag" (
        %GLSLC% "%%f" -o "%OUT_DIR%\!OUT_FILE!"
    ) else if /I "!EXT!"==".comp" (
        %GLSLC% "%%f" -o "%OUT_DIR%\!OUT_FILE!"
    ) else (
        echo Skipping unsupported file: %%f
    )
)

echo Done.
pause
