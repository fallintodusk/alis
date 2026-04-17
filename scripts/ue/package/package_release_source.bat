@echo off
setlocal

REM Package Shipping build using UE source engine (<ue-path>).
REM Delegates to package_release.ps1 with -EngineRoot override.
REM All additional arguments are forwarded as-is.

powershell -ExecutionPolicy Bypass -File "%~dp0package_release.ps1" -EngineRoot "<ue-path>" -CreateReleaseArchive %*
exit /b %ERRORLEVEL%
