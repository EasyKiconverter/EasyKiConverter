# 文档目录

本目录包含 EasyKiConverter 项目的所有文档。

## 文档组织

文档按照目标读者分为三类：

### 面向用户 (user/)

这类文档的目标是让用户知道这是什么以及如何使用它。

- [用户手册](user/USER_GUIDE.md) - 详细的使用说明
- [用户手册 (英文)](user/USER_GUIDE_en.md) - User Guide
- [快速开始](user/GETTING_STARTED.md) - 快速入门指南
- [快速开始 (英文)](user/GETTING_STARTED_en.md) - Getting Started
- [常见问题](user/FAQ.md) - 常见问题解答
- [常见问题 (英文)](user/FAQ_en.md) - FAQ
- [功能特性](user/FEATURES.md) - 详细的功能特性说明
- [功能特性 (英文)](user/FEATURES_en.md) - Features

### 面向开发者 (developer/)

这类文档的目标是让其他开发者能够理解项目、参与贡献和进行维护。

- [构建指南](developer/BUILD.md) - 从源代码构建
- [构建指南 (英文)](developer/BUILD_en.md) - Build Guide
- [编码规范](developer/CODING_STYLE.md) - C++ 和 QML 编码规范
- [编码规范 (英文)](developer/CODING_STYLE_en.md) - Coding Style
- [贡献指南](developer/CONTRIBUTING.md) - 如何贡献代码
- [贡献指南 (英文)](developer/CONTRIBUTING_en.md) - Contributing Guide
- [架构文档](developer/ARCHITECTURE.md) - 项目架构设计
- [转换层与映射关系](developer/CONVERSION_MAPPING.md) - EasyEDA、IR、KiCad、Altium、Xpedition 的算法与字段映射
- [格式解析架构](developer/PARSER_ARCHITECTURE.md) - 通用文本解析基础设施、诊断模型和格式支持边界
- [CADSTAR 导出能力](developer/CADSTAR_EXPORT.md) - CADSTAR ASCII 符号、封装、Pad 和 Part 导出范围
- [OrCAD Capture XML 符号库导出](developer/ORCAD_EXPORT.md) - OrCAD Capture XML 符号和封装名称关联导出范围
- [EasyEDA API 原始数据说明](developer/EASYEDA_API_DATA.md) - API 响应结构、shape 编码和字段解析
- [文档维护指南](developer/DOCUMENTATION_MAINTENANCE.md) - 文档事实来源、更新触发条件和过时内容清理规则
- [架构文档 (英文)](developer/ARCHITECTURE_en.md) - Architecture
- [测试开发指南](developer/TESTING_GUIDE.md) - 测试架构与 Mock 策略
- [开发者常见问题](developer/FAQ.md) - 技术问题、根因分析和回归预防
- [国际化实现](developer/I18N_IMPLEMENTATION_SUMMARY.md) - 国际化支持说明
- [AI 协作开发入口](developer/ai-development/README.md) - AI Agent 和开发者协作入口、规则导航与 Skills

### 面向项目决策 (project/)

这类文档记录了项目的演进过程和未来方向，帮助团队做出正确的战略决策。

- [项目路线图](project/ROADMAP.md) - 未来发展方向
- [通用 EDA 库转换器规划](project/UNIVERSAL_EDA_LIBRARY_CONVERTER.md) - 基于统一 IR 的库格式转换目标与边界
- [项目路线图 (英文)](project/ROADMAP_en.md) - Roadmap
- [通用 EDA 库转换器规划 (英文)](project/UNIVERSAL_EDA_LIBRARY_CONVERTER_en.md) - Universal EDA library conversion plan
- [架构决策记录](project/adr/README.md) - 技术决策记录

### 历史分析报告（归档）

以下报告内容已由 ADR 替代，仅作历史参考：

- [弱网支持分析报告](project/archive/WEAK_NETWORK_ANALYSIS.md) - v3.0.4 弱网容错分析
- [性能优化报告](project/archive/PERFORMANCE_OPTIMIZATION_REPORT.md) - v3.0.0 性能优化

## 文档语言

所有文档都提供中英文双语版本：
- 中文文档使用中文文件名
- 英文文档使用英文文件名（带 _en 后缀）

## 如何使用文档

### 如果您是用户

1. 从 [快速开始](user/GETTING_STARTED.md) 开始
2. 阅读 [用户手册](user/USER_GUIDE.md) 了解详细功能
3. 查看 [常见问题](user/FAQ.md) 解决遇到的问题

### 如果您是开发者

1. 阅读 [构建指南](developer/BUILD.md) 了解如何构建项目
2. 查看 [架构文档](developer/ARCHITECTURE.md) 理解项目架构
3. 阅读 [测试开发指南](developer/TESTING_GUIDE.md) 了解测试规范与 Mock 编写
4. 阅读 [开发者常见问题](developer/FAQ.md) 解决开发中的已知问题
5. 阅读 [贡献指南](developer/CONTRIBUTING.md) 了解如何贡献代码

如果使用 AI Agent 开发，请先阅读 [AI 协作开发入口](developer/ai-development/README.md)。

### 如果您想了解项目规划

1. 查看 [项目路线图](project/ROADMAP.md) 了解未来方向
2. 阅读 [架构决策记录](project/adr/) 了解技术决策

## 文档原则

- **文档即代码**：文档和代码放在同一个仓库中，使用 Markdown 等纯文本格式
- **保持更新**：过时的文档比没有文档更糟糕
- **为读者而写**：在写任何文档之前，先想清楚你的读者是谁

## 贡献文档

如果您想改进文档：

1. 确保文档与代码保持一致
2. 提供清晰的示例
3. 保持中英文双语版本同步
4. 提交 Pull Request

## 相关资源

- [项目主页](https://github.com/EasyKiconverter/EasyKiConverter)
- [GitHub Issues](https://github.com/EasyKiconverter/EasyKiConverter/issues)
- [GitHub Discussions](https://github.com/EasyKiconverter/EasyKiConverter/discussions)
