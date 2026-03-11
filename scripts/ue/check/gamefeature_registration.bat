@echo off
REM Check GameFeature Plugin Registration Status
REM This script verifies that all GameFeature plugins have valid configurations

echo ========================================
echo GameFeature Plugin Registration Check
echo ========================================
echo.

echo Checking plugin files...
echo.

REM Check if .uplugin files exist
echo [1/5] Verifying .uplugin files exist:
if exist "Plugins\GameFeatures\ProjectMenuCoreGF\ProjectMenuCoreGF.uplugin" (
    echo   [OK] ProjectMenuCoreGF.uplugin
) else (
    echo   [FAIL] ProjectMenuCoreGF.uplugin NOT FOUND
)

if exist "Plugins\GameFeatures\ProjectMenuExperienceGF\ProjectMenuExperienceGF.uplugin" (
    echo   [OK] ProjectMenuExperienceGF.uplugin
) else (
    echo   [FAIL] ProjectMenuExperienceGF.uplugin NOT FOUND
)
echo.

REM Check if GameFeatureData field exists in .uplugin files
echo [2/5] Checking GameFeatureData paths in .uplugin files:
findstr /C:"GameFeatureData" "Plugins\GameFeatures\ProjectMenuCoreGF\ProjectMenuCoreGF.uplugin" >nul
if %errorlevel%==0 (
    echo   [OK] ProjectMenuCoreGF has GameFeatureData field
    findstr "GameFeatureData" "Plugins\GameFeatures\ProjectMenuCoreGF\ProjectMenuCoreGF.uplugin"
) else (
    echo   [FAIL] ProjectMenuCoreGF MISSING GameFeatureData field
)

findstr /C:"GameFeatureData" "Plugins\GameFeatures\ProjectMenuExperienceGF\ProjectMenuExperienceGF.uplugin" >nul
if %errorlevel%==0 (
    echo   [OK] ProjectMenuExperienceGF has GameFeatureData field
    findstr "GameFeatureData" "Plugins\GameFeatures\ProjectMenuExperienceGF\ProjectMenuExperienceGF.uplugin"
) else (
    echo   [FAIL] ProjectMenuExperienceGF MISSING GameFeatureData field
)
echo.

REM Check if GameFeatureData .uasset files exist
echo [3/5] Verifying GameFeatureData .uasset files exist:
if exist "Plugins\GameFeatures\ProjectMenuCoreGF\Content\GameFeatureData\DA_ProjectMenuCore_GameFeature.uasset" (
    echo   [OK] DA_ProjectMenuCore_GameFeature.uasset exists
) else (
    echo   [FAIL] DA_ProjectMenuCore_GameFeature.uasset NOT FOUND
    echo   ^       Create this asset in Unreal Editor!
)

if exist "Plugins\GameFeatures\ProjectMenuExperienceGF\Content\GameFeatureData\GFD_MenuExperience.uasset" (
    echo   [OK] GFD_MenuExperience.uasset exists
) else (
    echo   [FAIL] GFD_MenuExperience.uasset NOT FOUND
    echo   ^       Create this asset in Unreal Editor!
)
echo.

REM Check plugin enablement in Alis.uproject
echo [4/5] Checking plugin enablement in Alis.uproject:
findstr /C:"ProjectMenuCoreGF" Alis.uproject >nul
if %errorlevel%==0 (
    echo   [OK] ProjectMenuCoreGF listed in Alis.uproject
) else (
    echo   [FAIL] ProjectMenuCoreGF NOT in Alis.uproject
)

findstr /C:"ProjectMenuExperienceGF" Alis.uproject >nul
if %errorlevel%==0 (
    echo   [OK] ProjectMenuExperienceGF listed in Alis.uproject
) else (
    echo   [FAIL] ProjectMenuExperienceGF NOT in Alis.uproject
)
echo.

REM Check Config/DefaultProject.ini
echo [5/5] Checking Menu experience configuration:
findstr /C:"ExperienceName=\"Menu\"" Config\DefaultProject.ini >nul
if %errorlevel%==0 (
    echo   [OK] Menu experience defined
    findstr "ExperienceName=\"Menu\"" Config\DefaultProject.ini
) else (
    echo   [FAIL] Menu experience NOT defined in Config\DefaultProject.ini
)
echo.

echo ========================================
echo Next Steps:
echo ========================================
echo.
echo 1. If GFD_MenuExperience.uasset is MISSING:
echo    - Open Alis.uproject in Unreal Editor
echo    - Follow: Plugins/GameFeatures/ProjectMenuExperienceGF/Content/README_CREATE_ASSET.md
echo    - Create the asset, configure it, and save
echo.
echo 2. If asset EXISTS but editor shows "missing game feature data":
echo    - Close Unreal Editor completely
echo    - Delete Intermediate/, Saved/, and Binaries/ folders (OPTIONAL - only if needed)
echo    - Reopen Alis.uproject
echo    - Editor will regenerate project files and reregister plugins
echo.
echo 3. Check editor Output Log for specific errors:
echo    - Window ^> Developer Tools ^> Output Log
echo    - Search for "GameFeature" or "ProjectMenuExperienceGF"
echo.
pause
