# Project Roadmap

This document separates released capabilities, development-branch capabilities, and future work so that development features are not mistaken for stable release features.

## Current Status

- **Latest stable release**: v3.1.13 (unchanged until the v3.1.14 Release is published)
- **Release preparation version**: v3.1.14
- **Release target commit**: `4ceeec67892e0427e5bd2ad5b1e11a816d9650c0`
- **Release branch**: `v3.1.14`
- **Current development branch**: `master` (source version 3.1.13)
- **Development status**: Continued work on the shared intermediate representation and multi-target export

The Chinese and English v3.1.14 changelogs, application metadata, and cross-platform CI validation are prepared. The stable-release status is determined by the `v3.1.14` tag and published Release created from this commit; before the tag exists, v3.1.14 must not be described as a published stable release.

The stable release currently focuses on KiCad export. The `master` branch contains the shared intermediate
representation (IR), Altium Designer library export, and Xpedition ASCII library export implementations,
but whether these capabilities are included in a stable package must be confirmed in the corresponding Release notes.

## Version Planning

### v3.0.0 - Core Feature Completion (Current Version)

**Goal**: Complete core features and improve user experience

**Core Architecture**:
- [x] Multi-stage pipeline parallelism implementation
- [x] Thread-safe bounded queues
- [x] Real-time progress feedback

**Export Functionality Enhancements**:
- [ ] Quick access to export destination folder after completion
- [x] Network request retry mechanism (implemented in v3.0.2)
- [x] Weak network resilience improvements (timeout retry, unified backoff strategy, NetworkWorker timeout protection) — completed in v3.0.5
- [x] 3D model path export options (relative/absolute) — completed in v3.1.8
  - [x] Relative path: Use KiCad project path + 3D model path
  - [x] Absolute path: Use export path + 3D model path
- [x] Export progress bar design — completed in v3.0.2
- [x] Export statistics design — completed in v3.0.2
- [ ] Export preview functionality (footprint, symbol, 3D model, manual PDF)

**Export Options Optimization**:
- [x] Overwrite and append export options
- [x] Export mode UI design optimization (clearer mode selection)
- [x] Library description export option (KiCad library description parameter) — completed in v3.1.7
- [ ] Footprint attribute enhancements (description, dual 3D model addresses, etc.)

**Data Parsing Improvements**:
- [~] Footprint symbol parser and exporter refinement
  - [x] Footprint symbol line width parsing
  - [ ] Other attribute parsing optimizations
- [ ] BOM table parsing functionality testing
- [ ] Special component export issue fixes (e.g., C7420375 position offset)

**User Interface Improvements**:
- [ ] Component list search functionality
- [~] Multi-language support (automatic based on system language) — CLI internationalization completed in v3.1.9, GUI translation files configured
- [ ] UI performance optimization and response speed improvements

**Development Tools and Processes**:
- [x] GitHub workflow configuration
  - [x] Automated code legal security review
  - [x] Automated testing — completed in v3.0.5
  - [x] Automated compilation and packaging — completed (Linux/Windows/macOS)
  - [ ] Automated version release
- [x] Contribution guidelines and project documentation improvements
- [x] Version management tool (manage_version.py) — completed in v3.0.5

### v3.1.0 - Performance Optimization and Stability Improvements

**Goal**: Optimize performance, improve stability, complete test coverage

**Main Features**:
- [x] Network request testing and optimization — completed in v3.0.5
- [x] Error handling improvements — completed in v3.0.5
- [x] Logging system optimization — completed in v3.0.5
- [x] UI automation testing — completed in v3.1.8
- [x] Integration testing improvements — completed in v3.1.8
- [ ] Performance testing and optimization (compare with Python version)
- [ ] Memory usage optimization

### v3.2.0 - Intermediate Representation and Multi-Target Export (In Development)

**Goal**: Improve multi-target export quality, diagnostics, and compatibility validation on top of the completed shared intermediate representation.

**Completed foundation**:
- [x] Create `src/core/ir/` with generic IR types, symbol, footprint, and 3D model structures
- [x] Create EasyEDA layer, pad-shape, and pin-type mapping tables
- [x] Build the shared symbol and footprint IR during import
- [x] Keep the KiCad exporters on the shared IR
- [x] Connect Altium SchLib/PcbLib exporters to the shared IR
- [x] Connect Xpedition ASCII symbol and footprint exporters to the shared IR

**Remaining work**:
- [ ] Complete Golden-file and real-EDA compatibility regression testing for each target
- [ ] Expand Altium and Xpedition format coverage and diagnostics
- [ ] Complete stable-release notes and a capability matrix for each target format
- [ ] Evaluate compatibility migration for legacy model serialization

## Feature Priorities

### High Priority

1. Performance optimization
2. Stability improvements
3. Test coverage completion
4. Error handling improvements
5. User experience optimization

### Medium Priority

1. New features addition
2. Advanced search
3. Component library management
4. Export preview functionality
5. Multi-language support

### Low Priority

1. Plugin system
2. Cloud sync
3. Mobile version
4. Enterprise edition
5. AI features

## Technical Debt

### Issues to Resolve

1. **Code Quality**
   - [ ] Increase code coverage to 80%+
   - [ ] Improve code review process
   - [ ] Unify code style

2. **Documentation**
   - [ ] Complete API documentation
   - [ ] Improve developer documentation
   - [ ] Update user manual

3. **Testing**
   - [ ] Unit test coverage
   - [ ] Integration testing improvements
   - [ ] Performance testing establishment

4. **Performance**
   - [ ] Startup speed optimization
   - [ ] Memory usage optimization
   - [ ] Network request optimization

## How to Participate

If you want to participate in project development:

1. Check the [Contributing Guide](../developer/CONTRIBUTING_en.md)
2. Choose a task you're interested in
3. Create an Issue to discuss your ideas
4. Submit a Pull Request

## Feedback and Suggestions

We welcome your feedback and suggestions:

- Submit issues on [GitHub Issues](https://github.com/EasyKiconverter/EasyKiConverter/issues)
- Participate in discussions on [GitHub Discussions](https://github.com/EasyKiconverter/EasyKiConverter/discussions)
- Contact project maintainers via email

## Changelog

This roadmap will be updated regularly. Please stay tuned for the latest updates.

## Disclaimer

This roadmap is for reference only. Actual development plans may be adjusted based on actual circumstances.
