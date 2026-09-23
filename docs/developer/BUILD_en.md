# Build Guide

This document provides detailed instructions for building the EasyKiConverter project locally.

## Prerequisites

### Operating System

- Windows 10/11 (Recommended)
- macOS 10.15+
- Ubuntu 20.04+ or other Linux distributions

### Required Software

#### Qt Framework

- **Version**: Qt 6.6 or higher (Recommended Qt 6.10.2)
- **Required Modules**:
  - Qt Quick
  - Qt Network
  - Qt Core
  - Qt Gui
  - Qt Widgets
  - Qt Quick Controls 2

**Download**: https://www.qt.io/download

#### CMake

- **Version**: CMake 3.16 or higher
- **Download**: https://cmake.org/download/

#### Compiler

**Windows**:
- MinGW 13.10 (Recommended)
- Or MSVC 2019+

**macOS**:
- Xcode 12+ (includes Clang)

**Linux**:
- GCC 9+ or Clang 10+

#### zlib

- Usually included with Qt on Windows
- Install via package manager on Linux:
  ```bash
  sudo apt-get install zlib1g-dev
  ```

## Installation Instructions

### Project-local development environment (Linux)

This project does not use the system Qt installation and does not install project Python packages into system Python. Use the repository-local `.venv` and the standalone Qt 6.10.2 installation.

```bash
# Create and activate the project virtual environment
python3 -m venv .venv
source .venv/bin/activate

# Install Python build helpers into the project environment
python -m pip install aqtinstall==3.2.1

# Use the project-specific Qt (not /usr/lib system Qt)
export Qt6_DIR=/home/dennis/software/QT/6.10.2/gcc_64/lib/cmake/Qt6
export CMAKE_PREFIX_PATH=/home/dennis/software/QT/6.10.2/gcc_64
export PATH=/home/dennis/software/QT/6.10.2/gcc_64/bin:$PATH
```

`format_code.py` automatically locates the project Qt `qmlformat` from `tools/config/build_config.json`; the `PATH` export is still recommended when invoking Qt tools directly.

Verify the Qt version:

```bash
qmake -query QT_VERSION       # must be 6.10.2
```

### Windows

#### 1. Install Qt

1. Download Qt Online Installer from https://www.qt.io/download
2. Run the installer and select Qt 6.10.2
3. Select MinGW 13.10 compiler
4. Select required modules:
   - Qt Quick
   - Qt Network
   - Qt Core
   - Qt Gui
   - Qt Widgets
   - Qt Quick Controls 2

#### 2. Install CMake

1. Download CMake from https://cmake.org/download/
2. Run the installer
3. Add CMake to system PATH

#### 3. Install MinGW (if not included with Qt)

1. Download MinGW-w64 from https://www.mingw-w64.org/
2. Extract to a directory (e.g., C:\mingw64)
3. Add bin directory to system PATH

Windows release builds support both architectures:

- x86_64: Qt `win64_msvc2022_64`, vcpkg triplet `x64-windows`; release filenames use `windows-x86_64`
- ARM64: Windows 11 on Arm, Qt `win64_msvc2022_arm64`, vcpkg triplet `arm64-windows`

The ARM64 release packages are built on the GitHub Actions `windows-11-arm` runner and use the `windows-arm64` filename marker for portable, installer, and MSIX packages. x86_64 packages use `windows-x86_64`; the MSIX manifest still uses the Windows-required `x64` architecture value internally.

### LoongArch64

LoongArch64 does not currently have an executable GitHub Actions workflow or official release package. GitHub-hosted runners do not support LoongArch64, and the project does not yet have a usable third-party native CI or x86_64/ARM64 cross-compilation toolchain, so no non-runnable workflow configuration is kept in the repository.

Once usable build infrastructure is available, add an independent Qt, toolchain, test, and `loongarch64` packaging workflow.

### macOS

#### 1. Install Qt

Download Qt 6.10.2 or newer from https://www.qt.io/download and install it into a project-specific directory. Do not use Homebrew Qt, `/usr/local/Qt`, or the system Qt installation. Before configuring the project, set `Qt6_DIR`, `CMAKE_PREFIX_PATH`, and `PATH` to that standalone Qt directory.

#### 2. Install CMake

```bash
brew install cmake
```

#### 3. Install Xcode Command Line Tools

```bash
xcode-select --install
```

### Linux (Ubuntu)

#### 1. Install Qt 6

```bash
sudo apt-get update
sudo apt-get install cmake ninja-build build-essential zlib1g-dev libxkbcommon-dev libxkbcommon-x11-dev

# Install project-specific Qt 6.10.2 using the project-local Qt instructions above.
# Do not use the system Qt under /usr/lib.
```

#### 2. Install CMake

```bash
sudo apt-get install cmake
```

#### 3. Install Compiler

```bash
sudo apt-get install build-essential
```

#### 4. Install zlib

```bash
sudo apt-get install zlib1g-dev
```

### Linux (Arch Linux)

```bash
# Install compiler and build dependencies. Qt must come from the standalone
# project-specific installation described above, not from system packages.
sudo pacman -S --needed base-devel cmake ninja zlib
```

## Building the Project

### Clone the Repository

```bash
git clone https://github.com/EasyKiconverter/EasyKiConverter.git
cd EasyKiConverter
```

### Windows + MinGW

```bash
# Create build directory
mkdir build
cd build

# Configure project
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/mingw_64"

# Build project (Debug version)
cmake --build . --config Debug

# Build project (Release version)
cmake --build . --config Release

# Run application
./bin/EasyKiConverter.exe
```

### macOS

```bash
# Set the project-specific Qt root (replace with the actual install path).
export PROJECT_QT_ROOT=/path/to/project/Qt/6.10.2/macos
export Qt6_DIR="$PROJECT_QT_ROOT/lib/cmake/Qt6"
export CMAKE_PREFIX_PATH="$PROJECT_QT_ROOT"
export PATH="$PROJECT_QT_ROOT/bin:$PATH"

# Create build directory
mkdir build
cd build

# Configure project
cmake .. -DCMAKE_PREFIX_PATH="$PROJECT_QT_ROOT"

# Build project (Debug version)
cmake --build . --config Debug

# Build project (Release version)
cmake --build . --config Release

# Run application
./bin/EasyKiConverter
```

### Linux

```bash
# Create build directory
mkdir build
cd build

# Configure project
cmake .. -DCMAKE_PREFIX_PATH="/home/dennis/software/QT/6.10.2/gcc_64"

# Build project (Debug version)
cmake --build . --config Debug

# Build project (Release version)
cmake --build . --config Release

# Run application
./bin/EasyKiConverter
```

## Build Options

### Enable Debug Export

Enable debug export for symbol and footprint data:

```bash
cmake .. -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=ON
```

### Disable Debug Export

Disable debug export (default is enabled):

```bash
cmake .. -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=OFF
```

### Build Type

Specify build type:

```bash
# Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build
cmake .. -DCMAKE_BUILD_TYPE=Release

# RelWithDebInfo build
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### Parallel Compilation

Speed up compilation on multi-core systems:

```bash
# Use all available cores
cmake --build . --parallel

# Use specific number of cores
cmake --build . --parallel 4
```

## Using Qt Creator

1. Open Qt Creator
2. Select "Open File or Project"
3. Select `CMakeLists.txt` in the project root
4. Configure build kit:
   - Select Qt version (Qt 6.10.2 or higher)
   - Select compiler (MinGW, Clang, or GCC)
5. Click "Configure Project"
6. Click "Build" button (or press Ctrl+B)
7. Click "Run" button (or press F5) to launch the application

## Build Output

After successful build, the executable files are located at:

- **Debug version**: `build/bin/EasyKiConverter.exe` (Windows) or `build/bin/EasyKiConverter` (macOS/Linux)
- **Release version**: `build/bin/EasyKiConverter.exe` (Windows) or `build/bin/EasyKiConverter` (macOS/Linux)

## Cleaning Build

```bash
cd build
cmake --build . --target clean
```

Or remove the entire build directory:

```bash
rm -rf build
```

## Common Issues

### Issue: Qt not found

**Error**: `Could not find Qt6`

**Solution**:
1. Verify Qt installation path
2. Set CMAKE_PREFIX_PATH correctly:
   ```bash
   cmake .. -DCMAKE_PREFIX_PATH="/path/to/Qt/6.10.2/compiler"
   ```

### Issue: zlib not found

**Error**: `Could not find ZLIB`

**Solution**:
- Windows: Qt usually includes zlib, ensure Qt path is correct
- Linux: Install zlib development package:
  ```bash
  sudo apt-get install zlib1g-dev
  ```

### Issue: QML module not found

**Error**: `module "EasyKiconverter_Cpp_Version" is not installed`

**Solution**:
1. Ensure CMakeLists.txt is configured correctly
2. Clean and rebuild:
   ```bash
   rm -rf build
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

### Issue: Linker errors

**Error**: `undefined reference to...`

**Solution**:
1. Ensure all required Qt modules are linked in CMakeLists.txt
2. Clean and rebuild the project
3. Check compiler compatibility

### Issue: Application crashes on startup

**Solution**:
1. Run in Debug mode to get stack trace
2. Check Qt plugin paths
3. Verify all required DLLs are in PATH (Windows)

## Development Build

For development, it's recommended to build in Debug mode with debug export enabled:

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=ON
cmake --build . --config Debug
```

## Release Build

For release builds, use Release mode:

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Deployment

### Windows Deployment

Use `windeployqt` to package the application:

```bash
cd build/bin
windeployqt EasyKiConverter.exe
```

This will copy all required Qt DLLs and plugins to the application directory.

### macOS Deployment

Create an application bundle:

```bash
cd build
cmake --install . --prefix /path/to/install
```

### Linux Deployment

Create a Debian package or AppImage:

```bash
# Using cpack
cpack -G DEB
```

## Next Steps

After successful build, refer to:
- [Getting Started Guide](../user/GETTING_STARTED_en.md) - Learn how to use the application
- [Features Documentation](../user/FEATURES_en.md) - Understand available features
- [Architecture Documentation](ARCHITECTURE_en.md) - Learn about project architecture

## Support

If you encounter any issues during the build process, please:
1. Check this documentation for solutions
2. Search existing GitHub Issues
3. Create a new issue with detailed information:
   - Operating system and version
   - Qt version
   - CMake version
   - Compiler version
   - Complete error message
   - Steps to reproduce
