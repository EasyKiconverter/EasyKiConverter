# Horizon EDA 格式参考

本文档记录 Horizon exporter 开发时核对过的格式事实。实现必须以对应上游源码为准，不得把本文档当作脱离源码的字段推测。

## 验证来源

- Horizon：`https://github.com/horizon-eda/horizon`
- 验证 commit：`ac014d28cf651fa05cef9f54f5b493053de9852b`
- Horizon Pool：`https://github.com/horizon-eda/horizon-pool`
- Pool 验证 commit：`0696685973dee6c9d7202704d23510768e212744`
- 验证日期：2026-10-01

重点依据 `src/pool/{unit,symbol,entity,package,padstack,part,pool_info}.{cpp,hpp}`、`src/package/{pad,hole,shape}.{cpp,hpp}`、`src/common/common.hpp` 和 `src/pool-update/` 的读取及 `serialize()` 实现。

## 已确认事实

- Horizon 坐标使用纳米整数；`common.hpp` 明确规定 1 个单位为 1 nm，即 1 mm = 1,000,000 units。
- `pool.json` 必须包含 `uuid`、`default_via`、`name`、`type: "pool"`；`pools_included` 可选，`default_frame` 也可选，存在时文件版本为 1。
- Unit 文件为 `type: "unit"`，包含 `name`、`manufacturer`、`uuid` 和按 UUID 索引的 `pins`。Pin 使用 `primary_name`、`direction`、`swap_group`，可选替代名称。
- Symbol 引用 Unit UUID，并按 UUID 索引 `junctions`、`pins`、`lines`、`arcs`、`polygons`、`texts` 和 `text_placements`。
- Entity 的 `gates` 以 Gate UUID 索引；Gate 至少包含 `name`、`suffix`、`swap_group` 和 Unit UUID。
- Part 的 `entity`、`package` 和 `pad_map` 是 UUID 引用；`pad_map` 的每一项把 Package Pad UUID 映射到 Gate UUID 与 Unit Pin UUID。
- Package 即使没有 3D 模型也必须包含 `default_model`，此时使用全零 UUID；存在模型时应指向 `models` 中的一个 UUID。
- PoolUpdater 负责 SQLite `pool.db` 索引；EasyKiConverter 只应写 source files，不应复制其数据库 schema writer。
- 导出阶段先在临时 Pool 上调用官方 Python binding 的 `horizon.Pool.update(path)`，确认官方生成了非空 `pool.db` 后才提交源文件；提交成功后再按官方 `PoolManager.get_pools()` 检查 UUID 冲突、调用 `horizon.PoolManager.add_pool(path)` 并回读注册表。追加、更新和重试模式通过复制现有 Pool 到临时目录再写入本次组件，保留未参与本次导出的对象；覆盖模式则重建整个 Pool。解释器可由 `EASYKICONVERTER_HORIZON_PYTHON` 指定，模块目录可由 `EASYKICONVERTER_HORIZON_PYTHONPATH` 指定；PoolUpdater 失败时不会提交源文件，注册失败则导出失败并保留已生成的源目录及诊断。

## 目录与实现边界

官方 Pool 使用 `units/`、`symbols/`、`entities/`、`padstacks/`、`packages/`、`parts/` 和 `3d_models/`。Package 是目录对象，官方样本将其主体写入 `packages/<path>/package.json`，并可在同目录下保存 padstack 文件。

所有 UUID、JSON 对象顺序、文件路径和诊断必须由 EasyKiConverter 自己确定性生成。本文档仅记录格式行为；导出实现独立编写，不复制 Horizon GPLv3 源码。

## 当前导出边界

- 封装圆形使用 Horizon 的四段原生圆弧，矩形和折线使用带线宽的原生 Line；椭圆、扇形、Bezier 和部分曲线在没有等价原生对象时才转换为折线或多边形，并记录近似诊断。
- SMD Padstack 同时写入铜、阻焊和锡膏图形，并使用官方 `parameter_program` 表达阻焊扩展和锡膏收缩；自定义焊盘多边形写入对应的 Padstack `polygons`，其制造层使用 `expand-polygon` 保持旋转后的轮廓。
- 通孔 Padstack 写入官方支持的全铜层铜图形、上下阻焊、镀层属性和圆孔/槽孔；独立安装孔保持机械 Padstack，不会生成电气引脚。
- 未镀通孔（NPTH）使用 `plated: false` 的机械语义；即使源数据带有空编号，也只生成 Package Pad 和 Padstack，不加入 Part 的 `pad_map`，因此不会伪造电气引脚。
- 独立安装孔写入机械 Padstack，并由 Package Pad 引用；当前 IR 的独立孔仅表达圆孔，槽孔仍需源数据提供槽长。
- KeepOut 使用 Package 的 `polygons` 与 `keepouts` 关联对象，不能当作 courtyard 使用。
- 圆角矩形优先保留 IR 提供的自定义轮廓；若同时缺少圆角半径和轮廓点，则降级为矩形并记录诊断。
- 启用三维模型导出时，STEP 写入 `3d_models/`，Package 的模型位置合并 `translation` 与 `stepOffsetMm`；当前固定版本 Horizon 的桌面 3D 加载路径仅支持 STEP，OBJ-only 模型会被拒绝并生成诊断。关闭三维模型导出时不写入模型文件和引用。
- 图片目前不写入目标图元并输出明确诊断；Bezier 和部分曲线采用折线近似。
- 导出器通过官方 binding 生成 `pool.db` 并注册 Pool；本仓库未复制官方 SQLite schema。若官方 binding 不可用，导出会失败并保留明确诊断，不会伪造“已自动注册”。
- 固定上游没有提供面向外部应用的运行时刷新 API；其 `reload-pools` 通知使用 Horizon 内部 IPC 和 cookie。因此已关闭的 Horizon 在下次启动时读取注册结果，已运行实例需要按提示重启或手动重新加载，EasyKiConverter 不注入进程或模拟界面操作。

## 验证状态

已完成 Qt 主机编译、确定性输出、引用完整性、引脚到焊盘映射、多部件、锡膏、安装孔、KeepOut、几何降级和 3D 模型测试。已使用上游 commit `ac014d2` 构建 `horizon-pr-review`，并以 `--pool-update` 成功读取并索引 EasyKiConverter 生成的 fixture Pool；官方工具未报告文件错误，`pool.db` 中已回读 entity、symbol、unit、package、padstack 和 part 索引。随后又在仓库内临时解包 Python 3.12 开发包并构建了同一上游 commit 的 `horizon.so`，实际调用 `horizon.Pool.update()` 和 `horizon.PoolManager.add_pool()` 成功，且使用隔离的 `XDG_CONFIG_HOME` 回读了 `pools.json`。2026-10-02 在 Niri/Wayland 会话中使用真实元器件 C2040 导出 Pool，启动同一固定 commit 构建的 Horizon 桌面程序并从已注册 Pool 打开 `RP2040` Unit；桌面编辑器成功修改制造商字段、保存并关闭，随后重启 Horizon 并重新打开同一 Unit，修改后的字段仍可读回。运行时刷新仍未验证；已运行实例仍按上游限制需要重启或手动重新加载。

可以用仓库内的集成验证器重复执行官方工具检查（不会修改输入 Pool；验证器会在 Pool 所在目录创建并自动清理隔离副本）：

```bash
.venv/bin/python tests/integration/validate_horizon_pool.py \
  --pool <generated-pool> \
  --horizon-review <path-to-horizon-pr-review>
```

该命令会运行固定版本的 `horizon-pr-review --pool-update`，确认 `pool.db` 非空，并检查官方索引以及 source JSON 的 Unit、Symbol、Entity、Package、Padstack、Part、Pad Map、3D 文件引用；同时拒绝越过 Pool 根目录的模型路径。若要同时验证官方 Python binding 的更新和注册流程，可将最后一个参数替换为：

```bash
EASYKICONVERTER_HORIZON_PYTHONPATH=<directory-containing-horizon.so> \
.venv/bin/python tests/integration/validate_horizon_pool.py \
  --pool <generated-pool> \
  --horizon-python <python-interpreter>
```

两种模式都会在 Pool 的隔离副本上运行，不修改输入目录；Python 模式还会在临时 `XDG_CONFIG_HOME` 中验证 `PoolManager.add_pool()` 注册结果。C2040 的桌面编辑、保存、重启后回读已经在 Niri/Wayland 会话中完成；命令行和桌面验证仍不等同于已运行 Horizon 的运行时刷新验证。
