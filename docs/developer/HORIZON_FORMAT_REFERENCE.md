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

## 目录与实现边界

官方 Pool 使用 `units/`、`symbols/`、`entities/`、`padstacks/`、`packages/`、`parts/` 和 `3d_models/`。Package 是目录对象，官方样本将其主体写入 `packages/<path>/package.json`，并可在同目录下保存 padstack 文件。

所有 UUID、JSON 对象顺序、文件路径和诊断必须由 EasyKiConverter 自己确定性生成。本文档仅记录格式行为；导出实现独立编写，不复制 Horizon GPLv3 源码。

## 当前导出边界

- 圆、椭圆、圆弧、扇形和常见封装图元转换为 Horizon 折线或多边形，并记录近似诊断。
- SMD Padstack 同时写入铜、阻焊和锡膏图形；自定义焊盘多边形写入对应的 Padstack `polygons`。
- 独立安装孔写入机械 Padstack，并由 Package Pad 引用；当前 IR 的独立孔仅表达圆孔，槽孔仍需源数据提供槽长。
- KeepOut 使用 Package 的 `polygons` 与 `keepouts` 关联对象，不能当作 courtyard 使用。
- 圆角半径缺失时圆角矩形降级为矩形并记录诊断。
- STEP/OBJ 写入 `3d_models/`，Package 的模型位置合并 `translation` 与 `stepOffsetMm`。
- 图片目前不写入目标图元并输出明确诊断；Bezier 和部分曲线采用折线近似。
- 导出器目前只写 Pool source files。`pool.db`、桌面应用的 PoolManager 注册和 PoolUpdater 必须由固定版本的官方 Horizon 工具完成；本仓库未复制官方 SQLite schema，也不会伪造“已自动注册”。

## 验证状态

已完成 Qt 主机编译、确定性输出、引用完整性、引脚到焊盘映射、多部件、几何降级和 3D 模型测试。已使用上游 commit `ac014d2` 构建 `horizon-pr-review`，并以 `--pool-update` 成功读取并索引生成的 fixture Pool；官方工具未报告文件错误。
