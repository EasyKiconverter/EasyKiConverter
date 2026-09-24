# .github 文件夹配置说明

## 概述

`.github` 文件夹包含 GitHub 自动识别的配置文件和模板，用于改善项目的用户体验和自动化工作流程。

## 文件结构

```
.github/
├── ISSUE_TEMPLATE/          # Issue 模板
│   ├── bug_report.md       # Bug 报告模板
│   ├── feature_request.md  # 功能建议模板
│   ├── question.md         # 问题咨询模板
│   └── config.yml          # Issue 模板配置
├── workflows/              # GitHub Actions 工作流
│   ├── ci.yml             # 持续集成工作流
│   ├── release.yml        # 发布工作流
│   └── security.yml       # 安全扫描工作流
├── CODE_OF_CONDUCT.md     # 行为准则
├── CONTRIBUTING.md        # 贡献指南
├── pull_request_template.md # Pull Request 模板
├── README.md              # .github 文件夹说明
├── SECURITY.md            # 安全政策
└── SUPPORT.md             # 获取支持指南
```

## 功能说明

### 1. Issue 模板 (ISSUE_TEMPLATE/)

GitHub 会自动识别这些模板，用户在创建 Issue 时可以选择合适的模板：

- **bug_report.md**: 用于报告 Bug
- **feature_request.md**: 用于提出功能建议
- **question.md**: 用于提问
- **config.yml**: 配置 Issue 模板和快速链接

### 2. Pull Request 模板 (pull_request_template.md)

用户创建 Pull Request 时会自动显示此模板，引导用户提供必要的信息。

### 3. 贡献指南 (CONTRIBUTING.md)

GitHub 会自动在以下位置显示此文件：
- Pull Request 页面的 "Contributing" 链接
- Issue 页面的 "Contributing" 链接

### 4. 获取支持指南 (SUPPORT.md)

GitHub 会自动在以下位置显示此文件：
- Issues 页面的 "Support" 链接
- Pull Request 页面的 "Support" 链接

### 5. 行为准则 (CODE_OF_CONDUCT.md)

GitHub 会自动在以下位置显示此文件：
- Issues 页面的 "Code of Conduct" 链接
- Pull Request 页面的 "Code of Conduct" 链接

### 6. 安全政策 (SECURITY.md)

GitHub 会自动在以下位置显示此文件：
- Issues 页面的 "Security" 链接
- Pull Request 页面的 "Security" 链接
- 专门的 Security 标签页

### 7. GitHub Actions 工作流 (workflows/)

自动化工作流程：

- **ci.yml**: 持续集成，自动构建和测试
- **release.yml**: 自动发布，创建 Release 和发布包
- **security.yml**: 安全扫描，检查依赖项漏洞

## 使用方法

### 提交到仓库

将这些文件提交到仓库后，GitHub 会自动识别并应用：

```bash
git add .github/
git commit -m "docs: 添加 GitHub 配置文件和模板"
git push origin dev
```

### 验证配置

1. 访问项目的 Issues 页面，查看是否显示模板
2. 访问项目的 Pull Requests 页面，查看是否显示 PR 模板
3. 查看 Actions 标签页，确认工作流已加载

## 自定义

### 修改模板内容

根据项目需求修改模板文件：

- 修改 `ISSUE_TEMPLATE/` 中的模板
- 修改 `pull_request_template.md`
- 修改工作流配置

### 添加新的工作流

在 `workflows/` 目录下添加新的 `.yml` 文件：

```yaml
name: Your Workflow Name
on:
  push:
    branches: [ master ]
jobs:
  your-job:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      # 你的步骤
```

## 注意事项

1. **文件位置**: 所有文件必须在 `.github` 文件夹的根目录或子目录中
2. **文件名**: 必须使用 GitHub 识别的标准文件名
3. **YAML 语法**: 工作流文件必须使用正确的 YAML 语法
4. **权限**: 某些工作流需要额外的权限（如发布工作流需要 `contents: write`）

## 相关资源

- [GitHub 文档 - Issue 模板](https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/configuring-issue-templates-for-your-repository)
- [GitHub 文档 - Pull Request 模板](https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/creating-a-pull-request-template-for-your-repository)
- [GitHub 文档 - 行为准则](https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions/adding-a-code-of-conduct-to-your-project)
- [GitHub 文档 - 贡献指南](https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions/adding-a-contributing-file-to-your-project)
- [GitHub 文档 - GitHub Actions](https://docs.github.com/en/actions)

## 维护

定期检查和更新：

- 更新 Issue 模板以反映新的问题类型
- 更新工作流以使用最新的 Actions 版本
- 更新安全政策以反映新的安全实践

## 反馈

如果您对这些配置有任何问题或建议，请：

- 创建 Issue 报告问题
- 创建 Pull Request 提出改进
- 在 Discussions 中讨论想法
