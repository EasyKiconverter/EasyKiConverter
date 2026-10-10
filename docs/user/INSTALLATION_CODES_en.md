# Installation Exit Codes - EasyKiConverter

This document describes the installation program exit codes for EasyKiConverter in various installation scenarios, for use with Microsoft Store app distribution.

## Installation Program Type

EasyKiConverter uses **Inno Setup** to create Windows installers (.exe format).

## Silent Installation Parameters

### Standard Silent Installation
```
/SILENT
```

### Very Silent Installation (no windows)
```
/VERYSILENT
```

### Without Desktop Shortcut
```
/SILENT /MERGETASKS=""
```

### Full Installation Parameters
```
/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-
```

## Installation Exit Codes

| Exit Code | Meaning | Description |
|-----------|---------|-------------|
| 0 | Installation Successful | Installation completed successfully |
| 1 | Setup Failed | Setup failed to initialize |
| 2 | User Cancelled | User clicked Cancel before installation started or chose "No" on the opening message box |
| 3 | Fatal Error | A fatal error occurred while preparing to move to the next installation phase |
| 4 | Installation Error | A fatal error occurred during the actual installation process |
| 5 | User Aborted | User clicked Cancel during installation or chose Abort at an Abort-Retry-Ignore box |
| 6 | Forcefully Terminated | Setup process was forcefully terminated by the debugger |
| 7 | Cannot Proceed | Preparing to Install stage determined that Setup cannot proceed |
| 8 | Cannot Proceed, Restart Required | Preparing to Install stage determined that Setup cannot proceed and system needs to be restarted |

## Standard Installation Scenarios

**Note**: Inno Setup provides a limited set of exit codes. Different scenarios may return the same code value. The following is the mapping for each scenario:

### 1. User Cancelled Installation
- **Exit Code**: 2
- **Description**: User clicked Cancel before installation started or chose "No" on the opening message box
- **Scenario**: User clicked the "Cancel" button in the installation wizard or chose "No" in the confirmation dialog

### 2. Application Already Exists
- **Exit Code**: 7
- **Description**: Preparing to Install stage detected that the application already exists
- **Scenario**: Application detected at target location but installation continues (overwrite)
- **Note**: Although overwrite installation will succeed, code 7 is used to distinguish the scenario

### 3. Installation Already in Progress
- **Exit Code**: 3
- **Description**: A fatal error occurred while preparing to move to the next installation phase
- **Scenario**: Memory shortage or insufficient Windows resources

### 4. Disk Space Full
- **Exit Code**: 4
- **Description**: A fatal error occurred during the actual installation process
- **Scenario**: System disk has less than required space (approximately 200MB)

### 5. Restart Required
- **Exit Code**: 8
- **Description**: Preparing to Install stage determined that Setup cannot proceed and system needs to be restarted
- **Scenario**: Some files or system components need to be activated after reboot

### 6. Network Failure
- **Exit Code**: 4
- **Description**: A fatal error occurred during the actual installation process
- **Scenario**: Network interruption while downloading dependencies
- **Note**: Shares exit code 4 with disk space full

### 7. Package Rejected During Installation
- **Exit Code**: 7
- **Description**: Preparing to Install stage determined that Setup cannot proceed
- **Scenario**: Security policy enabled on device, insufficient permissions or security software blocking installation
- **Note**: Shares exit code 7 with application already exists

### 8. Installation Successful
- **Exit Code**: 0
- **Description**: Installation program completed successfully
- **Scenario**: Normal installation process completed successfully (including overwrite installation)

## Other Installation Failure Scenarios

### Insufficient Permissions
- **Exit Code**: 7
- **Description**: Preparing to Install stage determined that Setup cannot proceed
- **Scenario**: Running installer as a standard user

### File in Use
- **Exit Code**: 4
- **Description**: A fatal error occurred during the actual installation process
- **Scenario**: Attempting to install while application is running

### Corrupted Installer Package
- **Exit Code**: 1
- **Description**: Setup failed to initialize
- **Scenario**: Incomplete download or modified installer

### Missing Dependencies
- **Exit Code**: 7
- **Description**: Preparing to Install stage determined that Setup cannot proceed
- **Scenario**: Visual C++ Redistributable not installed on system

### Missing Dependencies (Detected During Installation)
- **Exit Code**: 4
- **Description**: A fatal error occurred during the actual installation process
- **Scenario**: Required runtime environment not found during installation process

## Installer Information

- **Name**: EasyKiConverter Setup
- **Version**: 3.1.4
- **Architecture**: x64 or ARM64 (select according to the package filename)
- **Type**: Inno Setup (.exe)
- **Size**: Approximately 100-150 MB
- **Signature**: Includes digital signature

## System Requirements

- **Operating System**: Windows 10/11 (64-bit); ARM64 packages require Windows on Arm
- **Permissions**: Administrator privileges required
- **Disk Space**: Minimum 200MB
- **Network**: Not required (unless downloading from network)

## Technical Support

If you encounter installation issues, please contact:

- **GitHub Issues**: https://github.com/EasyKiconverter/EasyKiConverter/issues
- **Documentation**: https://easykiconverter.github.io/EasyKiConverter/
- **Email**: Please refer to developer contact information

## Reference Documentation

- [Inno Setup Official Documentation](https://jrsoftware.org/isinfo.php)
- [Inno Setup Exit Codes](https://jrsoftware.org/ishelp/topic_setupexitcodes.htm)
