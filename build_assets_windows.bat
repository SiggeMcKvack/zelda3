@echo off
REM Build script for generating cross-platform compatible zelda3_assets.dat on Windows

echo ========================================
echo Zelda3 Asset Builder for Android
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    echo Please install Python 3.x from https://www.python.org/
    pause
    exit /b 1
)

REM Check if ROM exists
if not exist "zelda3.sfc" (
    echo ERROR: zelda3.sfc not found!
    echo Please place your Zelda 3 (USA) ROM in this directory
    echo Expected SHA256: 66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb
    pause
    exit /b 1
)

echo Found: zelda3.sfc
echo.

REM Check Python dependencies
echo Checking Python dependencies...
python -c "import PIL" 2>nul
if errorlevel 1 (
    echo Installing required Python packages...
    python -m pip install --upgrade pip pillow pyyaml
    if errorlevel 1 (
        echo ERROR: Failed to install Python dependencies
        pause
        exit /b 1
    )
)

echo ✓ Python dependencies OK
echo.

REM Run test to verify cross-platform compatibility
echo Testing cross-platform binary compatibility...
python assets\test_cross_platform.py
if errorlevel 1 (
    echo.
    echo WARNING: Compatibility test failed!
    echo The asset file may not work correctly on Android
    echo Do you want to continue anyway? (Press Ctrl+C to cancel)
    pause
)

echo.
echo ========================================
echo Building Assets...
echo ========================================
echo.

REM Run the asset extraction
cd assets
python restool.py --extract-from-rom
if errorlevel 1 (
    echo.
    echo ERROR: Asset extraction failed!
    cd ..
    pause
    exit /b 1
)
cd ..

echo.
echo ========================================
echo Verifying Asset File...
echo ========================================
echo.

REM Verify the generated file
python assets\diagnose_assets.py zelda3_assets.dat
if errorlevel 1 (
    echo.
    echo WARNING: Asset file verification failed!
    echo The file may not work correctly on Android
    pause
    exit /b 1
)

echo.
echo ========================================
echo ✓ Success!
echo ========================================
echo.
echo Asset file generated: zelda3_assets.dat
echo.
echo Next steps:
echo 1. Copy zelda3_assets.dat to your Android project
echo 2. Place it in: app/src/main/assets/
echo 3. Or upload to your app's download server
echo.
echo The file is now cross-platform compatible and should work on:
echo   - Windows (x86/x64)
echo   - Linux (x86/x64/ARM)
echo   - macOS (x86/ARM)
echo   - Android (ARM/ARM64/x86)
echo.

pause
