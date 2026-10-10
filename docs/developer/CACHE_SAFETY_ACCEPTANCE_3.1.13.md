# v3.1.13 缓存安全发布前验收记录

## 验收范围

本记录针对 Issue #210 的缓存误删风险，验证 v3.1.13 源码中的目录选择、旧缓存迁移、自愈、配额裁剪、清空缓存和异步写入边界。验收只涉及缓存安全，不等同于完整的桌面发布验收。

基线为 `v3.1.13` 版本分支提交 `c5e14f92801e09ce6eb143acdb857db6745fdc48`；当前仓库尚未创建 `v3.1.13` 标签。本次验收使用的工作分支 HEAD 为 `cabf64f014aad355c267f78a011c9b8af846066a`。仓库中存在 `v3.1.12` 源码标签，但没有找到可安全运行的 v3.1.12 二进制，因此没有宣称完成旧二进制升级实测。

## 隔离方案与样本来源

- 所有测试使用 QtTest `QTemporaryDir` 创建临时缓存根目录、目标目录和配置文件。
- 目录迁移测试使用 v3.1.12 等价的旧布局样本：移除当前所有权字段和根目录标记，只保留可由旧布局识别的组件元数据；样本在测试运行后由临时目录自动清理。
- 不把真实用户主目录、真实缓存目录或用户文档目录作为清理目标。
- 回收站操作通过 `CacheSafety::TrashFunction` 注入替身验证成功、失败和“不永久删除”行为。

## 场景结果

| 场景 | 结果 | 证据 |
| --- | --- | --- |
| 空的自定义缓存目录 | 通过 | `test_cache_safety::acceptsEmptyCustomChildDirectory` |
| 用户主目录、根目录和非空未托管目录 | 通过 | `test_cache_safety`、`test_export_settings_viewmodel` |
| 父目录或缓存条目为符号链接 | 通过 | `rejectsSymlinkParentDirectory`、`rejectsSymlinkedModel3DDirectory`、未知条目保护测试 |
| 可识别旧缓存迁移 | 通过 | `testLegacyCacheDirMigrationPreservesExistingCache` |
| 目标冲突和迁移失败 | 通过 | `testCacheDirMigrationConflictPreservesSourceEntries`、ViewModel 迁移失败测试 |
| 未知文件和目录 | 通过 | `testClearCachePreservesUnknownComponentFiles`、迁移未验证条目测试 |
| 清理和配额裁剪 | 通过 | `test_cache_pruner`、`test_component_cache_service` |
| 回收站失败 | 通过 | `preservesEntryWhenTrashFails`、`testPrunePreservesEntryWhenTrashFails` |
| 异步写入代次、取消和 tombstone 边界 | 通过 | `testCacheWritePolicyRejectsStaleRequests`、相关 tombstone/generation 测试 |

安全策略保持保守：只有根目录、组件目录、元数据和具体缓存文件均能验证归属时才允许维护；未知内容保留；回收站失败不会回退到永久删除。三维模型目录符号链接会被拒绝，不会跟随链接写入或清理外部目录。

## 实际执行的检查

以下命令均在项目 `.venv`、Qt 6.10.2 GCC 环境下执行，退出状态为 0：

```bash
QT_QPA_PLATFORM=offscreen ./.venv/bin/python tools/python/build_project.py --test -j 2
./.venv/bin/python tools/python/format_code.py --check --cpp --dirs src tests
./.venv/bin/python tools/python/check_comment_rate.py --threshold 100 --check-doxygen --check-variables --check-params --forbid-generic-comments --forbid-external-references tests/unit/test_export_settings_viewmodel.cpp
./.venv/bin/python tools/python/validate_ai_development.py
./.venv/bin/python tools/python/build_docs.py --mkdocs
git diff --check
```

构建工具报告完整测试通过；单独复核的缓存测试包括 `test_cache_safety`、`test_cache_pruner`、`test_component_cache_service` 和 `test_export_settings_viewmodel`。

## 未验证项与发布结论

- 未执行 v3.1.12 二进制升级到 v3.1.13 的真实启动迁移；当前结论基于 v3.1.12 源码标签和等价旧布局隔离样本。
- 未在真实桌面环境观察系统回收站；注入替身证明了业务调用、失败保留和无永久删除后备逻辑，但不证明当前桌面环境的回收站实现可用。
- 未执行 Niri/Wayland 文件管理器或真实用户目录清理测试。

因此，缓存安全代码和自动化边界已有充分可复现证据，但若发布门槛要求真实旧版二进制升级或真实系统回收站验证，发布仍应保留相应人工验收项，不能将本记录表述为已完成全部实机验收。

[English version](CACHE_SAFETY_ACCEPTANCE_3.1.13_en.md)
