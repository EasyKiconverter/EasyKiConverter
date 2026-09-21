#include "CommandLineParser.h"

#include <QCoreApplication>
#include <QTextStream>

namespace EasyKiConverter {

static const QStringList VALID_3D_MODEL_FORMATS = {"wrl", "step", "both"};
static const QStringList VALID_3D_PATH_MODES = {"relative", "absolute"};

CommandLineParser::CommandLineParser(int argc, char* argv[])
    : m_debugOption(QStringList() << "d" << "debug", "启用调试模式（显示详细日志和控制台窗口）")
    , m_logLevelOption(QStringList() << "log-level",
                       "设置日志级别 (trace/debug/info/warn/error/fatal)",
                       "level",
                       "info")
    , m_logFileOption(QStringList() << "log-file", "指定日志文件路径", "path")
    , m_configOption(QStringList() << "config", "指定配置文件路径", "path")
    , m_languageOption(QStringList() << "language", "设置界面语言 (zh_CN/en)", "lang", "zh_CN")
    , m_themeOption(QStringList() << "theme", "设置界面主题 (dark/light)", "theme", "dark")
    , m_portableOption(QStringList() << "portable", "便携模式（配置文件保存在程序目录）")
    , m_syncLoggingOption(QStringList() << "sync-logging", "启用同步控制台日志输出（确保彩色日志显示，方便调试）")
    , m_cacheDirOption(QStringList() << "cache-dir", "设置磁盘缓存目录", "path")
    , m_diskCacheLimitOption(QStringList() << "cache-size-mb", "设置磁盘缓存大小限制 (MB)", "mb")
    , m_inputOption(QStringList() << "i" << "input", "输入文件路径 (BOM 表或元器件列表文件)", "path")
    , m_outputOption(QStringList() << "o" << "output", "输出目录路径", "path")
    , m_libNameOption("lib-name", "导出库名称", "name", "EasyKiConverter")
    , m_componentOption(QStringList() << "c" << "component", "LCSC 元器件编号", "id")
    , m_symbolOption("symbol", "导出符号库 (默认: true)")
    , m_footprintOption("footprint", "导出封装库 (默认: true)")
    , m_3dModelOption("3d-model", "导出 3D 模型")
    , m_3dModelFormatOption("3d-model-format", "3D 模型格式 (wrl/step/both，默认: wrl)", "format", "wrl")
    , m_datasheetOption("datasheet", "导出数据手册")
    , m_previewOption("preview", "导出预览图")
    , m_progressOption("progress", "显示进度条")
    , m_quietOption(QStringList() << "q" << "quiet", "安静模式，减少输出")
    , m_weakNetworkOption("weak-network", "启用弱网模式（超时翻倍、增加重试次数、降低并发）")
    , m_updateModeOption("update-mode", "更新模式（仅导出缺失或已更改的文件）")
    , m_3dPathModeOption("3d-path-mode", "3D 模型路径模式 (relative/absolute，默认: relative)", "mode", "relative")
    , m_overwriteOption("no-overwrite", "不覆盖已存在的文件（默认: 覆盖）")
    , m_symbolDescriptionOption("symbol-description", "符号库描述文本", "text")
    , m_footprintDescriptionOption("footprint-description", "封装库描述文本", "text")
    , m_targetFormatOption("target-format",
                           "目标 EDA 格式 (kicad/altium/xpedition/allegro/pads/eagle/pcad，默认: kicad)",
                           "format",
                           "kicad")
    , m_completionOption("completion", "生成 Shell 补全脚本 (bash/zsh/fish)", "shell")
    , m_completeOption("complete", "内部选项：输出动态补全数据", "type") {
    m_parser.setApplicationDescription(
        QCoreApplication::translate("main", "EasyKiConverter - LCSC/EasyEDA 元件转 KiCad 库工具"));

    // Qt 内置的帮助和版本选项
    m_parser.addHelpOption();
    m_parser.addVersionOption();

    // 自定义选项
    setupOptions();

    // CLI 模式选项
    setupCliOptions();

    // 补全选项
    m_parser.addOption(m_completionOption);
    m_parser.addOption(m_completeOption);

    // 设置应用程序参数（用于帮助和版本信息）
    // 重要：在 QCoreApplication 创建前存储参数，以确保 CLI 模式检测可靠
    m_argc = argc;
    m_argv = argv;
}

// 注册通用应用选项。
void CommandLineParser::setupOptions() {
    // 所有自定义选项（帮助和版本选项已在构造函数中通过 addHelpOption 和 addVersionOption 添加）
    m_parser.addOption(m_debugOption);
    m_parser.addOption(m_logLevelOption);
    m_parser.addOption(m_logFileOption);
    m_parser.addOption(m_configOption);
    m_parser.addOption(m_languageOption);
    m_parser.addOption(m_themeOption);
    m_parser.addOption(m_portableOption);
    m_parser.addOption(m_syncLoggingOption);
    m_parser.addOption(m_cacheDirOption);
    m_parser.addOption(m_diskCacheLimitOption);
}

// 注册 CLI 转换相关选项。
void CommandLineParser::setupCliOptions() {
    m_parser.addOption(m_inputOption);
    m_parser.addOption(m_outputOption);
    m_parser.addOption(m_libNameOption);
    m_parser.addOption(m_componentOption);
    m_parser.addOption(m_symbolOption);
    m_parser.addOption(m_footprintOption);
    m_parser.addOption(m_3dModelOption);
    m_parser.addOption(m_3dModelFormatOption);
    m_parser.addOption(m_datasheetOption);
    m_parser.addOption(m_previewOption);
    m_parser.addOption(m_progressOption);
    m_parser.addOption(m_quietOption);
    m_parser.addOption(m_weakNetworkOption);
    m_parser.addOption(m_updateModeOption);
    m_parser.addOption(m_3dPathModeOption);
    m_parser.addOption(m_overwriteOption);
    m_parser.addOption(m_symbolDescriptionOption);
    m_parser.addOption(m_footprintDescriptionOption);
    m_parser.addOption(m_targetFormatOption);
}

// 解析保存的命令行参数并识别转换子命令。
bool CommandLineParser::parse() {
    // 重要：使用存储的 argv 而非 QCoreApplication::arguments()
    // 因为此函数可能在 QCoreApplication 创建前被调用
    QStringList args;
    if (m_argv) {
        args.reserve(m_argc);
        for (int i = 0; i < m_argc; ++i) {
            args.append(QString::fromLocal8Bit(m_argv[i]));
        }
    }

    bool result = m_parser.parse(args);

    if (result) {
        // 检测 CLI 子命令
        // 使用 args 而非 QCoreApplication::arguments() 保证可靠性

        // 检查是否包含 "convert" 命令
        for (int i = 1; i < args.size(); ++i) {
            QString arg = args[i].toLower();
            if (arg == "convert") {
                m_hasConvertCommand = true;
                // 检查下一个参数
                if (i + 1 < args.size()) {
                    QString subcommand = args[i + 1].toLower();
                    if (subcommand == "bom") {
                        m_hasBomSubcommand = true;
                        m_cliMode = CliMode::ConvertBom;
                    } else if (subcommand == "component") {
                        m_hasComponentSubcommand = true;
                        m_cliMode = CliMode::ConvertComponent;
                    } else if (subcommand == "batch") {
                        m_hasBatchSubcommand = true;
                        m_cliMode = CliMode::ConvertBatch;
                    }
                }
                break;
            }
        }
    }

    return result;
}

// 返回是否启用了调试模式选项。
bool CommandLineParser::isDebugMode() const {
    return m_parser.isSet(m_debugOption);
}

// 返回规范化后的日志级别。
QString CommandLineParser::logLevel() const {
    return m_parser.value(m_logLevelOption).toLower();
}

// 返回日志文件路径。
QString CommandLineParser::logFile() const {
    return m_parser.value(m_logFileOption);
}

// 返回配置文件路径。
QString CommandLineParser::configFile() const {
    return m_parser.value(m_configOption);
}

// 返回命令行指定的语言。
QString CommandLineParser::language() const {
    return m_parser.value(m_languageOption);
}

// 返回规范化后的主题名称。
QString CommandLineParser::theme() const {
    return m_parser.value(m_themeOption).toLower();
}

// 判断是否显式设置了主题。
bool CommandLineParser::isThemeSet() const {
    return m_parser.isSet(m_themeOption);
}

// 判断是否启用了便携模式。
bool CommandLineParser::isPortableMode() const {
    return m_parser.isSet(m_portableOption);
}

// 判断是否启用了同步日志。
bool CommandLineParser::isSyncLogging() const {
    return m_parser.isSet(m_syncLoggingOption);
}

// 判断是否显式设置了缓存目录。
bool CommandLineParser::isCacheDirSet() const {
    return m_parser.isSet(m_cacheDirOption);
}

// 返回命令行指定的缓存目录。
QString CommandLineParser::cacheDir() const {
    return m_parser.value(m_cacheDirOption);
}

// 判断是否显式设置了磁盘缓存上限。
bool CommandLineParser::isDiskCacheLimitSet() const {
    return m_parser.isSet(m_diskCacheLimitOption);
}

// 返回磁盘缓存上限，单位为 MB。
int CommandLineParser::diskCacheLimitMB() const {
    return m_parser.value(m_diskCacheLimitOption).toInt();
}

// 返回通用命令行帮助文本。
QString CommandLineParser::helpText() const {
    return m_parser.helpText();
}

// 判断是否请求显示帮助。
bool CommandLineParser::isHelpRequested() const {
    // QCommandLineParser 的 addHelpOption() 会添加一个帮助选项
    // 我们需要检查该选项是否被设置
    return m_parser.isSet("help");
}

// 判断是否请求显示版本。
bool CommandLineParser::isVersionRequested() const {
    // QCommandLineParser 的 addVersionOption() 会添加一个版本选项
    // 我们需要检查该选项是否被设置
    return m_parser.isSet("version");
}

// 返回未被选项消费的位置参数。
QStringList CommandLineParser::positionalArguments() const {
    return m_parser.positionalArguments();
}

// 校验通用选项和 CLI 转换选项的组合是否合法。
bool CommandLineParser::validate() const {
    // 验证日志级别
    if (m_parser.isSet(m_logLevelOption)) {
        QString level = logLevel();
        QStringList validLevels = {"trace", "debug", "info", "warn", "error", "fatal"};
        if (!validLevels.contains(level)) {
            return false;
        }
    }

    // 验证语言设置
    if (m_parser.isSet(m_languageOption)) {
        QString lang = language();
        QStringList validLangs = {"zh_CN", "en"};
        if (!validLangs.contains(lang)) {
            return false;
        }
    }

    // 验证主题设置
    if (m_parser.isSet(m_themeOption)) {
        QString theme = this->theme();
        QStringList validThemes = {"dark", "light"};
        if (!validThemes.contains(theme)) {
            return false;
        }
    }

    // CLI 模式验证
    if (m_parser.isSet(m_cacheDirOption) && cacheDir().trimmed().isEmpty()) {
        return false;
    }

    if (m_parser.isSet(m_diskCacheLimitOption)) {
        bool ok = false;
        const int sizeMB = m_parser.value(m_diskCacheLimitOption).toInt(&ok);
        if (!ok || sizeMB <= 0) {
            return false;
        }
    }

    // CLI 模式验证
    if (isCliMode()) {
        if (m_parser.isSet(m_3dModelFormatOption)) {
            if (!VALID_3D_MODEL_FORMATS.contains(model3DFormat())) {
                return false;
            }
        }

        if (m_parser.isSet(m_3dPathModeOption)) {
            if (!VALID_3D_PATH_MODES.contains(model3DPathMode())) {
                return false;
            }
        }

        // 验证输出目录
        if (!m_parser.isSet(m_outputOption)) {
            return false;
        }

        // BOM 转换需要输入文件
        if (m_cliMode == CliMode::ConvertBom && !m_parser.isSet(m_inputOption)) {
            return false;
        }

        // 单个元器件转换需要组件编号
        if (m_cliMode == CliMode::ConvertComponent && !m_parser.isSet(m_componentOption)) {
            return false;
        }

        // 批量转换需要输入文件
        if (m_cliMode == CliMode::ConvertBatch && !m_parser.isSet(m_inputOption)) {
            return false;
        }
    }

    return true;
}

// 汇总命令行参数校验错误，供 CLI 调用方展示。
QString CommandLineParser::validationError() const {
    QStringList errors;

    // 检查日志级别
    if (m_parser.isSet(m_logLevelOption)) {
        QString level = logLevel();
        QStringList validLevels = {"trace", "debug", "info", "warn", "error", "fatal"};
        if (!validLevels.contains(level)) {
            errors.append(QCoreApplication::translate("CommandLineParser", "无效的日志级别: %1（有效值: %2）")
                              .arg(level)
                              .arg(validLevels.join(", ")));
        }
    }

    // 检查语言设置
    if (m_parser.isSet(m_languageOption)) {
        QString lang = language();
        QStringList validLangs = {"zh_CN", "en"};
        if (!validLangs.contains(lang)) {
            errors.append(QCoreApplication::translate("CommandLineParser", "无效的语言设置: %1（有效值: %2）")
                              .arg(lang)
                              .arg(validLangs.join(", ")));
        }
    }

    // 检查主题设置
    if (m_parser.isSet(m_themeOption)) {
        QString theme = this->theme();
        QStringList validThemes = {"dark", "light"};
        if (!validThemes.contains(theme)) {
            errors.append(QCoreApplication::translate("CommandLineParser", "无效的主题设置: %1（有效值: %2）")
                              .arg(theme)
                              .arg(validThemes.join(", ")));
        }
    }

    // CLI 模式验证错误
    if (m_parser.isSet(m_cacheDirOption) && cacheDir().trimmed().isEmpty()) {
        errors.append(QCoreApplication::translate("CommandLineParser", "缓存目录不能为空"));
    }

    if (m_parser.isSet(m_diskCacheLimitOption)) {
        bool ok = false;
        const int sizeMB = m_parser.value(m_diskCacheLimitOption).toInt(&ok);
        if (!ok || sizeMB <= 0) {
            errors.append(
                QCoreApplication::translate("CommandLineParser", "磁盘缓存大小必须是大于 0 的整数（单位: MB）"));
        }
    }

    // CLI 模式验证错误
    if (isCliMode()) {
        if (m_parser.isSet(m_3dModelFormatOption)) {
            const QString format = model3DFormat();
            if (!VALID_3D_MODEL_FORMATS.contains(format)) {
                errors.append(QCoreApplication::translate("CommandLineParser", "无效的 3D 模型格式: %1（有效值: %2）")
                                  .arg(format)
                                  .arg(VALID_3D_MODEL_FORMATS.join(", ")));
            }
        }

        if (m_parser.isSet(m_3dPathModeOption)) {
            const QString pathMode = model3DPathMode();
            if (!VALID_3D_PATH_MODES.contains(pathMode)) {
                errors.append(
                    QCoreApplication::translate("CommandLineParser", "无效的 3D 模型路径模式: %1（有效值: %2）")
                        .arg(pathMode, VALID_3D_PATH_MODES.join(", ")));
            }
        }

        if (!m_parser.isSet(m_outputOption)) {
            errors.append(QCoreApplication::translate("CommandLineParser", "CLI 模式必须指定输出目录 (-o/--output)"));
        }

        if (m_cliMode == CliMode::ConvertBom && !m_parser.isSet(m_inputOption)) {
            errors.append(QCoreApplication::translate("CommandLineParser", "BOM 表转换必须指定输入文件 (-i/--input)"));
        }

        if (m_cliMode == CliMode::ConvertComponent && !m_parser.isSet(m_componentOption)) {
            errors.append(
                QCoreApplication::translate("CommandLineParser", "单个元器件转换必须指定 LCSC 编号 (-c/--component)"));
        }

        if (m_cliMode == CliMode::ConvertBatch && !m_parser.isSet(m_inputOption)) {
            errors.append(QCoreApplication::translate("CommandLineParser", "批量转换必须指定输入文件 (-i/--input)"));
        }
    }

    return errors.join("\n");
}

// ========== CLI 模式相关方法实现 ==========

// 判断当前是否进入 CLI 转换模式。
bool CommandLineParser::isCliMode() const {
    return m_cliMode != CliMode::None;
}

// 返回已识别的 CLI 转换模式。
CommandLineParser::CliMode CommandLineParser::cliMode() const {
    return m_cliMode;
}

// 返回 CLI 输入文件路径。
QString CommandLineParser::inputFile() const {
    return m_parser.value(m_inputOption);
}

// 返回 CLI 输出目录路径。
QString CommandLineParser::outputDir() const {
    return m_parser.value(m_outputOption);
}

// 返回 CLI 导出库名称。
QString CommandLineParser::libName() const {
    return m_parser.value(m_libNameOption);
}

// 返回 CLI 指定的元器件编号。
QString CommandLineParser::componentId() const {
    return m_parser.value(m_componentOption);
}

// 返回符号导出开关，未设置时默认开启。
bool CommandLineParser::exportSymbol() const {
    // 默认为 true，除非显式设置为 false
    return !m_parser.isSet(m_symbolOption) || m_parser.value(m_symbolOption).toLower() != "false";
}

// 返回封装导出开关，未设置时默认开启。
bool CommandLineParser::exportFootprint() const {
    // 默认为 true，除非显式设置为 false
    return !m_parser.isSet(m_footprintOption) || m_parser.value(m_footprintOption).toLower() != "false";
}

// 返回三维模型导出开关。
bool CommandLineParser::export3DModel() const {
    // --3d-model 是 flag 选项，默认 false（未设置则不导出 3D 模型）
    return m_parser.isSet(m_3dModelOption);
}

// 返回规范化后的三维模型格式。
QString CommandLineParser::model3DFormat() const {
    return m_parser.value(m_3dModelFormatOption).toLower();
}

// 返回数据手册导出开关。
bool CommandLineParser::exportDatasheet() const {
    return m_parser.isSet(m_datasheetOption);
}

// 返回预览图导出开关。
bool CommandLineParser::exportPreview() const {
    return m_parser.isSet(m_previewOption);
}

// 返回进度显示开关。
bool CommandLineParser::showProgress() const {
    return m_parser.isSet(m_progressOption);
}

// 返回安静模式开关。
bool CommandLineParser::isQuietMode() const {
    return m_parser.isSet(m_quietOption);
}

// 返回弱网络适配开关。
bool CommandLineParser::weakNetworkSupport() const {
    return m_parser.isSet(m_weakNetworkOption);
}

// 返回更新导出模式开关。
bool CommandLineParser::updateMode() const {
    return m_parser.isSet(m_updateModeOption);
}

// 返回规范化后的三维模型路径模式。
QString CommandLineParser::model3DPathMode() const {
    return m_parser.value(m_3dPathModeOption).toLower();
}

// 返回是否允许覆盖已有文件。
bool CommandLineParser::overwriteExistingFiles() const {
    // 默认 true，--no-overwrite 禁用
    return !m_parser.isSet(m_overwriteOption);
}

// 返回符号库描述文本。
QString CommandLineParser::symbolDescription() const {
    return m_parser.value(m_symbolDescriptionOption);
}

// 返回封装库描述文本。
QString CommandLineParser::footprintDescription() const {
    return m_parser.value(m_footprintDescriptionOption);
}

// 返回规范化后的目标 EDA 格式。
QString CommandLineParser::targetFormat() const {
    return m_parser.value(m_targetFormatOption).toLower();
}

// 生成 CLI 子命令、选项和示例的帮助文本。
QString CommandLineParser::cliHelpText() const {
    QString help;
    QTextStream stream(&help);

    stream << QCoreApplication::translate("CommandLineParser", "EasyKiConverter CLI 模式") << "\n\n";
    stream << QCoreApplication::translate("CommandLineParser", "用法:") << "\n";
    stream << "  easykiconverter convert " << QCoreApplication::translate("CommandLineParser", "<子命令> [选项]")
           << "\n\n";
    stream << QCoreApplication::translate("CommandLineParser", "子命令:") << "\n";
    stream << "  bom        " << QCoreApplication::translate("CommandLineParser", "转换 BOM 表文件") << "\n";
    stream << "  component  " << QCoreApplication::translate("CommandLineParser", "转换单个元器件（通过 LCSC 编号）")
           << "\n";
    stream << "  batch      "
           << QCoreApplication::translate("CommandLineParser", "批量转换元器件（通过元器件列表文件）") << "\n\n";
    stream << QCoreApplication::translate("CommandLineParser", "选项:") << "\n";
    stream << "  -i, --input <path>      "
           << QCoreApplication::translate("CommandLineParser", "输入文件路径（BOM 表或元器件列表文件）") << "\n";
    stream << "  -o, --output <path>     " << QCoreApplication::translate("CommandLineParser", "输出目录路径（必需）")
           << "\n";
    stream << "  --lib-name <name>       "
           << QCoreApplication::translate("CommandLineParser", "导出库名称（默认: EasyKiConverter）") << "\n";
    stream << "  -c, --component <id>    " << QCoreApplication::translate("CommandLineParser", "LCSC 元器件编号")
           << "\n";
    stream << "  --symbol                "
           << QCoreApplication::translate("CommandLineParser", "导出符号库（默认: true）") << "\n";
    stream << "  --footprint             "
           << QCoreApplication::translate("CommandLineParser", "导出封装库（默认: true）") << "\n";
    stream << "  --3d-model              " << QCoreApplication::translate("CommandLineParser", "导出 3D 模型") << "\n";
    stream << "  --3d-model-format <fmt> "
           << QCoreApplication::translate("CommandLineParser", "3D 模型格式（wrl/step/both，默认: wrl）") << "\n";
    stream << "  --datasheet             " << QCoreApplication::translate("CommandLineParser", "导出数据手册") << "\n";
    stream << "  --preview               " << QCoreApplication::translate("CommandLineParser", "导出预览图") << "\n";
    stream << "  --cache-dir <path>      " << QCoreApplication::translate("CommandLineParser", "设置磁盘缓存目录")
           << "\n";
    stream << "  --cache-size-mb <mb>    "
           << QCoreApplication::translate("CommandLineParser", "设置磁盘缓存大小限制 (MB)") << "\n";
    stream << "  --progress              " << QCoreApplication::translate("CommandLineParser", "显示进度条") << "\n";
    stream << "  -q, --quiet             " << QCoreApplication::translate("CommandLineParser", "安静模式，减少输出")
           << "\n";
    stream << "  --weak-network          "
           << QCoreApplication::translate("CommandLineParser", "启用弱网模式（超时翻倍、增加重试、降低并发）") << "\n";
    stream << "  --update-mode           "
           << QCoreApplication::translate("CommandLineParser", "更新模式（仅导出缺失或已更改的文件）") << "\n";
    stream << "  --3d-path-mode <mode>   "
           << QCoreApplication::translate("CommandLineParser", "3D 模型路径模式（relative/absolute，默认: relative）")
           << "\n";
    stream << "  --no-overwrite          "
           << QCoreApplication::translate("CommandLineParser", "不覆盖已存在的文件（默认: 覆盖）") << "\n";
    stream << "  --symbol-description <t> " << QCoreApplication::translate("CommandLineParser", "符号库描述文本")
           << "\n";
    stream << "  --footprint-description <t> " << QCoreApplication::translate("CommandLineParser", "封装库描述文本")
           << "\n\n";
    stream << QCoreApplication::translate("CommandLineParser", "示例:") << "\n";
    stream << "  # " << QCoreApplication::translate("CommandLineParser", "转换 BOM 表") << "\n";
    stream << "  easykiconverter convert bom -i my_project.xlsx -o ./kicad_libs\n\n";
    stream << "  # " << QCoreApplication::translate("CommandLineParser", "转换单个元器件") << "\n";
    stream << "  easykiconverter convert component -c C12345 -o ./output\n\n";
    stream << "  # " << QCoreApplication::translate("CommandLineParser", "批量转换") << "\n";
    stream << "  easykiconverter convert batch -i components.txt -o ./output --3d-model\n";

    return help;
}

// ========== 补全相关方法实现 ==========

// 判断是否请求生成补全脚本。
bool CommandLineParser::isCompletionRequested() const {
    return m_parser.isSet(m_completionOption);
}

// 返回规范化后的补全 Shell 类型。
QString CommandLineParser::completionShell() const {
    return m_parser.value(m_completionOption).toLower();
}

// 判断是否请求执行补全查询。
bool CommandLineParser::isCompleteRequested() const {
    return m_parser.isSet(m_completeOption);
}

// 返回补全查询类型。
QString CommandLineParser::completeType() const {
    return m_parser.value(m_completeOption).toLower();
}

// 判断命令行是否包含 convert 主命令。
bool CommandLineParser::hasConvertCommand() const {
    return m_hasConvertCommand;
}

}  // namespace EasyKiConverter
