# Documentation Directory

This directory contains all documentation for the EasyKiConverter project.

## Documentation Organization

Documentation is organized into three categories based on target audience:

### For Users (user/)

These documents help users understand what the project is and how to use it.

- [User Guide](user/USER_GUIDE_en.md) - Detailed usage instructions
- [Getting Started](user/GETTING_STARTED_en.md) - Quick start guide
- [FAQ](user/FAQ_en.md) - Frequently asked questions
- [Features](user/FEATURES_en.md) - Detailed feature descriptions
- [Debug Mode](user/DEBUG_MODE_en.md) - Debug mode configuration
- [Installation Codes](user/INSTALLATION_CODES_en.md) - Installer exit codes
- [Privacy Policy](user/PRIVACY_POLICY_en.md) - Privacy policy

### For Developers (developer/)

These documents help other developers (or your future self) understand the project, contribute, and maintain it.

- [Build Guide](developer/BUILD_en.md) - Build from source
- [Contributing Guide](developer/CONTRIBUTING_en.md) - How to contribute code
- [Architecture](developer/ARCHITECTURE_en.md) - Project architecture design
- [Xpedition Export](developer/XPEDITION_EXPORT_en.md) - Xpedition ASCII symbol and footprint export scope, limitations, and verification
- [Format Parsing Architecture](developer/PARSER_ARCHITECTURE_en.md) - Shared text parsing infrastructure, diagnostics, and format support boundaries
- [Testing Guide](developer/TESTING_GUIDE_en.md) - Testing architecture & Mocking strategy
- [BOM Parsing Guide](developer/BOM_PARSING_GUIDE_en.md) - BOM file parsing documentation
- [Coding Style](developer/CODING_STYLE_en.md) - Code style guidelines
- [Developer FAQ](developer/FAQ_en.md) - Technical issues, root cause analysis, regression prevention
- [Performance Baseline](developer/performance_baseline_en.md) - Performance benchmarks
- [i18n Implementation](developer/I18N_IMPLEMENTATION_SUMMARY.md) - Internationalization summary
- [Flatpak Development](developer/FLATPAK_DEVELOPMENT.md) - Flatpak packaging guide

### For Project & Strategy (project/)

These documents record the project's evolution and future direction, helping the team make correct strategic decisions.

- [Roadmap](project/ROADMAP_en.md) - Future development directions
- [Architecture Decision Records](project/adr/README_en.md) - Technical decision records
- [Logging Architecture](project/LOGGING_ARCHITECTURE.md) - Logging system design

### Historical Reports (archived)

These reports have been superseded by ADRs and are kept for reference only:

- [Weak Network Analysis](project/archive/WEAK_NETWORK_ANALYSIS_en.md) - v3.0.4 network resilience analysis
- [Performance Optimization Report](project/archive/PERFORMANCE_OPTIMIZATION_REPORT_en.md) - v3.0.0 optimization history

### Additional Documentation

- [CLI Usage](CLI_USAGE_en.md) - Command-line interface usage
- [API Documentation](api/index.md) - API reference documentation

## Documentation Languages

Most documents are available in both Chinese and English:
- Chinese documents use Chinese filenames (without _en suffix)
- English documents use English filenames (with _en suffix)

## How to Use Documentation

### If You Are a User

1. Start with [Getting Started](user/GETTING_STARTED_en.md)
2. Read [User Guide](user/USER_GUIDE_en.md) for detailed features
3. Check [FAQ](user/FAQ_en.md) for solutions to common problems

### If You Are a Developer

1. Read [Build Guide](developer/BUILD_en.md) to learn how to build the project
2. Review [Architecture](developer/ARCHITECTURE_en.md) to understand the project architecture
3. Read [Testing Guide](developer/TESTING_GUIDE_en.md) for testing standards and Mock implementation
4. Read [Developer FAQ](developer/FAQ_en.md) for known development issues and regression prevention
5. Read [Contributing Guide](developer/CONTRIBUTING_en.md) to learn how to contribute code

### If You Want to Understand Project Planning

1. Check [Roadmap](project/ROADMAP_en.md) to understand future directions
2. Read [Architecture Decision Records](project/adr/README_en.md) to understand technical decisions

## Documentation Principles

- **Docs as Code**: Documentation and code are in the same repository, using plain text formats like Markdown
- **Keep it Alive**: Outdated documentation is worse than no documentation
- **Write for the Audience**: Before writing any documentation, think clearly about who your readers are

## Contributing to Documentation

If you want to improve documentation:

1. Ensure documentation is consistent with code
2. Provide clear examples
3. Keep Chinese and English versions in sync
4. Submit a Pull Request

## Related Resources

- [Project Homepage](../README_en.md)
- [GitHub Issues](https://github.com/EasyKiconverter/EasyKiConverter/issues)
- [GitHub Discussions](https://github.com/EasyKiconverter/EasyKiConverter/discussions)
