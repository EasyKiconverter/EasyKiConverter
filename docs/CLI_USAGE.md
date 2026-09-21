# CLI 模式使用说明

EasyKiConverter 支持命令行模式，可以在不打开 UI 界面的情况下进行转换操作。

## 子命令

```bash
# 转换 BOM 表
easykiconverter convert bom -i <bom_file> -o <output_dir> [options]

# 转换单个元器件
easykiconverter convert component -c <lcsc_id> -o <output_dir> [options]

# 批量转换
easykiconverter convert batch -i <component_list_file> -o <output_dir> [options]
```

## 通用参数

| 参数 | 简写 | 描述 | 默认值 |
|------|------|------|--------|
| `--input` | `-i` | 输入文件路径 | - |
| `--output` | `-o` | 输出目录路径 | - |
| `--lib-name` | | 导出库名称 | EasyKiConverter |
| `--component` | `-c` | LCSC 元器件编号 | - |
| `--target-format` | | 目标格式（kicad/altium/xpedition/allegro/pads/eagle/pcad） | kicad |
| `--symbol` | | 导出符号库 | true |
| `--footprint` | | 导出封装库 | true |
| `--3d-model` | | 导出 3D 模型（默认 WRL 格式） | false |
| `--3d-model-format` | | 3D 模型格式（wrl/step/both） | wrl |
| `--datasheet` | | 导出数据手册 | false |
| `--preview` | | 导出预览图 | false |
| `--3d-path-mode` | | 3D 模型路径模式（relative/absolute） | relative |
| `--symbol-description` | | 符号库描述文本 | - |
| `--footprint-description` | | 封装库描述文本 | - |
| `--weak-network` | | 弱网模式（超时翻倍、增加重试、降低并发） | false |
| `--update-mode` | | 更新模式（仅导出缺失或已更改的文件） | false |
| `--no-overwrite` | | 不覆盖已存在的文件 | false |
| `--cache-dir` | | 设置磁盘缓存目录 | 配置文件中的缓存目录 |
| `--cache-size-mb` | | 设置磁盘缓存大小上限（MB） | 配置文件中的缓存上限 |
| `--progress` | | 显示进度条 | false |
| `--quiet` | `-q` | 安静模式 | false |
| `--debug` | `-d` | 调试模式 (生成详细报告) | false |
| `--completion` | | 生成 Shell 补全脚本 | - |

## 默认导出选项

CLI 模式默认导出以下内容：
- 符号库 (Symbol)
- 封装库 (Footprint)

**注意**：
- 默认不导出 3D 模型、预览图和数据手册
- 需要 3D 模型时传入 `--3d-model`；KiCad 默认使用 WRL，Altium 会自动收敛为 STEP 并嵌入 PcbLib
- Xpedition 当前只导出符号和封装 ZIP 包，不支持 3D 模型关联；传入 `--3d-model` 时会记录告警并跳过 3D 阶段
- PADS 当前导出 ASCII Schematic Decal 符号、Part Type 器件关联和 PCB Decal 封装；传入 `--3d-model` 时由独立阶段输出 WRL/STEP 文件，更新、追加和重试模式会被拒绝
- Eagle 当前可导出包含 Symbol、Package、DeviceSet 和引脚到焊盘关联的 XML `.lbr` 库；`--3d-model` 由独立阶段输出 WRL/STEP 文件，但不会伪造受管 `package3d` 关联，更新、追加和重试模式会被拒绝
- P-CAD 当前只导出 ASCII PCB Library（`.lia`）的 Pad Style 和 Pattern，不支持符号、Component/Part 关联和原生 3D 模型关联；传入 `--3d-model` 时由独立阶段输出 WRL/STEP 文件，更新、追加和重试模式会被拒绝
- 需要数据手册时传入 `--datasheet`
- 普通模式不生成详细报告，仅在调试模式 (`--debug`) 下生成

## 缓存配置

CLI 模式会复用与 GUI 相同的元器件磁盘缓存。可以通过命令行覆盖本次运行使用的缓存目录和缓存大小上限：

```bash
easykiconverter convert component -c C12345 -o ./output \
  --cache-dir /tmp/easykiconverter-cache \
  --cache-size-mb 2048
```

说明：
- `--cache-dir` 指定本次运行使用的缓存根目录。
- `--cache-size-mb` 的有效范围为 `1` 到 `1048576`。
- 在 GUI 中修改缓存目录时，应用会尝试将旧缓存迁移到新目录以复用已下载数据；目标目录已有同名文件时不会覆盖。
- 3D 模型缓存保存在缓存目录的 `model3d` 子目录，默认不参与 LRU 容量淘汰，避免频繁重新下载大文件。

## 自动补全

EasyKiConverter 支持 Shell 自动补全，包括参数选项和动态补全 LCSC ID。

### Bash 补全

```bash
# 临时启用 (当前会话)
eval "$(easykiconverter --completion bash)"

# 永久启用
easykiconverter --completion bash >> ~/.bash_completion
echo 'source ~/.bash_completion' >> ~/.bashrc
```

### Zsh 补全

```bash
# 临时启用 (当前会话)
eval "$(easykiconverter --completion zsh)"

# 永久启用
mkdir -p ~/.zsh
easykiconverter --completion zsh > ~/.zsh/_easykiconverter
echo 'fpath=(~/.zsh $fpath)' >> ~/.zshrc
echo 'autoload -Uz compinit && compinit' >> ~/.zshrc
```

### Fish 补全

```bash
# 永久启用
easykiconverter --completion fish > ~/.config/fish/completions/easykiconverter.fish
```

### 补全功能

- 参数选项补全：自动补全 `--help`, `--input`, `--output` 等选项
- 子命令补全：自动补全 `convert`, `bom`, `component`, `batch` 等子命令
- 文件补全：输入 `-i` 后自动补全 `.xlsx`, `.csv` 等文件
- 目录补全：输入 `-o` 后自动补全目录路径
- **LCSC ID 动态补全**：输入 `-c` 后自动补全已缓存的元器件编号

## 使用示例

```bash
# 默认导出 (符号库 + 封装库)
easykiconverter convert bom -i my_project.xlsx -o ./kicad_libs

# 导出 KiCad 符号库、封装库和 3D 模型（默认 WRL 格式）
easykiconverter convert bom -i my_project.xlsx -o ./output --3d-model

# 导出 Altium SchLib/PcbLib（3D 模型固定嵌入 STEP）
easykiconverter convert bom -i my_project.xlsx -o ./output --target-format altium --3d-model

# 导出所有内容，包括预览图
easykiconverter convert bom -i my_project.xlsx -o ./output --3d-model --preview

# 调试模式 (生成详细报告)
easykiconverter convert bom -i my_project.xlsx -o ./output --debug

# 带进度条
easykiconverter convert bom -i bom.xlsx -o ./output --progress

# 使用自定义缓存目录和 2GB 磁盘缓存上限
easykiconverter convert component -c C12345 -o ./output --cache-dir /tmp/ekc-cache --cache-size-mb 2048

# 转换单个元器件
easykiconverter convert component -c C12345 -o ./output

# 批量转换
easykiconverter convert batch -i components.txt -o ./output

# 导出 STEP 格式 3D 模型
easykiconverter convert bom -i my_project.xlsx -o ./output --3d-model --3d-model-format step

# 导出数据手册
easykiconverter convert bom -i my_project.xlsx -o ./output --datasheet

# 使用绝对路径引用 3D 模型
easykiconverter convert bom -i my_project.xlsx -o ./output --3d-model --3d-path-mode absolute

# 不覆盖已存在的文件
easykiconverter convert bom -i my_project.xlsx -o ./output --no-overwrite

# 仅导出缺失或已更改的文件（更新模式）
easykiconverter convert bom -i my_project.xlsx -o ./output --update-mode

# 弱网模式（超时翻倍、增加重试、降低并发）
easykiconverter convert bom -i my_project.xlsx -o ./output --weak-network

# 指定库描述文本
easykiconverter convert bom -i my_project.xlsx -o ./output --symbol-description "My Symbol Lib" --footprint-description "My Footprint Lib"

# 指定库名称
easykiconverter convert bom -i my_project.xlsx -o ./output --lib-name my_lib
```

## 架构说明

CLI 模块采用低耦合设计，包含以下组件：

- `CommandLineParser` - 命令行参数解析器
- `CliContext` - CLI 上下文，保存共享状态
- `CliPrinter` - CLI 输出工具
- `FileReader` - 文件读取工具
- `BaseConverter` - 转换器基类
- `BomConverter` - BOM 表转换器
- `ComponentConverter` - 单个元器件转换器
- `BatchConverter` - 批量转换器
- `CliConverter` - 主协调器
- `CompletionGenerator` - Shell 补全脚本生成器
