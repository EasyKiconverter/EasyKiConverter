# Tools

This directory contains various development and build tools for the EasyKiConverter project.

## Structure

```
tools/
├── windows/          # Windows-specific tools
│   ├── format_code.bat           # C++ code formatter
│   ├── format_qml.bat            # QML code formatter
│   └── build_project.bat         # Python build tool wrapper
├── linux/            # Linux-specific tools
│   ├── build-packages.sh         # Linux packaging script
│   ├── fix-desktop-icon.sh       # Desktop icon fix script
│   └── BUILD_PACKAGES_README.md  # Linux packaging guide
├── macos/            # macOS-specific tools
└── python/           # Platform-independent Python tools
    ├── build_project.py          # Project build manager
    ├── manage_version.py         # Version management tool
    ├── manage_translations.py    # Translation file management tool
    ├── format_code.py            # Code formatter
    ├── check_comment_rate.py     # C++ comment coverage and policy checker
    ├── count_lines.py            # Code line counter
    ├── analyze_lines.py          # Code complexity analyzer
    ├── build_docs.py             # Documentation builder
    ├── check_env.py              # Environment dependency checker
    ├── convert_to_utf8.py        # File encoding converter
    └── fix_qml_translations.py   # QML translation fixer
```

## Linux Tools

### [build-packages.sh](../linux/build-packages.sh)

Linux packaging script supporting AppImage, DEB, RPM, Arch Linux, and Flatpak packaging.

**Quick Usage:**
```bash
# Build all packages
./tools/linux/build-packages.sh all

# Build only AppImage
./tools/linux/build-packages.sh appimage

# Build only DEB package
./tools/linux/build-packages.sh deb

# Specify version
VERSION=3.0.8 ./tools/linux/build-packages.sh all
```

### [fix-desktop-icon.sh](../linux/fix-desktop-icon.sh)

Linux desktop icon fix script to fix issues where user-level desktop files override system configuration.

**Quick Usage:**
```bash
sudo ./tools/linux/fix-desktop-icon.sh
```

## Windows Tools

### [format_code.bat](../windows/format_code.bat)

Automatically formats C++ and QML source code using clang-format.

**Usage:**
```bash
cd tools/windows
format_code.bat
```

**Requirements:**
- clang-format must be installed and available in PATH

### [format_qml.bat](../windows/format_qml.bat)

Automatically formats QML source code using qmlformat.

**Usage:**
```bash
cd tools/windows
format_qml.bat
```

**Requirements:**
- qmlformat must be installed and available in PATH

### [build_project.bat](../windows/build_project.bat)

Python build tool wrapper for Windows, supporting Debug/Release builds.

**Quick Usage:**
```bash
# Default Debug build
tools\windows\build_project.bat

# Release build
tools\windows\build_project.bat -t Release
```
- `.clang-format` configuration file should be in the project root

**What it does:**
- Formats all `.cpp`, `.h`, and `.qml` files in the project
- Uses the `.clang-format` configuration file in the project root
- Ensures consistent code style across the codebase

## Python Tools

### [build_project.py](../python/build_project.py)

Project build manager supporting environment check, CMake configuration, parallel compilation, and log management.

**Quick Usage:**
```bash
# Default Debug build
python tools/python/build_project.py

# Release build
python tools/python/build_project.py -t Release --parallel

# Environment check only
python tools/python/build_project.py --check
```

### [manage_version.py](../python/manage_version.py)

Automates project version synchronization across `vcpkg.json`, `CMakeLists.txt`, and `src/main.cpp`.

**Usage:**
```bash
# Check current version
python tools/python/manage_version.py --check

# Update version to 3.0.8
python tools/python/manage_version.py 3.0.8
```

**Requirements:**
- Python 3.x
- Recommended to run from the project root.

**Updated Files:**
- `vcpkg.json`: Updates the `"version-string"` field.
- `CMakeLists.txt`: Updates the default `VERSION_FROM_CI` and `qt_add_qml_module` version.
- `src/main.cpp`: Updates `app.setApplicationVersion`.

### [manage_translations.py](../python/manage_translations.py)

Automated Qt translation workflow (lupdate/lrelease), supporting automatic extraction and compilation of translation files.

**Quick Usage:**
```bash
# Update and compile all translation files
python tools/python/manage_translations.py --all

# Update translation source files only
python tools/python/manage_translations.py --update

# Compile translation files only
python tools/python/manage_translations.py --release
```

### [format_code.py](../python/format_code.py)

Unified code formatter supporting C++ and QML file formatting.

**Quick Usage:**
```bash
# Format all code
python tools/python/format_code.py --all

# Format C++ files only
python tools/python/format_code.py --cpp

# Format QML files only
python tools/python/format_code.py --qml

# Check format without modifying (CI mode)
python tools/python/format_code.py --check --all
```

### [count_lines.py](../python/count_lines.py)

Cross-platform code line counter, outputting project size in KLoC (thousands of lines of code).

**Quick Usage:**
```bash
# Count project code lines
python tools/python/count_lines.py

# JSON format output
python tools/python/count_lines.py --json
```

### [analyze_lines.py](../python/analyze_lines.py)

Code complexity analyzer to identify high-coupling risk files.

**Quick Usage:**
```bash
# Analyze entire project
python tools/python/analyze_lines.py

# Enable terminal hyperlinks (clickable files)
python tools/python/analyze_lines.py --link

# Analyze specific directory
python tools/python/analyze_lines.py tests
```

### [build_docs.py](../python/build_docs.py)

Documentation builder supporting Doxygen API docs and MkDocs site generation.

**Quick Usage:**
```bash
# Build all documentation
python tools/python/build_docs.py --all

# Generate API docs only
python tools/python/build_docs.py --doxygen

# Local preview
python tools/python/build_docs.py --serve
```

### [check_env.py](../python/check_env.py)

Development dependency environment checker, verifying CMake, compilers, Qt6, and other dependencies.

**Quick Usage:**
```bash
python tools/python/check_env.py
```

### [convert_to_utf8.py](../python/convert_to_utf8.py)

File encoding converter, converting files to UTF-8 encoding (without BOM).

**Quick Usage:**
```bash
# Convert entire src directory
python tools/python/convert_to_utf8.py src/
```

### [check_comment_rate.py](../python/check_comment_rate.py)

Checks C++ logical-unit comment coverage and can reject external-project references in source comments. With `--check-variables`, it also checks recognizable variable declarations; with `--check-params`, it checks that function Doxygen comments cover their `@param` entries; with `--check-doxygen`, it checks that every function definition has an adjacent multi-line Doxygen block containing `@brief`.

**Quick usage:**
```bash
# Check selected files with a 90% minimum coverage
python tools/python/check_comment_rate.py --threshold 90 src/core/utils/GeometryUtils.cpp

# Also check recognizable variable declarations
python tools/python/check_comment_rate.py --check-variables src/services/export/TempFileManager.cpp

# Also check that function parameters have Doxygen @param descriptions
python tools/python/check_comment_rate.py --check-params src/core/allegro/ExporterAllegroFootprint.cpp

# Also check that function Doxygen comments contain @brief
python tools/python/check_comment_rate.py --check-doxygen src/core/allegro/ExporterAllegroFootprint.cpp

# Also enforce the source-comment reference policy
python tools/python/check_comment_rate.py --threshold 100 --forbid-external-references \
  $(git diff --name-only master...HEAD -- '*.cpp' '*.h')
```

The tool only inspects source comments and never modifies files. External project names and phrases such as “reference implementation” are reported as policy violations.

### [fix_qml_translations.py](../python/fix_qml_translations.py)

QML translation fixer for fixing translation-related issues in QML files.

**Quick Usage:**
```bash
python tools/python/fix_qml_translations.py
```

## Command Line Help Support

All command-line tools must support `--help` or `-h` flag to display usage information.

### Implementation Requirements

| Tool Type | Recommended Implementation | Example |
|-----------|---------------------------|---------|
| Python scripts | Use `argparse` module | `parser = argparse.ArgumentParser(...)` |
| Windows batch | Check for `-h`, `--help`, `/h` | `if "%~1"=="--help" goto help` |
| Shell scripts | Check for `-h`, `--help` | `case "$1" in --help) show_help ;; esac` |

### Help Content Requirements

The help output should include:
1. **Tool name and description** - Brief explanation of what the tool does
2. **Usage format** - Command line syntax
3. **Arguments** - Description of each argument and default values
4. **Examples** - Common usage scenarios
5. **Requirements** - Required tools or libraries

### Example (Python)

```python
import argparse

def main():
    parser = argparse.ArgumentParser(
        description="Tool description",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python tool.py --option value
  python tool.py --help
        """
    )
    parser.add_argument("--input", help="Input file path")
    parser.add_argument("--verbose", action="store_true", help="Verbose output")
    args = parser.parse_args()
```

## Adding New Tools

When adding new tools, you must follow these requirements:

1. Create the appropriate subdirectory (`windows/`, `linux/`, `macos/`, or `python/`)
2. **Help Support**: All command-line tools **must** support `-h` and `--help` flags to display usage information. See "Command Line Help Support" section above for implementation details.
3. Add the tool script or executable
4. Update this README with documentation
5. Consider adding usage examples and requirements
