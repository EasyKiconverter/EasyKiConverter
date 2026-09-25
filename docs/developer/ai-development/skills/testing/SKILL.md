# Testing Skill

## 适用场景

当任务需要选择、执行或报告格式检查、构建、定向测试、全量测试或文档构建时使用。本 Skill 可能生成构建目录和日志，不修改 Git 历史，也不执行远端操作。

## 必读资料

- `AGENTS.md`
- `PROJECT_INSTRUCTIONS.md`
- `docs/developer/ai-development/TESTING_GUIDE.md`
- `docs/developer/ai-development/verification-policy.json`
- `docs/developer/TESTING_GUIDE.md`
- 与改动模块对应的测试和 workflow

## 执行步骤

1. 读取 `git status --short --branch` 和 `git diff --stat`，确认测试范围。
2. 依据 `verification-policy.json` 选择最低验证；C++、CMake、测试基础设施或 CI 改动默认扩大到构建和全量 CTest。
3. 使用项目 `.venv` 和专用 Qt，测试前设置 `QT_QPA_PLATFORM=offscreen`。
4. 优先运行定向测试，再运行完整构建和 `ctest --test-dir build --output-on-failure`。
5. 文档改动运行 `.venv/bin/python tools/python/build_docs.py --mkdocs`。
6. 运行适用的 C++/QML 格式检查和 `git diff --check`。
7. 分开记录已执行、未执行、环境阻塞和推断结果，并使用 Evidence Report 模板。

## 停止条件

- 测试依赖真实网络、用户目录或未安装的商业 EDA 软件时，改用 Mock/临时目录或明确报告未验证。
- 构建失败时先保留失败日志并定位根因，不通过修改断言、跳过测试或删除测试消除失败。

## 验收要求

- 新行为有回归测试。
- Qt/QML 测试在无头环境运行。
- 测试输出能指向失败用例和断言。
- 最终报告不能把局部测试通过描述成全量验证通过。
