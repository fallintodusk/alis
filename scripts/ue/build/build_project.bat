@echo off
REM Wrapper to keep legacy entry point but use UE_PATH from scripts/config/ue_path.conf
setlocal

REM Resolve project root for logs
pushd "%~dp0..\..\.."
set PROJECT_ROOT=%CD%
popd

set LOG=%PROJECT_ROOT%\Saved\Logs\build_project.log
if not exist "%PROJECT_ROOT%\Saved\Logs" mkdir "%PROJECT_ROOT%\Saved\Logs"

call "%~dp0build.bat" AlisEditor Development -WaitMutex -NoHotReload > "%LOG%" 2>&1
exit /b %ERRORLEVEL%
