# AI Testing Guide

[中文版](TESTING_GUIDE.md)

This page helps an AI Agent select verification. The authoritative testing architecture, mock policy, and test-writing rules remain in the [testing guide](../TESTING_GUIDE_en.md).

## Change-to-verification matrix

| Change type | Minimum check | Additional verification |
| --- | --- | --- |
| C++/headers | `.venv/bin/python tools/python/format_code.py --cpp --check` | Build, focused tests, and `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure` |
| Importer/parser | C++ format check | Local fixtures, empty/corrupt/invalid-field/missing-association/unit/coordinate tests, and full CTest when appropriate |
| Exporter/IR | C++ format check | Golden or structural round-trip checks, diagnostics/reference-integrity tests, and full CTest |
| QML/UI | `.venv/bin/python tools/python/format_code.py --qml --check` | Build `test_ui` and run focused UI tests with `QT_QPA_PLATFORM=offscreen` |
| Python tools | `.venv/bin/python -m unittest discover -s tests/python -p 'test_*.py'` | Failure-input, path, encoding, and CLI behavior tests |
| CMake/dependencies/workflows | YAML/config syntax checks | Project build, affected tests, and an actual workflow run; local parsing is not enough |
| Documentation/MkDocs | `.venv/bin/python tools/python/build_docs.py --mkdocs` | Links, bilingual links, Mermaid, and navigation checks |

## Common local flow

```bash
export Qt6_DIR=/home/dennis/software/QT/6.10.2/gcc_64/lib/cmake/Qt6
export CMAKE_PREFIX_PATH=/home/dennis/software/QT/6.10.2/gcc_64
export PATH=/home/dennis/software/QT/6.10.2/gcc_64/bin:$PATH
export QT_QPA_PLATFORM=offscreen

.venv/bin/python tools/python/build_project.py --test -q /home/dennis/software/QT/6.10.2/gcc_64 -j 6
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
.venv/bin/python tools/python/build_docs.py --mkdocs
git diff --check
```

Run format checks separately:

```bash
.venv/bin/python tools/python/format_code.py --cpp --check
.venv/bin/python tools/python/format_code.py --qml --check
```

## Reporting rules

- Distinguish executed, not executed, environment-blocked, and inferred results.
- A local build does not prove Windows/macOS/ARM or commercial EDA validation.
- Network tests must not access real services; use `tests/common/MockNetworkClient.hpp` or an existing fake.
- New parser and exporter behavior requires a minimal regression test. If only manual validation is possible, record why and the remaining risk.
