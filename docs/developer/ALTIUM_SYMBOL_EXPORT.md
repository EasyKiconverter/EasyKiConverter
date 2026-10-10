# Altium SchLib 符号导出能力

本文说明 EasyKiConverter 当前对 AD/Altium SchLib 符号的导出范围，以及中间表示与 SchLib 记录之间的映射关系。

## 已支持内容

- 符号主体图形：矩形、圆角矩形、线段、圆/椭圆、圆弧、椭圆弧、扇形、多边形、折线、Path、三次 Bézier 曲线、IEEE 图形、文本、文本框和图片。
- 图元样式：实线、虚线和点线，以及填充颜色、边框颜色和线宽。
- 引脚几何：位置、长度、方向、名称、编号、显示状态和部件归属。
- 引脚电气类型：输入、输出、双向、无源、电源、开集电极和开集发射极。
- 引脚装饰：覆盖 Altium IEEE 图形集合，包括取反圆点、时钟、低电平有效、开集电极/发射极、高阻、脉冲、延迟、移位、模拟/数字输入、总线、逻辑关系和信号方向标志。
- 符号参数：Value、Description、Manufacturer、Manufacturer Part Number、Datasheet、LCSC、JLCPCB、供应商及自定义参数。
- 参数控制：参数值、名称、显示/隐藏、只读、位置、旋转角度、字体编号、字体大小和所属部件。
- 文本字体：普通文本支持字体族、字号、粗体和斜体，并在 SchLib `FileHeader` 中动态登记字体表；参数的 `fontSizeMm` 会映射到参数记录的 `FONTID`；无法解析到当前字体表的显式编号回退到默认字体 1；旋转角度按最近的 90° 方向归一化为 Altium `Orientation=0..3`。
- 多部件符号：图形、文本、参数和引脚按 `partIndex` 写入对应部件；同名参数按 `OWNERPARTID` 分开保留，不会因名称去重而丢失部件参数。读取器同时解析 `OWNERPARTID` 和 `OWNERPARTDISPLAYMODE`，二进制 Pin 记录也会保留显示模式字段。
- 公共图元：`SymbolPart::commonToAllParts` 会在缓存序列化和 `IrBuilder` 阶段保留，转换为 IR 的负 `partIndex`；最终写入 Altium Part Zero（`OWNERPARTID=-1`）。公共部件中的引脚会设置 `SymbolPinIR::commonToAllParts`，其名称、编号文字也会继承该归属。文本图元同时写出 `OWNERPARTDISPLAYMODE=1`，与当前单显示模式的引脚记录保持一致。
- `IndexInSheet`：图元、二进制引脚和归属于具体部件的用户参数共享组件内从 `0` 开始的内容记录计数；首条内容记录隐含索引 `0`，后续文本记录和用户参数写出 `IndexInSheet`。二进制引脚虽然没有文本字段，但会推进同一计数器；组件记录、Designator、公共参数和实现记录不占用该计数。
- 多候选封装：每个封装生成一个 SchLib implementation，并自动去重。
- 通用模型关联：支持通过 IR 或来源元数据写入 SPICE、SIM、STEP、VRML 等模型类型、数据文件实体、参数和引脚映射。
- 模型默认状态：仅第一个实现写入 `ISCURRENT=T`；没有数据文件实体的 SPICE/SIM 等模型不会错误生成 `.PcbLib` 路径。
- 符号别名：从 EasyEDA 数据或 IR 中读取，写入组件记录及参数字段。
- 来源元数据：未知键会作为自定义参数保留，避免供应商字段丢失。
- 独立 IEEE 图形：通过 `SymbolIeeeIR` 写入 Altium `RECORD=3`，支持缩放、旋转、镜像、颜色和部件归属。
- 图片图元：通过 `SymbolImageIR` 写入 `RECORD=30`；提供图片字节和文件名时会同时写入压缩后的 `/Storage` 嵌入资源，否则保留为外部链接。读取 `/Storage` 时会验证完整 zlib 流并限制最大解压输出，拒绝损坏或资源耗尽风险较高的压缩数据。
- 非填充 SVG Path 的直线和 C/S/Q/T 曲线会保留为段级 IR；直线写入路径记录，三次/二次 Bézier 写入 Altium 原生 `RECORD=5`，避免曲线被无谓采样成大量折线。
- 嵌入图片文件名在单个库内按大小写不敏感方式去重；重复名称会稳定追加 `_2`、`_3` 等后缀，并同步更新图片记录和 `/Storage` 条目。
- 嵌入资源只使用文件名部分写入 `/Storage`，路径分隔符会被规范化；外部链接仍保留原始路径。空名称、路径遍历名称或超出 255 字节的嵌入名称会被跳过并记录警告。
- `SymbolData::validationErrors()` 可在导出前一次性报告空名称、非正尺寸、负圆角半径、无效点列、空路径、非法 SVG 字符或命令参数、空文本和不支持的文本锚点等输入问题；路径校验按 SVG token 和命令参数组数量解析，支持科学计数法，避免解析器静默丢弃不完整图元。文本锚点支持 `start`、`middle`、`end`，空锚点会在导出时回退为 `middle`；原点位于 `(0,0)` 的正尺寸边界框视为合法；`validate()` 保留只返回首个错误的兼容接口。路径段、独立多边形、折线、Bézier、圆弧、矩形、椭圆、扇形、椭圆弧、IEEE 图形、文本、文本框、引脚标签、参数和图片的几何参数及线宽在 Altium 导出阶段再次校验，非法数据会被跳过并写入诊断；三点共线的圆弧仍使用安全回退圆心。
- SchLib 底层写入器会拒绝空组件列表、空组件名称和空输出路径，并在复用写入器时清理上一轮诊断，避免无效输入继续进入 OLE 构建。
- SchLib 写入器对非正 `partCount` 保持兼容性规范化为 1，并通过非致命诊断提示调用方；磁盘格式仍会额外计入公共 Part 0。
- SchLib 图片 Storage 即使没有嵌入图片也会写入 `Weight=0`，确保空 Storage 可被读取器和后续校验一致解析。
- SchLib 的 `OWNERPARTID` 统一使用 `-1` 表示公共 Part、`1..32767` 表示普通 Part；写入侧对非法的 0、负数或超出二进制范围的值会规范化并记录诊断，读取侧拒绝小于 -1 或超出组件部件范围的记录，避免引脚与图元记录出现不同语义。
- SchLib 文本的非有限或负字体尺寸不会进入 `qRound()`，写入器会回退为默认字体大小并记录诊断，避免底层直接调用绕过上层校验时产生未定义结果。
- SchLib 圆弧、扇形和椭圆弧的非有限起止角度会回退为 `0/360` 度并记录诊断，避免底层直接调用时把 `NaN/Infinity` 序列化到 `Data` 流。
- SchLib 底层写入器会拒绝非正圆弧/椭圆/扇形/椭圆弧半径、负圆角半径、退化多边形/折线/路径以及非有限顶点，避免绕过上层 IR 校验时写入不可用几何记录。
- SchLib 读取器会对已知圆弧、椭圆、扇形、椭圆弧、圆角矩形和点列记录执行对称字段校验；未知记录仍保留原始数据，便于兼容未来 Altium 扩展。
- SchLib 写入器会在写出前检查所有引脚、图元、图片和参数的正数 `OWNERPARTID` 是否落在 `1..partCount`；公共 Part `-1` 以及历史兼容值 `0`/负值仍按既有规范化规则处理，避免生成读取端必然拒绝的多 Part 文件。
- SchLib 底层写入器遇到空文本时会保留记录以维持 `WEIGHT` 和 `IndexInSheet` 一致，同时通过诊断提示调用方；上层 IR 校验仍会优先跳过空文本。
- SchLib UTF-8 参数块中的组件、引脚、文本、图片文件名、参数值、来源元数据和实现字段会拒绝 `|` 与 NUL 字符；这些字符会破坏 Altium 参数边界，不能通过替换实现无损保存。普通 Unicode 文本仍通过 `%UTF8%` 扩展写入。
- PcbLib 写入器对旋转角度、区域/元件体轮廓坐标、3D 模型姿态和不透明度执行有限值校验；异常值回退到安全默认值并通过 `diagnostics()` 报告。
- PcbLib 的 Region、ComponentBody、封装描述、扩展参数以及 3D 模型元数据会拒绝 `|`、NUL、换行和无法编码的字符，避免 C 字符串参数块或模型元数据被静默改写。
- PcbLib 输出路径中的 `|`、换行或 NUL 字符会被拒绝，避免 `Library/Data` 元数据中的路径记录与实际输出文件不一致。
- PcbLib 写入器诊断会经由 `ExporterAltiumFootprint` 和 `FootprintExportStage` 汇总到导出进度及详细报告，不再只输出到日志。
- PcbLib 底层写入器会拒绝空输入、空输出路径、空封装名称、非正焊盘尺寸/通孔尺寸、非正弧线半径和不足三个顶点的区域，避免生成结构可读但不可用的封装。
- PcbLib 结构化读取器会同步拒绝非有限角度/旋转、非正尺寸、负孔参数和非法区域顶点数量，避免损坏库在反向解析时被误认为有效数据。
- PcbLib 读写两端统一限制已解析图元层号为 Altium 的 `1..74` 合法范围，覆盖铜层、覆盖层、机械层和 Multi Layer。
- 符号导出阶段会将这些非致命诊断附加到导出项状态；写出器直接调用和上层导出都会保留非法 Bézier 等被跳过图元的诊断。启用调试模式时，`easykiconverter_export_detailed_report.md` 会按元件列出诊断，不会因可回退的图元问题静默丢失信息。

## 数据流

```text
SymbolData
    ↓ IrBuilder::toSymbolIR
SymbolComponentIR
    ↓ ExporterAltiumSymbol::convertSymbol
AltiumSchComponent
    ↓ AltiumSchLibWriter
SchLib OLE/Data
```

## 参数字段约定

- `sourceMetadata["value"]` 优先作为 Altium `Comment` 字段；未提供时使用符号名称。
- `sourceMetadata["description"]` 优先作为 Description；未提供时使用 `SymbolComponentIR::description`。
- `sourceMetadata` 中的标准键使用稳定的 AD 显示名称。
- 未知键原样作为自定义参数名称写入。
- `SymbolParameterIR` 适用于需要显式控制显示、只读、位置和旋转的参数。
- 空名称或空值字段不会写入 SchLib。

## 已知限制

- 非填充来源 SVG Path 的直线、C/S/Q/T 曲线会保留原生段；圆形且未旋转的 A/a 圆弧写入 Altium 原生圆弧记录，椭圆弧、旋转弧及填充路径仍使用采样点列回退，以确保现有闭合和填充语义兼容。
- `FlagLeft` 和 `FlagRight` 使用 Altium 的左右信号流图形表达；不同 AD 版本的图形外观仍需目标版本实机确认。
- EasyEDA 当前数据源不一定提供完整的符号级 3D/仿真模型协议字段；调用方可通过 `SymbolModelIR` 或约定的来源元数据补充模型记录。
- EasyEDA 当前符号数据没有统一的文本框、图片、扇形和椭圆弧来源字段；这些图元可由调用方通过 IR 直接添加，圆角矩形的 `rx/ry` 会从来源数据自动保留。
- 图元顺序在完整且无重复引用的 `graphicOrder` 存在时按源顺序恢复，否则按类型列表顺序完整写出，并在导出诊断中报告回退原因。写出器会按最终 Data 流重新计算共享内容记录序号，避免引脚和图元之间出现重复或跳号；仍需使用不同 Altium 版本的真实 SchLib 样本做最终兼容性确认。
- 最终兼容性仍需使用目标版本的 Altium Designer 打开并检查引脚连接、参数编辑和多部件显示。

## 验证

使用项目构建工具执行：

```bash
QT_QPA_PLATFORM=offscreen python3 tools/python/build_project.py --test
```

当前单元、集成和 UI 测试均应通过，且 SchLib 测试会验证参数记录、UTF-8 文本、别名和 `WEIGHT` 计数。

## Windows + Altium Designer 手工验证清单

自动化测试只能确认 OLE 流、记录字段、压缩数据和来源链路满足当前读取器的结构约束，不能替代目标版本 Altium Designer 的实际打开验证。发布前应在 Windows 环境使用与用户相同或更高版本的 Altium Designer 执行以下检查：

1. 使用 EasyKiConverter 从 EasyEDA fixture 或真实元件导出 `.SchLib`，并保存导出日志及详细报告。
2. 在 Altium Designer 中打开库，确认没有损坏提示、恢复提示或无法解析的记录警告。
3. 放置单部件符号，检查引脚名称、编号、方向、电气类型、圆点/时钟装饰、圆角矩形、原生曲线、文本和图片显示。
4. 放置多部件符号，分别切换公共 Part、Part A 和 Part B，确认公共图元在所有部件显示，普通图元和参数只属于对应部件。
5. 检查参数编辑器中的 Value、Description、制造商字段和自定义参数，确认字体族、字号、粗体、斜体、显示状态和只读状态符合预期。
6. 对包含重复图片文件名、非 ASCII 文本和长文件名的库执行保存、关闭、重新打开，确认图片仍可显示且文件名没有碰撞。
7. 将库保存为新的 Altium 文件后重新打开，并再次运行读取器测试或人工检查，确认保存过程没有丢失图元、参数和部件归属。

每次人工验证应记录 Altium Designer 版本、Windows 架构、输入 fixture/元件 ID、输出文件哈希和结果；未完成上述实机检查前，不应在发布说明中宣称已通过 Altium Designer 兼容性验证。
