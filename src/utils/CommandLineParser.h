#ifndef COMMANDLINEPARSER_H
#define COMMANDLINEPARSER_H

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief 命令行参数解析器
 *
 * 提供命令行参数解析功能，支持以下参数：
 * - --debug, -d: 启用调试模式
 * - --log-level: 设置日志级别 (trace/debug/info/warn/error/fatal)
 * - --log-file: 指定日志文件路径
 * - --config: 指定配置文件路径
 * - --language: 设置语言 (zh_CN/en)
 * - --theme: 设置主题 (dark/light)
 * - --help, -h: 显示帮助信息
 * - --version, -v: 显示版本信息
 * - --portable: 便携模式（配置文件保存在程序目录）
 * - --sync-logging: 启用同步控制台日志输出（确保彩色日志显示，方便调试）
 *
 * CLI 模式子命令：
 * - convert bom: 转换 BOM 表
 * - convert component: 转换单个元器件
 * - convert batch: 批量转换
 */
class CommandLineParser {
public:
    /**
     * @brief CLI 模式枚举
     */
    enum class CliMode {
        None,  // 无 CLI 模式，启动 GUI
        ConvertBom,  // 转换 BOM 表
        ConvertComponent,  // 转换单个元器件
        ConvertBatch  // 批量转换
    };

    /**
     * @brief 构造函数
     * @param argc 参数数量
     * @param argv 参数列表
     */
    CommandLineParser(int argc, char* argv[]);

    /**
     * @brief 解析命令行参数
     * @return 解析成功返回 true，否则返回 false
     */
    bool parse();

    /**
     * @brief 验证参数值的有效性
     * @return 所有参数有效返回 true，否则返回 false
     */
    bool validate() const;

    /**
     * @brief 获取验证错误信息
     * @return 错误信息字符串
     */
    QString validationError() const;

    /**
     * @brief 是否启用调试模式
     * @return 启用返回 true，否则返回 false
     */
    bool isDebugMode() const;

    /**
     * @brief 获取日志级别
     * @return 日志级别字符串
     */
    QString logLevel() const;

    /**
     * @brief 获取日志文件路径
     * @return 日志文件路径
     */
    QString logFile() const;

    /**
     * @brief 获取配置文件路径
     * @return 配置文件路径
     */
    QString configFile() const;

    /**
     * @brief 获取语言设置
     * @return 语言代码 (zh_CN/en)
     */
    QString language() const;

    /**
     * @brief 获取主题设置
     * @return 主题名称 (dark/light)
     */
    QString theme() const;

    /**
     * @brief 是否显式指定了主题参数
     * @return 用户指定了 --theme 参数返回 true，否则返回 false
     */
    bool isThemeSet() const;

    /**
     * @brief 是否为便携模式
     * @return 便携模式返回 true，否则返回 false
     */
    bool isPortableMode() const;

    /**
     * @brief 是否启用同步控制台日志输出
     * @return 启用返回 true，否则返回 false
     */
    bool isSyncLogging() const;

    /**
     * @brief 是否显式指定了缓存目录
     * @return 指定返回 true，否则返回 false
     */
    bool isCacheDirSet() const;

    /**
     * @brief 获取缓存目录
     * @return 缓存目录路径
     */
    QString cacheDir() const;

    /**
     * @brief 是否显式指定了磁盘缓存大小
     * @return 指定返回 true，否则返回 false
     */
    bool isDiskCacheLimitSet() const;

    /**
     * @brief 获取磁盘缓存大小限制
     * @return 缓存大小限制（MB）
     */
    int diskCacheLimitMB() const;

    /**
     * @brief 获取帮助文本
     * @return 帮助文本
     */
    QString helpText() const;

    /**
     * @brief 是否显示帮助信息
     * @return 显示返回 true，否则返回 false
     */
    bool isHelpRequested() const;

    /**
     * @brief 是否显示版本信息
     * @return 显示返回 true，否则返回 false
     */
    bool isVersionRequested() const;

    /**
     * @brief 获取剩余的未解析参数
     * @return 未解析参数列表
     */
    QStringList positionalArguments() const;

    // ========== CLI 模式相关方法 ==========

    /**
     * @brief 是否处于 CLI 模式
     * @return CLI 模式返回 true，否则返回 false
     */
    bool isCliMode() const;

    /**
     * @brief 获取 CLI 模式
     * @return CLI 模式枚举
     */
    CliMode cliMode() const;

    /**
     * @brief 获取输入文件路径（用于 BOM 表和批量转换）
     * @return 输入文件路径
     */
    QString inputFile() const;

    /**
     * @brief 获取输出目录路径
     * @return 输出目录路径
     */
    QString outputDir() const;

    /**
     * @brief 获取导出库名称
     * @return 库名称
     */
    QString libName() const;

    /**
     * @brief 获取 LCSC 元器件编号（用于单个元器件转换）
     * @return LCSC 编号
     */
    QString componentId() const;

    /**
     * @brief 是否导出符号库
     * @return 导出返回 true，否则返回 false
     */
    bool exportSymbol() const;

    /**
     * @brief 是否导出封装库
     * @return 导出返回 true，否则返回 false
     */
    bool exportFootprint() const;

    /**
     * @brief 是否导出 3D 模型
     * @return 导出返回 true，否则返回 false
     */
    bool export3DModel() const;

    /**
     * @brief 获取 3D 模型导出格式
     * @return 格式名称 (wrl/step/both)
     */
    QString model3DFormat() const;

    /**
     * @brief 是否导出数据手册
     * @return 导出返回 true，否则返回 false
     */
    bool exportDatasheet() const;

    /**
     * @brief 是否导出预览图
     * @return 导出返回 true，否则返回 false
     */
    bool exportPreview() const;

    /**
     * @brief 是否显示进度条
     * @return 显示返回 true，否则返回 false
     */
    bool showProgress() const;

    /**
     * @brief 是否为安静模式
     * @return 安静模式返回 true，否则返回 false
     */
    bool isQuietMode() const;

    /**
     * @brief 是否启用弱网模式
     * @return 启用返回 true，否则返回 false
     */
    bool weakNetworkSupport() const;

    /**
     * @brief 是否启用更新模式（仅导出缺失或已更改的文件）
     * @return 启用返回 true，否则返回 false
     */
    bool updateMode() const;

    /**
     * @brief 获取 3D 模型路径模式
     * @return 路径模式名称 (relative/absolute)
     */
    QString model3DPathMode() const;

    /**
     * @brief 是否覆盖已存在的文件（默认 true，--no-overwrite 禁用）
     * @return 覆盖返回 true，否则返回 false
     */
    bool overwriteExistingFiles() const;

    /**
     * @brief 获取符号库描述文本
     * @return 描述文本
     */
    QString symbolDescription() const;

    /**
     * @brief 获取封装库描述文本
     * @return 描述文本
     */
    QString footprintDescription() const;

    /**
     * @brief 获取目标 EDA 格式
     * @return 格式字符串（"kicad" 或 "altium"），默认 "kicad"
     */
    QString targetFormat() const;

    /**
     * @brief 获取 CLI 帮助文本
     * @return CLI 帮助文本
     */
    QString cliHelpText() const;

    // ========== 补全相关方法 ==========

    /**
     * @brief 是否请求生成补全脚本
     * @return 请求返回 true，否则返回 false
     */
    bool isCompletionRequested() const;

    /**
     * @brief 获取补全脚本类型 (bash/zsh/fish)
     * @return Shell 类型字符串
     */
    QString completionShell() const;

    /**
     * @brief 是否请求动态补全数据
     * @return 请求返回 true，否则返回 false
     */
    bool isCompleteRequested() const;

    /**
     * @brief 获取动态补全类型
     * @return 补全类型字符串 (如 "lcsc-id")
     */
    QString completeType() const;

    /**
     * @brief 是否包含 convert 命令（无论子命令是否有效）
     * @return convert 命令存在返回 true，否则返回 false
     */
    bool hasConvertCommand() const;

private:
    void setupOptions();
    void setupCliOptions();

    // 存储命令行参数（在 QCoreApplication 创建前获取）
    int m_argc = 0;  ///< 保存初始化阶段传入的命令行参数数量。
    char** m_argv = nullptr;  ///< 保存初始化阶段传入的命令行参数数组。

    QCommandLineParser m_parser;  ///< Qt 命令行参数解析器。
    QCommandLineOption m_debugOption;  ///< 调试模式选项。
    QCommandLineOption m_logLevelOption;  ///< 日志级别选项。
    QCommandLineOption m_logFileOption;  ///< 日志文件路径选项。
    QCommandLineOption m_configOption;  ///< 配置文件路径选项。
    QCommandLineOption m_languageOption;  ///< 界面语言选项。
    QCommandLineOption m_themeOption;  ///< 界面主题选项。
    QCommandLineOption m_portableOption;  ///< 便携模式选项。
    QCommandLineOption m_syncLoggingOption;  ///< 同步日志选项。
    QCommandLineOption m_cacheDirOption;  ///< 缓存目录选项。
    QCommandLineOption m_diskCacheLimitOption;  ///< 磁盘缓存上限选项。

    // CLI 模式选项
    QCommandLineOption m_inputOption;  ///< 输入文件或数据源选项。
    QCommandLineOption m_outputOption;  ///< 输出目录选项。
    QCommandLineOption m_libNameOption;  ///< 库名称选项。
    QCommandLineOption m_componentOption;  ///< 元器件编号列表选项。
    QCommandLineOption m_symbolOption;  ///< 符号库导出选项。
    QCommandLineOption m_footprintOption;  ///< 封装库导出选项。
    QCommandLineOption m_3dModelOption;  ///< 三维模型导出选项。
    QCommandLineOption m_3dModelFormatOption;  ///< 三维模型格式选项。
    QCommandLineOption m_datasheetOption;  ///< 数据手册导出选项。
    QCommandLineOption m_previewOption;  ///< 预览图导出选项。
    QCommandLineOption m_progressOption;  ///< 进度输出选项。
    QCommandLineOption m_quietOption;  ///< 静默输出选项。
    QCommandLineOption m_weakNetworkOption;  ///< 弱网络策略选项。
    QCommandLineOption m_updateModeOption;  ///< 导出更新模式选项。
    QCommandLineOption m_3dPathModeOption;  ///< 三维模型路径模式选项。
    QCommandLineOption m_overwriteOption;  ///< 覆盖已有文件选项。
    QCommandLineOption m_symbolDescriptionOption;  ///< 符号库描述选项。
    QCommandLineOption m_footprintDescriptionOption;  ///< 封装库描述选项。
    QCommandLineOption m_targetFormatOption;  ///< 目标格式选项。

    // 补全选项
    QCommandLineOption m_completionOption;  ///< Shell 补全脚本生成选项。
    QCommandLineOption m_completeOption;  ///< 动态补全数据请求选项。

    CliMode m_cliMode = CliMode::None;  ///< 当前解析得到的 CLI 模式。
    bool m_hasConvertCommand = false;  ///< 是否出现 convert 命令。
    bool m_hasBomSubcommand = false;  ///< 是否出现 BOM 子命令。
    bool m_hasComponentSubcommand = false;  ///< 是否出现 component 子命令。
    bool m_hasBatchSubcommand = false;  ///< 是否出现 batch 子命令。
};

}  // namespace EasyKiConverter

#endif  // COMMANDLINEPARSER_H
