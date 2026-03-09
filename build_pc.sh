#!/bin/bash
# build_pc.sh - Build the Animal Crossing PC port (one command)
# Run from MSYS2 MINGW32 shell
#
# Prerequisites:
#   pacman -S mingw-w64-i686-gcc mingw-w64-i686-cmake mingw-w64-i686-SDL2 mingw-w64-i686-ninja
#
# Usage:
#   1. Place your Animal Crossing disc image in orig/GAFE01_00/
#   2. ./build_pc.sh
#   3. pc/build32/bin/AnimalCrossing.exe

set -e

if [ "$MSYSTEM" != "MINGW32" ]; then
    echo "Error: Must run from MSYS2 MINGW32 shell (not MINGW64 or MSYS)"
    echo "Open 'MSYS2 MINGW32' from your Start menu, then run this script again."
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/pc/build32"
BIN_DIR="$BUILD_DIR/bin"

# --- Find ROM image ---
ROM=""
for f in "$SCRIPT_DIR"/orig/GAFE01_00/*.{iso,gcm,rvz,wia,wbfs,ciso,gcz,nkit}; do
    if [ -f "$f" ]; then
        ROM="$f"
        break
    fi
done
if [ -z "$ROM" ]; then
    for f in "$SCRIPT_DIR"/orig/GAFE01_00/*.{ISO,GCM,RVZ,WIA,WBFS,CISO,GCZ,NKIT}; do
        if [ -f "$f" ]; then
            ROM="$f"
            break
        fi
    done
fi

if [ -z "$ROM" ]; then
    echo "Error: No ROM image found in orig/GAFE01_00/"
    echo "Place your Animal Crossing disc image (ISO/GCM/RVZ/WIA/WBFS/CISO/GCZ) there."
    exit 1
fi
echo "=== Found ROM: $(basename "$ROM") ==="

# --- Find Python ---
PYTHON=""
for cmd in python3 python; do
    if command -v "$cmd" > /dev/null 2>&1; then
        PYTHON="$cmd"
        break
    fi
done
# Check common Windows install paths if not found in MSYS2
if [ -z "$PYTHON" ]; then
    for p in \
        /c/Python3*/python.exe \
        /c/Users/*/AppData/Local/Programs/Python/Python3*/python.exe \
        /c/Program\ Files/Python3*/python.exe \
        /c/Program\ Files\ \(x86\)/Python3*/python.exe; do
        if [ -f "$p" ]; then
            PYTHON="$p"
            break
        fi
    done
fi
if [ -z "$PYTHON" ]; then
    echo "Error: Python 3 not found. Install Python 3 or add it to your PATH."
    exit 1
fi

# --- Step 1: Decomp build (generates .inc asset files from ROM) ---
if [ ! -d "$SCRIPT_DIR/build/GAFE01_00/include/assets" ]; then
    echo "=== Running decomp pipeline (configure.py + ninja) ==="
    echo "    This extracts assets from the ROM and generates ~16,000 .inc files."
    echo "    First run takes several minutes..."
    cd "$SCRIPT_DIR"
    $PYTHON configure.py
    ninja
fi

# --- Step 2: Extract runtime assets (archives, audio, etc.) ---
DTK="$SCRIPT_DIR/build/tools/dtk.exe"
if [ ! -f "$BIN_DIR/assets/files/COPYDATE" ]; then
    if [ ! -f "$DTK" ]; then
        echo "Error: dtk not found at $DTK"
        echo "The decomp pipeline (Step 1) should have downloaded it."
        exit 1
    fi
    echo "=== Extracting runtime assets ==="
    mkdir -p "$BIN_DIR/assets"
    "$DTK" disc extract "$ROM" "$BIN_DIR/assets"
fi

# --- Create runtime directories ---
mkdir -p "$BIN_DIR/texture_pack"
mkdir -p "$BIN_DIR/save"

# --- Step 3: CMake configure ---
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ ! -f Makefile ]; then
    echo "=== Configuring CMake ==="
    cmake .. -G "MinGW Makefiles"
fi

# --- Step 4: Build ---
echo "=== Building PC port ==="
mingw32-make -j$(nproc)

echo ""
echo "=== Build complete! ==="
echo "Run: pc/build32/bin/AnimalCrossing.exe"
