@echo off
setlocal

set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..\..\..\..\..") do set PROJECT_ROOT=%%~fI
set PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject
set LOG_DIR=%PROJECT_ROOT%\Saved\Logs
set HELPER_SCRIPT=%PROJECT_ROOT%\scripts\ue\editor\blueprint\helpers\set_blueprint_component_transform.py

call "%PROJECT_ROOT%\scripts\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

set "ALIS_BP_ASSET_PATH=/ProjectObject/Human/Hero/BP_Hero"
set "ALIS_BP_COMPONENT_VARIABLE_NAME=Mesh"
set "ALIS_BP_COMPONENT_OBJECT_NAME=CharacterMesh0"
set "ALIS_BP_RELATIVE_LOCATION=0,0,-90"
set "ALIS_BP_RELATIVE_ROTATION=0,-90,0"

echo Aligning BP_Hero CharacterMesh0 inherited defaults with native ProjectCharacter...
"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "%PROJECT_FILE%" ^
    -run=pythonscript ^
    -script="%HELPER_SCRIPT%" ^
    -unattended ^
    -nop4 ^
    -NoSound ^
    -NullRHI ^
    -CrashForUAT ^
    -log="%LOG_DIR%\fix_bp_hero_character_mesh_defaults.log" ^
    > "%LOG_DIR%\fix_bp_hero_character_mesh_defaults_stdout.log" 2>&1
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

REM Recompile Blueprints after the template edit so generated defaults reflect the saved asset state.
call "%PROJECT_ROOT%\scripts\ue\check\bp_compile.bat"
exit /b %ERRORLEVEL%
