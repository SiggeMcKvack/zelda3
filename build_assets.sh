#!/bin/bash
# Build script for generating cross-platform compatible zelda3_assets.dat
# Works on Linux, macOS, and WSL

set -e  # Exit on error

echo "========================================"
echo "Zelda3 Asset Builder for Android"
echo "========================================"
echo ""

# Check if Python 3 is installed
if ! command -v python3 &> /dev/null; then
    echo "ERROR: python3 is not installed"
    echo "Please install Python 3.x"
    exit 1
fi

echo "Python version:"
python3 --version
echo ""

# Check if ROM exists
if [ ! -f "zelda3.sfc" ]; then
    echo "ERROR: zelda3.sfc not found!"
    echo "Please place your Zelda 3 (USA) ROM in this directory"
    echo "Expected SHA256: 66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb"
    exit 1
fi

echo "✓ Found: zelda3.sfc"
echo ""

# Check Python dependencies
echo "Checking Python dependencies..."
if ! python3 -c "import PIL" 2>/dev/null; then
    echo "Installing required Python packages..."
    python3 -m pip install --upgrade pip pillow pyyaml
fi

if ! python3 -c "import yaml" 2>/dev/null; then
    echo "Installing PyYAML..."
    python3 -m pip install pyyaml
fi

echo "✓ Python dependencies OK"
echo ""

# Run cross-platform test
echo "Testing cross-platform binary compatibility..."
if ! python3 assets/test_cross_platform.py; then
    echo ""
    echo "WARNING: Compatibility test failed!"
    echo "The asset file may not work correctly on Android"
    read -p "Do you want to continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo ""
echo "========================================"
echo "Building Assets..."
echo "========================================"
echo ""

# Run the asset extraction
cd assets
if ! python3 restool.py --extract-from-rom; then
    echo ""
    echo "ERROR: Asset extraction failed!"
    cd ..
    exit 1
fi
cd ..

echo ""
echo "========================================"
echo "Verifying Asset File..."
echo "========================================"
echo ""

# Verify the generated file
if ! python3 assets/diagnose_assets.py zelda3_assets.dat; then
    echo ""
    echo "WARNING: Asset file verification failed!"
    echo "The file may not work correctly on Android"
    exit 1
fi

echo ""
echo "========================================"
echo "✓ Success!"
echo "========================================"
echo ""
echo "Asset file generated: zelda3_assets.dat"
echo ""
echo "File size: $(du -h zelda3_assets.dat | cut -f1)"
echo ""
echo "Next steps:"
echo "1. Copy zelda3_assets.dat to your Android project"
echo "2. Place it in: app/src/main/assets/"
echo "3. Or upload to your app's download server"
echo ""
echo "The file is now cross-platform compatible and should work on:"
echo "  - Windows (x86/x64)"
echo "  - Linux (x86/x64/ARM)"
echo "  - macOS (x86/ARM)"
echo "  - Android (ARM/ARM64/x86)"
echo ""
