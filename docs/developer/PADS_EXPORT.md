# PADS PCB 封装导出

当前功能只支持 **EasyEDA/LCSC 封装 → PADS Parts Library ASCII PCB Decal**。它不是 PADS 原生二进制库、原理图库或完整元件库导出器。

## 导出边界

```mermaid
flowchart LR
    EasyEDA[EasyEDA/LCSC 数据] --> IR[统一 Footprint IR]
    IR --> Model[PADS Decal 文本模型]
    Model --> Decal[独立 .d PCB Decal 文件]
    Decal --> PADS[PADS 中继续导入或建立库]
```

导出器只读取统一 IR，不重新解析 EasyEDA JSON，也不复用 KiCad、Altium 或 Xpedition 的最终写入逻辑。当前使用 PADS ASCII Parts Library 规范中的英制 mil 坐标（文件头单位标识为 `I`）。

## 当前已实现

- GUI 目标格式：`PADS PCB`。
- CLI 目标格式：`--target-format pads`。
- 一个输出目录中为每个封装生成一个经过安全清洗的 `<name>.d` 文件。
- 圆形、方形、矩形、椭圆和基本走线/矩形/区域图元。
- SMD 与 PTH Pad 栈；PTH 会写入顶层和底层记录，并保留钻孔及镀层标志。
- 清洗后名称冲突、空编号、非法坐标和非 ASCII 编号/文本会阻止导出并返回诊断。
- 每个 Decal 文件包含头部、时间戳、图元、文本、端子和 Pad 栈记录。

## 明确限制

- 当前不支持独立机械孔、圆弧、RoundRect、Trapezoid、Polygon 自定义 Pad 的无损输出；遇到这些数据会失败并给出诊断，不会静默退化。
- 当前不写入 3D 模型关联；如果 IR 包含模型，会产生可见诊断。PADS 目标的 3D 关联需要单独的目标版本和样本验证。
- 尚未实现 PADS 原理图库、CAE 元件、完整 Part/Logic 库、多文件索引或现有 PADS 库更新/追加。
- 现有输出目录的 `no-overwrite`、`update-mode` 和 `retry-mode` 不会伪装成安全合并；这些模式会被导出阶段拒绝。
- 当前环境没有安装 PADS 软件，因此只完成文本结构、文件生成和引用数量的自动化验证，未完成 PADS 实机打开、保存和回读验证。

## 验证来源和使用方式

导出格式依据 [PADS Parts Library ASCII 规范](https://www.freecalypso.org/pub/CAD/PADS/pdfdocs/Plib_ASCII.pdf) 实现，重点包括 PCB Decal 头部、`CIRCLE`/`OPEN`/`CLOSED` 图元、`T` 端子和 `PAD` Pad 栈记录。实现没有复制第三方项目代码。

GUI 中选择 `PADS PCB` 后只提供封装导出；CLI 使用：

```text
easykiconverter --target-format pads ...
```

生成目录需要由用户按照目标 PADS 版本的库管理流程导入。请在目标软件中确认层语义、文本编码、制造属性和后续库索引；本项目不宣称跨版本的 PADS 原生兼容性。

## 测试

`tests/unit/test_pads_exporter.cpp` 使用本地 IR fixture 验证基本 Decal 结构、mil 单位头、SMD/PTH Pad 栈、清洗名称冲突和不可无损表达数据的失败诊断。测试不访问网络，也不依赖本地安装的 PADS 软件。
