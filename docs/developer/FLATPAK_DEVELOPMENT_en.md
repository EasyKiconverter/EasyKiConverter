# Flatpak Development Guide

This document provides a complete guide for compiling, testing, and managing configuration files for Flatpak builds locally.

## Table of Contents

- [Environment Setup](#environment-setup)
- [Flatpak Build](#flatpak-build)
- [Running and Testing](#running-and-testing)
- [Configuration File Management](#configuration-file-management)
- [Debugging Tips](#debugging-tips)
- [FAQ](#faq)

## Environment Setup

### Install Flatpak Builder

```bash
# Ubuntu/Debian
sudo apt-get install flatpak-builder

# Fedora
sudo dnf install flatpak-builder

# Arch Linux
sudo pacman -S flatpak-builder
```

### Add Flatpak Remotes

```bash
# Add Flathub (if not already added)
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

# Add KDE Platform 6.10 runtime
flatpak remote-add --if-not-exists kdeapps https://distribute.kde.org/kdeapps.flatpakrepo
```

### Install KDE Platform 6.10 Runtime and SDK

```bash
# Install runtime
flatpak install flathub org.kde.Platform//6.10

# Install SDK
flatpak install flathub org.kde.Sdk//6.10
```

## Flatpak Build

### Method 1: Using Flatpak Builder

```bash
# Navigate to project root
cd /path/to/EasyKiConverter

# Build Flatpak package (using local test manifest)
flatpak-builder --force-clean --repo=flatpak-repo build-flatpak ../../deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml

# Install the built package
flatpak --user remote-add --no-gpg-verify local-repo flatpak-repo
flatpak --user install local-repo io.github.tangsangsimida.easykiconverter

# Run
flatpak run io.github.tangsangsimida.easykiconverter
```

### Method 2: Using Project Script

```bash
# Check Flatpak build environment
python tools/python/flatpak_tool.py --check

# Build Flatpak
python tools/python/flatpak_tool.py --build

# Execute full workflow (check+build+install+run)
python tools/python/flatpak_tool.py --all --force

# Clean build directory
python tools/python/flatpak_tool.py --clean

# View app info
python tools/python/flatpak_tool.py --info
```

### Clean Build Cache

```bash
# Clean Flatpak build cache
rm -rf flatpak_build/
```

## Running and Testing

### Run Installed Flatpak App

```bash
# Basic run
flatpak run io.github.tangsangsimida.easykiconverter

# Run with debug info
flatpak run --devel io.github.tangsangsimida.easykiconverter

# Enable debug mode
flatpak run io.github.tangsangsimida.easykiconverter --debug

# Set language
flatpak run io.github.tangsangsimida.easykiconverter --language en
flatpak run io.github.tangsangsimida.easykiconverter --language zh_CN
```

### View Logs

```bash
# View app logs
flatpak run --command=sh io.github.tangsangsimida.easykiconverter

# View logs inside Flatpak environment
journalctl --user -u flatpak-io.github.tangsangsimida.easykiconverter
```

### Uninstall and Reinstall

```bash
# Uninstall app
flatpak --user uninstall io.github.tangsangsimida.easykiconverter

# Remove local repo
flatpak --user remote-delete local-repo

# Clean Flatpak cache
flatpak repair --user
```

## Configuration File Management

### Config File Location

**Flatpak app config file location differs from local app:**

```bash
# Flatpak app config location
~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json

# Local app config location
~/.config/EasyKiConverter/EasyKiConverter/config.json
```

### View Config File

```bash
# View current config
cat ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json | python3 -m json.tool

# Or use jq (if installed)
cat ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json | jq '.'
```

### Modify Config File

```bash
# Edit config file
nano ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json

# Or use vim
vim ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json
```

### Common Config Examples

```json
{
  "outputPath": "/home/username/Desktop",
  "libName": "easyeda_convertlib",
  "exportSymbol": true,
  "exportFootprint": true,
  "exportModel3D": true,
  "exportPreviewImages": false,
  "exportDatasheet": false,
  "overwriteExistingFiles": false,
  "darkMode": false,
  "language": "en",
  "windowWidth": 1664,
  "windowHeight": 936,
  "windowX": 0,
  "windowY": 0,
  "exitPreference": ""
}
```

### Reset Config File

```bash
# Backup current config
cp ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json ~/backup_config.json

# Delete config file (app will use defaults)
rm ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json

# Or reset to defaults
cat > ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json << EOF
{
  "outputPath": "/home/$(whoami)/Desktop",
  "libName": "easyeda_convertlib",
  "exportSymbol": true,
  "exportFootprint": true,
  "exportModel3D": true,
  "exportPreviewImages": false,
  "exportDatasheet": false,
  "overwriteExistingFiles": false,
  "darkMode": false,
  "language": "en",
  "windowWidth": -1,
  "windowHeight": -1,
  "windowX": -9999,
  "windowY": -9999,
  "exitPreference": ""
}
EOF
```

## Debugging Tips

### Enter Flatpak Sandbox

```bash
# Enter Flatpak app sandbox environment
flatpak run --command=sh io.github.tangsangsimida.easykiconverter

# Inside sandbox you can:
# - View filesystem
# - Check config files
# - Run debugging tools
ls -la ~/.var/app/io.github.tangsangsimida.easykiconverter/
```

### Using GDB Debugging

```bash
# Debug with GDB
flatpak run --devel --command=gdb io.github.tangsangsimida.easykiconverter

# Set breakpoints in GDB
(gdb) break main
(gdb) run
(gdb) backtrace
```

### View Permissions

```bash
# View app permissions
flatpak info --show-permissions io.github.tangsangsimida.easykiconverter

# Temporarily override permissions (for debugging only)
flatpak run --filesystem=home io.github.tangsangsimida.easykiconverter
```

### Performance Profiling

```bash
# Use perf for profiling
flatpak run --devel --command=perf io.github.tangsangsimida.easykiconverter record -g /tmp/perf.data
flatpak run --devel --command=perf io.github.tangsangsimida.easykiconverter report --stdio /tmp/perf.data
```

## FAQ

### Issue 1: Build Failed - Runtime Not Found

**Error:**
```
error: Remote 'kdeapps' not found
```

**Solution:**
```bash
# Add KDE runtime repo
flatpak remote-add --if-not-exists kdeapps https://distribute.kde.org/kdeapps.flatpakrepo

# Or install from Flathub
flatpak install flathub org.kde.Platform//6.10 org.kde.Sdk//6.10
```

### Issue 2: Build Failed - QXlsx Dependency Problem

**Error:**
```
error: Build failed
```

**Solution:**
The Flatpak config already has QXlsx built-in source configuration. Check QXlsx module config in `deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml`.

### Issue 3: Cannot Access Filesystem at Runtime

**Error:**
```
Permission denied
```

**Solution:**
```bash
# Grant access to home directory
flatpak override --filesystem=home io.github.tangsangsimida.easykiconverter

# Grant access to specific directory
flatpak override --filesystem=/home/username/Documents io.github.tangsangsimida.easykiconverter
```

### Issue 4: Config File Not Saved

**Possible Causes:**
- Flatpak app config file location differs from local app
- Permission issues

**Solution:**
```bash
# Check if config directory exists
ls -la ~/.var/app/io.github.tangsangsimida.easykiconverter/config/

# If not, create directory manually
mkdir -p ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/
```

### Issue 5: Language Switch Not Taking Effect After Restart

**Check config file:**
```bash
cat ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json | grep language
```

**Expected output:**
```json
"language": "en"
```

If language is set correctly but UI hasn't updated, translation file may not be loaded correctly. Check logs:
```bash
flatpak run --devel io.github.tangsangsimida.easykiconverter 2>&1 | grep -i language
```

### Issue 6: Out of Memory

**Error:**
```
error: Out of memory
```

**Solution:**
```bash
# Increase build cache size
flatpak-builder --force-clean --repo=flatpak-repo --keep-build-dirs build-flatpak ../../deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml

# Clean old build cache
rm -rf ~/.var/app/io.github.tangsangsimida.easykiconverter/cache/
```

## Advanced Tips

### Custom Build Options

```bash
# Enable debug build
flatpak-builder --force-clean --repo=flatpak-repo --build-args="--env=CMAKE_BUILD_TYPE=Debug" build-flatpak ../../deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml

# Enable tests
flatpak-builder --force-clean --repo=flatpak-repo --build-args="--env=EASYKICONVERTER_BUILD_TESTS=ON" build-flatpak ../../deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml
```

### Export as Single Bundle

```bash
# Export as single-file bundle
flatpak build-bundle flatpak-repo easykiconverter.flatpak io.github.tangsangsimida.easykiconverter

# Run bundle
flatpak install easykiconverter.flatpak
flatpak run io.github.tangsangsimida.easykiconverter
```

### Development Environment Setup

```bash
# Create development sandbox
flatpak build-init dev-sandbox org.kde.Sdk//6.10 org.kde.Platform//6.10

# Mount project directory
flatpak build --filesystem=home dev-sandbox

# Run in development sandbox
flatpak build dev-sandbox /path/to/easykiconverter/build/bin/easykiconverter
```

## Related Resources

- [Flatpak Official Documentation](https://docs.flatpak.org/)
- [KDE Flatpak Guide](https://community.kde.org/Guidelines_and_HOWTOs/Flatpak)
- [Project Flatpak Config](../../deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml)
- [Flatpak Manifest Format](https://docs.flatpak.org/en/latest/flatpak-manifest.html)

## Contributing

If you encounter issues during Flatpak build or usage:

1. Check the "FAQ" section of this document
2. Check project's [GitHub Issues](https://github.com/EasyKiconverter/EasyKiConverter/issues)
3. Submit a new Issue with detailed error info and logs

## Changelog

- 2026-03-15: Initial version with Flatpak development guide
- Includes environment setup, build, run, config file management and debugging tips
