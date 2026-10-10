#!/bin/bash
# 同步 GitHub Labels 到仓库
# 用法: .github/sync-labels.sh

set -e

REPO="EasyKiconverter/EasyKiConverter"

echo "=========================================="
echo "  同步 GitHub Labels"
echo "  Repository: $REPO"
echo "=========================================="
echo ""

# 删除旧标签（保留 GitHub 默认标签）
echo ">>> 删除旧标签..."
for label in "core" "easyeda" "kicad" "utils" "ui" "pyqt6" "ui-utils" "widgets" "workers" "build" "tests" "github-actions" "feature" "performance" "ui-improvement" "refactor" "dependencies" "test-enhancement" "models" "services" "ci" "resources" "docs"; do
  gh label delete "$label" --repo "$REPO" --yes 2>/dev/null || true
done

echo ""
echo ">>> 创建新标签..."

# 类型标签
gh label create "bug / 缺陷" --color "d73a4a" --description "Something isn't working / 功能异常" --repo "$REPO" --force
gh label create "feature / 新功能" --color "0e8a16" --description "New feature request / 新功能请求" --repo "$REPO" --force
gh label create "enhancement / 功能增强" --color "1d76db" --description "Improve existing functionality / 改进现有功能" --repo "$REPO" --force
gh label create "docs / 文档" --color "0075ca" --description "Documentation improvements / 文档改进" --repo "$REPO" --force
gh label create "refactor / 重构" --color "e4e669" --description "Code refactoring / 代码重构" --repo "$REPO" --force
gh label create "test / 测试" --color "d876e3" --description "Testing related / 测试相关" --repo "$REPO" --force
gh label create "ci / 持续集成" --color "fda67a" --description "CI/CD workflows / 持续集成工作流" --repo "$REPO" --force
gh label create "build / 构建" --color "a6c28c" --description "Build configuration / 构建配置" --repo "$REPO" --force
gh label create "deps / 依赖" --color "2ae51d" --description "Dependency updates / 依赖更新" --repo "$REPO" --force
gh label create "performance / 性能" --color "fbca04" --description "Performance optimization / 性能优化" --repo "$REPO" --force

# 模块标签
gh label create "core / 核心" --color "b60205" --description "Core conversion engine / 核心转换引擎" --repo "$REPO" --force
gh label create "easyeda / 嘉立创" --color "0b641b" --description "EasyEDA module / EasyEDA 模块" --repo "$REPO" --force
gh label create "kicad / KiCad" --color "e3621f" --description "KiCad export module / KiCad 导出模块" --repo "$REPO" --force
gh label create "ui / 界面" --color "f2347e" --description "User interface / 用户界面" --repo "$REPO" --force
gh label create "network / 网络" --color "1d76db" --description "Network client / 网络客户端" --repo "$REPO" --force
gh label create "services / 服务" --color "76611c" --description "Service layer / 服务层" --repo "$REPO" --force
gh label create "workers / 工作线程" --color "b5a4fa" --description "Background workers / 后台工作线程" --repo "$REPO" --force
gh label create "cli / 命令行" --color "ededed" --description "Command line interface / 命令行工具" --repo "$REPO" --force
gh label create "models / 数据模型" --color "0e28c6" --description "Data models / 数据模型" --repo "$REPO" --force
gh label create "resources / 资源" --color "fe81f4" --description "Icons, images, QML / 图标、图片、QML" --repo "$REPO" --force
gh label create "utils / 工具" --color "6f42c1" --description "Shared utility code / 共享工具代码" --repo "$REPO" --force

# 状态标签
gh label create "good first issue / 新手友好" --color "7057ff" --description "Good for newcomers / 适合新手参与" --repo "$REPO" --force
gh label create "help wanted / 寻求帮助" --color "008672" --description "Extra attention is needed / 需要社区帮助" --repo "$REPO" --force
gh label create "wontfix / 不修复" --color "ffffff" --description "Will not be worked on / 不予修复" --repo "$REPO" --force
gh label create "duplicate / 重复" --color "cfd3d7" --description "Already exists / 重复问题" --repo "$REPO" --force
gh label create "invalid / 无效" --color "e4e669" --description "Not valid / 无效问题" --repo "$REPO" --force
gh label create "question / 提问" --color "d876e3" --description "Further information needed / 需要更多信息" --repo "$REPO" --force
gh label create "priority: high / 高优先级" --color "b60205" --description "High priority issue / 高优先级问题" --repo "$REPO" --force
gh label create "priority: medium / 中优先级" --color "fbca04" --description "Medium priority / 中优先级" --repo "$REPO" --force
gh label create "priority: low / 低优先级" --color "0e8a16" --description "Low priority / 低优先级" --repo "$REPO" --force
gh label create "status: in progress / 进行中" --color "1d76db" --description "Currently being worked on / 正在处理" --repo "$REPO" --force
gh label create "status: blocked / 阻塞" --color "d73a4a" --description "Blocked by something / 被阻塞" --repo "$REPO" --force
gh label create "status: ready for review / 待审核" --color "7057ff" --description "Ready for code review / 等待代码审核" --repo "$REPO" --force

echo ""
echo "=========================================="
echo "   Labels 同步完成！"
echo "=========================================="
