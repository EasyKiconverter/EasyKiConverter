# Arch Linux Packaging

This directory contains the PKGBUILD for building EasyKiConverter on Arch Linux.

## Building from Source

### Prerequisites

Install the required build dependencies:

```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-quickcontrols2 qt6-svg qt6-shadertools qt6-tools zlib
```

### Build and Install

```bash
# Clone the AUR repository or use this PKGBUILD directly
git clone https://aur.archlinux.org/easykiconverter.git
cd easykiconverter

# Build the package
makepkg -si

# Or build without installing
makepkg -s
```

### Using the PKGBUILD from this Repository

```bash
cd deploy/archlinux

# Build the package
makepkg -si

# Or build without installing
makepkg -s
```

## AUR Submission

To submit this package to AUR:

1. Create an AUR account at https://aur.archlinux.org/
2. Set up SSH keys for AUR
3. Clone the AUR repository:
   ```bash
   git clone ssh://aur@aur.archlinux.org/easykiconverter.git
   cd easykiconverter
   ```
4. Copy the PKGBUILD and .SRCINFO files
5. Update the sha256sums (run `updpkgsums` or use `SKIP` for development)
6. Commit and push:
   ```bash
   git add PKGBUILD .SRCINFO
   git commit -m "Update to version 3.1.11"
   git push
   ```

## Dependencies

### Runtime Dependencies

- `qt6-base` - Qt 6 base modules
- `qt6-declarative` - Qt Quick/QML engine
- `qt6-quickcontrols2` - Qt Quick Controls 2
- `qt6-svg` - SVG support
- `qt6-shadertools` - Shader tools for Qt Quick
- `zlib` - Compression library

### Optional Dependencies

- `qt6-5compat` - For some legacy QML components

### Build Dependencies

- `cmake` - Build system
- `gcc` - C++ compiler
- `git` - Version control
- `ninja` - Build tool (optional, can use make)
- `qt6-tools` - Qt 6 development tools

## Troubleshooting

### Qt Version Issues

If you encounter Qt version conflicts, ensure you're using the system Qt 6 packages:

```bash
# Check installed Qt version
qmake6 --version

# List installed Qt packages
pacman -Qs qt6
```

### Build Failures

If the build fails, try:

1. Clean the build directory:
   ```bash
   rm -rf src/
   ```

2. Ensure all dependencies are installed:
   ```bash
   sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-quickcontrols2 qt6-svg qt6-shadertools qt6-tools zlib
   ```

3. Check the build logs for specific errors

## Links

- [AUR Package Page](https://aur.archlinux.org/packages/easykiconverter)
- [GitHub Repository](https://github.com/EasyKiconverter/EasyKiConverter)
- [Arch Linux Packaging Standards](https://wiki.archlinux.org/title/Arch_package_guidelines)
