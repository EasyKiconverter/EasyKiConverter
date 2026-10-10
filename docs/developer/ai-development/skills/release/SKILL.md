---
name: release
description: 按 EasyKiConverter 当前仓库规则准备并执行版本发布，覆盖版本门禁、跨平台 GitHub Release、标签、Flathub 和 Arch Linux/AUR 更新及发布后核验。
---

# Release Skill

## 适用场景

用户要求准备或执行 EasyKiConverter 的正式版本发布时使用。发布准备、创建标签、推送标签、公开 GitHub Release、更新 Flathub、推送 AUR 是不同阶段；调用本 Skill 不自动授权任何外部写入。

## 必读资料

- `docs/developer/ai-development/policy/AI_POLICY.md`
- `docs/developer/CONTRIBUTING.md` 的发布流程和分支管理约定
- `.github/workflows/release.yml`
- `.github/workflows/pack-linux.yml`
- `.github/workflows/pack-windows.yml`
- `.github/workflows/pack-macos.yml`
- `tools/python/manage_version.py` 及其当前 `--help` 输出
- `deploy/archlinux/PKGBUILD`、`deploy/archlinux/README.md`
- 当前版本的更新日志、README 下载说明、应用元数据和发布阻塞项

如果要更新 Flathub 或 AUR，还必须读取对应的实际仓库、manifest/PKGBUILD、当前上游维护说明和该仓库的 CI 规则。不得把本地项目中的旧副本当成官方商店仓库的当前状态。

## 发布边界

- 以当前 checkout 的源码、测试、工作流和配置为准；每次发布前重新确认资产矩阵、脚本参数、平台状态和分发渠道要求，不依赖本 Skill 中的历史版本号或旧资产清单。
- 遵循项目分支约定：一个主题分支可有多个相关提交，PR 回到对应版本分支。发布目标分支由维护者按本次发布计划确认；不要把“必须先合并到 `master`”当作通用前置条件。目标 commit 必须同时满足当前发布政策和 `release.yml` 的分支准入校验，不得直接改写受保护分支。
- 版本号、更新日志、发布说明、应用元数据和打包配置必须一致。运行版本管理脚本前先读 `--help` 和脚本实现；脚本中的 `--push-aur` 会进行远端写入，不得与本地版本同步步骤混为一谈。
- 不得移动、重建或强制推送已存在的版本标签；若标签已存在，先比较标签对象和目标 commit，发现不一致立即停止。
- GitHub 发布工作流会依据标签打包 Linux、Windows 和 macOS 制品，校验资产和 SHA256 后创建 Draft Release；公开发布由 `release-approval` Environment 单独审批。Agent 不得绕过该门禁直接发布。
- Flathub 是独立仓库和审核流程：更新应提交到实际 Flathub 仓库并遵循其 PR/CI/审核要求，不假设可直接推送主分支。
- AUR 是独立 Git 仓库：校验源码归档校验和、`PKGBUILD`、`.SRCINFO` 和构建结果后才可推送；`SKIP` 不能被当作已验证校验和。
- Issue #210 的缓存清理安全验收是已知发布风险。发布 v3.1.13 时必须重新核实 Issue 状态及旧版本升级、缓存迁移和清理的真实验收证据；若仍未完成，不得宣称该风险已关闭或自行判定稳定发布无阻塞。后续版本也应从当前 Issue、CI 和发布风险清单重新确认未关闭的 P0/P1。

## 执行流程

### 1. 建立发布基线

1. 读取 `git status --short --branch`、当前分支、HEAD、远端跟踪状态和待发布版本；保留用户现有修改，不清理或覆盖工作区。
2. 确认目标版本、维护者指定的发布分支、目标 commit、是否已有同名标签或 Release。用 Git 和 GitHub 当前状态交叉核实，不根据分支名推测，也不自行要求目标 commit 必须已合并到 `master`。
3. 检查 Issue/PR、全平台 CI、文档检查、安全检查、已知回归和发布阻塞项。v3.1.13 需要特别核实 Issue #210 的最终验收证据。
4. 检查版本号、更新日志、README 下载表、应用元数据、Flathub manifest 和 Arch 包版本是否有遗漏或互相矛盾。只在用户要求准备版本内容时修改仓库文件；否则报告缺项并等待授权。
5. 形成发布计划，列明版本、目标 commit、将创建/更新的远端对象、各平台包、Flathub/AUR 状态和阻塞项。

### 2. 发布前验证

1. 按当前仓库 Testing Skill 和 `verification-policy.json` 执行应有验证；正式发布必须检查目标版本对应的全平台 CI，不以本地 Linux 构建代替其他平台验证。
2. 确认发布工作流的标签格式、版本匹配规则、允许的分支包含规则、制品命名、必需架构、单文件校验和及汇总 `SHA256SUMS`。这些均以当前 `.github/workflows/` 为准。
3. 核对 GitHub `release-approval` Environment 配置了 Required reviewer；不能只凭 workflow 名称认定审批门禁有效。
4. 确认发布说明准确反映当前版本，不要盲目沿用 workflow 中的硬编码描述；Draft Release 生成后必须逐项人工检查制品、架构、链接、更新说明和校验和。
5. 如果准备分发渠道更新，检查 Flathub 源码 tag 与精确 commit SHA；Arch 源码归档 SHA256、`PKGBUILD` 和生成的 `.SRCINFO`；记录需要的构建和 lint 验证。

### 3. 外部操作授权门

在任何远端写操作前，向用户一次性列出精确动作和目标，包括：

- 将要创建并推送的标签、目标 commit 和实际推送凭据类型；说明普通用户凭据、PAT 或 GitHub App 凭据的标签 push 会触发 `release.yml`，但使用仓库 `GITHUB_TOKEN` 推送不会触发新的 `push` workflow（见 [GitHub 官方 GITHUB_TOKEN 说明](https://docs.github.com/en/actions/concepts/security/github_token)）；
- 工作流会打包 Linux、Windows、macOS 并创建/更新 Draft Release。若使用普通用户凭据/PAT/GitHub App，标签 push 与启动 Release workflow 是耦合动作；若使用 `GITHUB_TOKEN`，发布计划必须明确授权后续 `workflow_dispatch`（该事件是 `GITHUB_TOKEN` 可触发 workflow 的例外）。用户若只授权推标签但未授权由此启动的发布流程，不得推送标签；
- 公开发布仍须由 GitHub `release-approval` 环境的 Required reviewer 完成；
- 是否创建 Flathub PR；
- 是否向 AUR 远端推送提交；
- 使用的 GitHub/AUR 身份及目标仓库。

只有用户对这些具体动作有明确授权后才执行相应远端写入。发布 Skill 本身、过去对其他版本的授权、仅要求“准备发布”均不构成授权。未获授权时可以完成只读检查和本地准备，然后停在待审批状态。

### 4. 标签与 GitHub Release

1. 在最终目标 commit 上确认版本文件和发布阻塞均通过；读取远端同名标签及 Release 状态，并检查该版本已有的 workflow runs。
2. **首次发布（远端标签不存在）**：先确认推送凭据类型。经用户明确授权相应的标签和 workflow 动作后，创建指向精确 commit 的带注释标签并核对 peeled commit SHA，再推送标签。若使用普通用户凭据、PAT 或 GitHub App 凭据，推送后检查是否已产生 Release workflow run；若使用仓库 `GITHUB_TOKEN`，不得期待标签 push 自动触发 workflow，而应走下述手动触发路径。凭据类型或触发结果无法确认时停止，不盲目补发。
3. **手动触发及精确 ref 校验**：仅在用户已明确授权 `workflow_dispatch` 时使用。先确认该 workflow 文件存在于仓库默认分支（GitHub 的 [`workflow_dispatch` 规则](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow)要求工作流位于默认分支），再同时满足以下条件：
   - `--ref` 精确使用已核实的目标版本标签，不使用默认分支或其他分支。
   - `tag_name` 输入与 `--ref` 标签名称逐字完全一致。
   - 该标签解析出的 commit SHA 与预定发布 commit SHA 完全一致。
   - 启动后检查 workflow run 的 `head_sha` 与目标 commit SHA 完全一致；不一致时停止，不继续重试或操作 Release。
   - 确认所选标签对应的 workflow 文件及 `resolve-target` 中的版本/分支准入检查与该次发布相符；不能仅因默认分支存在 workflow 就假定 tag ref 可用。
4. 标签 push 已触发 Release workflow 时，不得为同一发布再调用 `workflow_dispatch`；只有确认仓库 `GITHUB_TOKEN` 抑制了 push 事件或已有失败 run 需要重试，并且用户明确授权后，才采用手动触发。
5. **检查 concurrency 边界**：当前 `release.yml` 的并发组为 `release-${{ inputs.tag_name || github.ref_name }}` 且 `cancel-in-progress: false`。它不是去重锁，也不保证所有重复 run 按顺序排队：GitHub 同一并发组默认最多一个运行中的 run 和一个 pending run；新的 run 进入队列时会取消并替换原 pending run（见[官方 concurrency 说明](https://docs.github.com/en/actions/concepts/workflows-and-actions/concurrency)）。每次触发/重试前，检查该版本是否已有任何未完成的 run；有则等待，不要再 push tag 或 dispatch。触发后记录 run ID，并核对 event、ref、commit SHA 与状态；若有 run 被替换/取消或状态无法确认，停止后续触发并报告，不自行补发。
6. **已有标签重试**：若标签已存在，禁止重建、移动或覆盖；标签指向与目标 commit 不一致时停止。若指向一致，检查该版本所有 workflow run、Draft/正式 Release 状态及是否有运行中或 pending 的任务：未完成则等待并复核；先前运行已成功则不得重复启动；确需重试时，先向用户说明拟采用的单一重试路径，经明确授权后优先重跑适用的失败 job；只有确认没有未完成/重复 run、workflow_dispatch 默认分支及精确 ref 条件均满足且重跑不适用时，才考虑 dispatch 重试。一次重试只使用一种路径，不能同时 rerun 和 dispatch。
7. 经授权后按所选路径触发或重试工作流。持续跟踪 `resolve-target`、Linux、Windows、macOS、制品校验、Draft 上传和审批 job；失败时保留日志并定位，不重复创建标签或绕过门禁。
8. Draft Release 生成后核对当前 workflow 规定的全部平台/架构制品、版本和 commit 对应关系、每个 `.sha256sum` 以及汇总 `SHA256SUMS`。下载到临时目录后执行适用的校验和验证，确认汇总文件只使用制品 basename。
9. 完成草稿审核后，将公开发布交给配置好的 GitHub Environment Required reviewer 批准；Agent 不得移除 reviewer、改 Environment 或直接把 Draft 改为公开。

### 5. Flathub 与 Arch Linux/AUR

**Flathub**

1. 定位并确认用户指定或本机已有的官方 Flathub app 仓库；不要假设其路径，也不要修改 EasyKiConverter 主仓库中的旧 manifest 来代替官方仓库。
2. 使用已发布的 tag 和对应精确 commit SHA 更新 manifest；按 Flathub 当前规则运行本地构建/lint，审阅 diff。
3. 只有经过授权才推送分支并创建 PR；记录 PR 链接、CI、审核和合并状态。上游 GitHub Release 成功不代表 Flathub 更新已完成。

**Arch Linux/AUR**

1. 确认主仓库 `deploy/archlinux` 文件与 AUR 实际仓库的版本、依赖、源 URL、校验和一致；从实际发布源码归档计算 SHA256，不以 `SKIP` 作为完成状态。
2. 按 Arch 当前工具链重新生成 `.SRCINFO`；运行 `makepkg --verifysource` 和适用的构建/打包验证，并审查 `PKGBUILD` 与 `.SRCINFO` 的差异。
3. 只有经过授权才更新 AUR Git 仓库、创建中文 Conventional Commit 并推送；AUR 通常是直接 Git 推送，不虚构 PR 流程。确认远端工作树、提交结果和 AUR 页面版本。

Flathub 审核和 AUR 更新可异步于 GitHub Release，但必须在报告中区分“已提交”“CI 通过”“已审核/合并”或“已推送/可安装”，不能把提交等同于用户可用。

### 6. 发布后核验

- 核实 GitHub Release 已公开且不是 Draft/Pre-release（除非发布计划明确要求预发布），标签仍指向预期 commit。
- 核实全部必需平台制品、架构、安装格式和校验文件可下载，汇总校验和通过。
- 核实 README 下载链接/下载计数数据来源与已发布资产匹配。
- 单独记录 Flathub PR/审核/发布状态和 AUR 当前版本/可构建状态。
- 检查更新接口所见版本、release 页面和仓库版本记录一致；如果缓存或 CDN 尚未刷新，注明观察时间和待复核项。
- 不在没有真实下载、校验或安装证据时宣称各平台“安装验证通过”。

## 停止条件

- 工作区存在未识别改动、目标版本/commit/分支不明确，或标签已存在但目标 SHA 不一致。
- 必需 CI 失败/仍在运行、版本信息不一致、发布说明不准确、必需资产缺失或校验失败。
- GitHub 发布审批 Environment 缺少有效 Required reviewer。
- 有未解决的 P0/P1 风险；尤其 v3.1.13 的 Issue #210 升级与清理验收证据不足。
- 外部写操作未取得针对该版本和目标仓库的明确授权，或者当前账号/远端身份不符合仓库规则。
- Flathub/AUR 的当前规范、实际仓库、来源校验和或构建结果无法核实。

停止时保留现状，不删除标签、不强推、不绕过审核；输出已完成阶段、证据、阻塞和需要用户决定的具体动作。

## 交付证据

最终报告至少包含：目标版本与 commit SHA、基线分支和工作区状态、标签及 Release URL、每个平台制品/校验状态、GitHub 审批状态、Flathub PR/状态、AUR commit/版本状态、实际执行命令及退出码、未执行项目和原因、剩余风险。把“本地验证”“CI 验证”“商店审核”“真实安装验证”分开陈述。
