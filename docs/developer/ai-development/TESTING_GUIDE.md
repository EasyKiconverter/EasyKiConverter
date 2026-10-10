# AI 测试选择指南

[English version](TESTING_GUIDE_en.md)

本页是 AI 任务的验证选择表；最低验证集合以机器可读的[验证策略](verification-policy.json)为准，详细测试架构、Mock 约束和测试编写规则以 [测试开发指南](../TESTING_GUIDE.md) 为准。

## 变更到验证项

| 改动类型 | 最小验证 | 需要补充的验证 |
| --- | --- | --- |
| C++/头文件 | `.venv/bin/python tools/python/format_code.py --cpp --check` | 构建、相关定向测试、`QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure` |
| Importer/Parser | C++ 格式检查 | 本地 fixture、空/损坏/非法字段/缺失关联/单位和坐标测试，必要时全量 CTest |
| Exporter/IR | C++ 格式检查 | golden 或结构回读、诊断和引用完整性测试、全量 CTest |
| QML/UI | `.venv/bin/python tools/python/format_code.py --qml --check` | 构建 `test_ui`，设置 `QT_QPA_PLATFORM=offscreen` 后运行 UI 定向测试 |
| Python 工具 | `.venv/bin/python -m unittest discover -s tests/python -p 'test_*.py'` | 失败输入、路径、编码和命令行行为测试 |
| CMake/依赖/工作流 | YAML/配置语法检查 | 项目构建、受影响测试、工作流实际运行；不能只依赖本地解析 |
| 文档/MkDocs | `.venv/bin/python tools/python/build_docs.py --mkdocs` | 链接、双语互链、Mermaid 和导航检查 |

## 通用本地流程

将 `PROJECT_QT_ROOT` 替换为本机的项目专用 Qt 6.6+ 安装目录；Windows/macOS 的目录结构和环境变量配置请参阅[构建指南](../BUILD.md)。

```bash
export PROJECT_QT_ROOT=/path/to/Qt/6.10.2/gcc_64
export Qt6_DIR="$PROJECT_QT_ROOT/lib/cmake/Qt6"
export CMAKE_PREFIX_PATH="$PROJECT_QT_ROOT"
export PATH="$PROJECT_QT_ROOT/bin:$PATH"
export QT_QPA_PLATFORM=offscreen

.venv/bin/python tools/python/build_project.py -q "$PROJECT_QT_ROOT" -j 6
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
.venv/bin/python tools/python/build_docs.py --mkdocs
git diff --check
```

格式检查分开执行：

```bash
.venv/bin/python tools/python/format_code.py --cpp --check
.venv/bin/python tools/python/format_code.py --qml --check
```

## 报告规则

- 明确区分已执行、未执行、环境阻塞和推断结果。
- 变更分类器只选择 CI 范围，不替代本表和[验证策略](verification-policy.json)定义的模块级验证。
- 不能把本地构建通过写成 Windows/macOS/ARM 或商业 EDA 实机已验证。
- 网络相关测试不得访问真实服务；使用 `tests/common/MockNetworkClient.hpp` 或项目已有 fake。
- 解析器和导出器新增功能必须带最小回归测试；如果只能人工验证，要记录原因和剩余风险。
