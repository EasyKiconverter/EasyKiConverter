// 我发现我在学校，不谈恋爱也不学习，不打游戏也不逃课，
// 每天就是处于认真听课，然后因为听不懂走神了，
// 反应过来以后再继续认真听课的循环，进入这种状态的时候，
// 我一般就是左脑和右脑已经聊美了，完全就是一个放飞自我，
// 意淫我有多少多少家产，然后怕班里有人会读心术紧急撤回一条意淫，
// 那这在恋爱小说里我不就是那种…背景板吗，
// 就每天在学校挂机任务就完成的npc
#include "Version.h"
#include "core/LanguageManager.h"
#include "core/network/NetworkClient.h"
#include "services/ComponentCacheService.h"
#include "services/ConfigService.h"
#include "services/UpdateCheckerService.h"
#include "services/export/ExportProgress.h"
#include "services/export/ParallelExportService.h"
#include "services/export/TempFileManager.h"
#include "ui/viewmodels/ComponentListViewModel.h"
#include "ui/viewmodels/ExportProgressViewModel.h"
#include "ui/viewmodels/ExportSettingsViewModel.h"
#include "ui/viewmodels/ExportTargetModel.h"
#include "ui/viewmodels/ThemeSettingsViewModel.h"
#include "utils/CommandLineParser.h"
#include "utils/FileUtils.h"
#include "utils/cli/CliConverter.h"
#include "utils/cli/CompletionGenerator.h"
#include "utils/logging/Log.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPoint>
#include <QPointer>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QSize>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QWindow>

#include <cstdio>

#ifdef _WIN32
#    include <conio.h>
#    include <fcntl.h>
#    include <io.h>
#    include <windows.h>
#endif

namespace {

/** @brief 根据命令行参数和环境变量判断是否启用调试模式。 */
bool isDebugMode(const EasyKiConverter::CommandLineParser& parser) {
    // 优先检查命令行参数
    if (parser.isDebugMode()) {
        return true;
    }

    // 检查环境变量（向后兼容）
    if (qEnvironmentVariableIsSet("EASYKICONVERTER_DEBUG_MODE")) {
        QString debugValue = qEnvironmentVariable("EASYKICONVERTER_DEBUG_MODE", "false").toLower();
        return (debugValue == "true" || debugValue == "1" || debugValue == "yes");
    }
    return false;
}

/** @brief 解析命令行指定或便携模式使用的配置文件路径。 */
QString resolveConfigFilePath(const EasyKiConverter::CommandLineParser& parser) {
    if (!parser.configFile().isEmpty()) {
        return parser.configFile();
    }
    if (parser.isPortableMode()) {
        return QCoreApplication::applicationDirPath() + "/easykiconverter_config.json";
    }
    return QString();
}

/** @brief 将命令行中的缓存目录和容量设置应用到缓存服务。 */
void applyCacheConfiguration(const EasyKiConverter::CommandLineParser& parser) {
    auto* configService = EasyKiConverter::ConfigService::instance();
    auto* cacheService = EasyKiConverter::ComponentCacheService::instance();

    QString cacheDir = configService->getCacheDir();
    if (parser.isCacheDirSet()) {
        cacheDir = QDir::cleanPath(parser.cacheDir());
    }

    int diskCacheLimitMB = configService->getDiskCacheLimitMB();
    if (parser.isDiskCacheLimitSet()) {
        diskCacheLimitMB = parser.diskCacheLimitMB();
    }

    cacheService->setCacheDir(cacheDir);
    cacheService->setDiskCacheLimit(diskCacheLimitMB);
}

#ifdef _WIN32
/** @brief 在 Windows 调试控制台中重新打开标准输入输出流。 */
bool reopenConsoleStream(FILE* stream, const char* device, const char* mode, const char* streamName) {
    FILE* reopened = nullptr;
    errno_t err = freopen_s(&reopened, device, mode, stream);
    if (err != 0 || reopened == nullptr) {
        qCritical() << "Failed to reopen stream" << streamName << "to" << device << "error:" << err;
        return false;
    }
    return true;
}
#endif

/** @brief 初始化项目日志系统并配置平台相关的控制台输出。 */
void setupLogging(bool debugMode, const QString& logLevelStr, const QString& logFilePath, bool syncLogging) {
    using namespace EasyKiConverter;

#ifdef _WIN32
    // 如果启用调试模式，动态分配控制台（仅对 GUI 应用有效）
    if (debugMode) {
        // 尝试分配控制台
        if (AllocConsole()) {
            // 重定向标准输出
            (void)reopenConsoleStream(stdout, "CONOUT$", "w", "stdout");
            (void)reopenConsoleStream(stderr, "CONOUT$", "w", "stderr");
            (void)reopenConsoleStream(stdin, "CONIN$", "r", "stdin");

            // 设置控制台标题
            SetConsoleTitleA("EasyKiConverter - Debug Console");

            // 启用 ANSI 转义序列支持（彩色输出）
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE) {
                DWORD mode = 0;
                if (GetConsoleMode(hOut, &mode)) {
                    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                    SetConsoleMode(hOut, mode);
                }
            }
        }
    }
#endif

    auto logger = Logger::instance();

    // 解析日志级别

    LogLevel logLevel = LogLevel::Info;

    if (debugMode) {
        logLevel = LogLevel::Debug;

    } else if (!logLevelStr.isEmpty()) {
        if (logLevelStr == "trace") {
            logLevel = LogLevel::Trace;

        } else if (logLevelStr == "debug") {
            logLevel = LogLevel::Debug;

        } else if (logLevelStr == "info") {
            logLevel = LogLevel::Info;

        } else if (logLevelStr == "warn") {
            logLevel = LogLevel::Warn;

        } else if (logLevelStr == "error") {
            logLevel = LogLevel::Error;

        } else if (logLevelStr == "fatal") {
            logLevel = LogLevel::Fatal;
        }
    }

    logger->setGlobalLevel(logLevel);

    // 控制台输出（彩色）
    // 默认使用异步模式（更好的性能），--sync-logging 参数可启用同步模式
    auto consoleAppender = QSharedPointer<ConsoleAppender>::create(true, !syncLogging);

    consoleAppender->setFormatter(QSharedPointer<PatternFormatter>::create(PatternFormatter::simplePattern()));

    logger->addAppender(consoleAppender);

    // 文件输出（仅在调试模式或明确指定日志文件路径时启用）

    if (debugMode || !logFilePath.isEmpty()) {
        QString logPath;

        if (!logFilePath.isEmpty()) {
            logPath = logFilePath;

        } else {
            // 在调试模式下，将日志文件保存到 debug 文件夹
            QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/debug";
            QDir().mkpath(logDir);

            // 使用时间戳创建日志文件名
            QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
            logPath = logDir + QString("/easykiconverter_debug_%1.log").arg(timestamp);
        }

        auto fileAppender =

            QSharedPointer<FileAppender>::create(logPath, 10 * 1024 * 1024, 5, true);  // 10MB, 5 files, async

        fileAppender->setFormatter(QSharedPointer<PatternFormatter>::create(PatternFormatter::simplePattern()));

        logger->addAppender(fileAppender);

        LOG_INFO(LogModule::Core,
                 "日志系统已初始化 - 级别: {}, 日志文件: {}",
                 logLevelStr.isEmpty() ? "default" : logLevelStr,
                 logPath);

    } else {
        LOG_INFO(LogModule::Core,
                 "日志系统已初始化 - 级别: {} (仅控制台输出)",
                 logLevelStr.isEmpty() ? "default" : logLevelStr);
    }

    // 安装 Qt 日志适配器（将 qDebug/qWarning/qCritical 重定向到新系统）
    QtLogAdapter::install();
}

/** @brief 规范化 AppImage 挂载目录或开发构建目录的路径层级。 */
QString normalizeMountedAppDir(QString appDir) {
    if (appDir.endsWith("/usr/bin")) {
        return appDir.chopped(8);  // 去掉 /usr/bin
    }
    if (appDir.endsWith("/bin")) {
        return appDir.chopped(4);  // 去掉 /bin
    }
    return appDir;
}

/** @brief 根据 AppImage 环境变量或应用路径解析运行时目录。 */
QString resolveRuntimeAppDir(const QString& appImagePath) {
    if (!appImagePath.isEmpty()) {
        return QFileInfo(appImagePath).absolutePath();
    }
    return QCoreApplication::applicationDirPath();
}

/** @brief 从运行目录反推出项目根目录，供开发环境查找资源。 */
QString resolveProjectRootFromAppDir(QString appDir) {
    appDir = normalizeMountedAppDir(std::move(appDir));
    if (appDir.endsWith("/build/bin")) {
        return appDir.chopped(10);  // 去掉 /build/bin
    }
    if (appDir.endsWith("/build")) {
        return appDir.chopped(6);  // 去掉 /build
    }
    if (appDir.endsWith("/bin")) {
        return appDir.chopped(4);  // 去掉 /bin
    }
    return appDir;
}

/** @brief 按当前主题和运行环境生成任务栏图标候选路径。 */
QStringList themedTaskbarIconPaths(bool darkMode) {
    const QString iconThemeDir = darkMode ? QStringLiteral("hicolor-dark") : QStringLiteral("hicolor");
    const QString fallbackIconThemeDir = QStringLiteral("hicolor");
    const QString themedAppIcon = darkMode ? QStringLiteral("app_icon_dark.png") : QStringLiteral("app_icon.png");
    const QString appImagePath = qgetenv("APPIMAGE");
    const QString runtimeAppDir = resolveRuntimeAppDir(appImagePath);
    const QString mountedAppDir = normalizeMountedAppDir(QCoreApplication::applicationDirPath());
    const QString projectRoot = resolveProjectRootFromAppDir(mountedAppDir);
    const bool isAppImage = !appImagePath.isEmpty();
    const QStringList sizes = {"512x512", "256x256", "128x128", "64x64", "48x48", "32x32", "24x24", "16x16"};

    QStringList qrcIconPaths = {
        QStringLiteral(":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/") + themedAppIcon,
        QStringLiteral(":/resources/icons/") + themedAppIcon,
    };
    QStringList appImageIconPaths;
    QStringList devIconPaths;
    QStringList systemIconPaths = {
        QStringLiteral("io.github.tangsangsimida.easykiconverter"),
    };

    for (const QString& size : sizes) {
        const QString iconName = QStringLiteral("/apps/io.github.tangsangsimida.easykiconverter.png");
        const QString themedIconPath = iconThemeDir + "/" + size + iconName;
        qrcIconPaths << QStringLiteral(":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/") + themedIconPath
                     << QStringLiteral(":/resources/icons/") + themedIconPath;
        appImageIconPaths << mountedAppDir + "/usr/share/icons/" + themedIconPath
                          << runtimeAppDir + "/usr/share/icons/" + themedIconPath
                          << mountedAppDir + "/resources/icons/" + themedIconPath;
        devIconPaths << projectRoot + "/resources/icons/" + themedIconPath;
        systemIconPaths << "/usr/share/icons/" + themedIconPath;

        if (darkMode) {
            const QString fallbackIconPath = fallbackIconThemeDir + "/" + size + iconName;
            qrcIconPaths << QStringLiteral(":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/") + fallbackIconPath
                         << QStringLiteral(":/resources/icons/") + fallbackIconPath;
            appImageIconPaths << mountedAppDir + "/usr/share/icons/" + fallbackIconPath
                              << runtimeAppDir + "/usr/share/icons/" + fallbackIconPath
                              << mountedAppDir + "/resources/icons/" + fallbackIconPath;
            devIconPaths << projectRoot + "/resources/icons/" + fallbackIconPath;
            systemIconPaths << "/usr/share/icons/" + fallbackIconPath;
        }
    }

    QStringList genericPaths = {
        mountedAppDir + "/" + themedAppIcon,
        runtimeAppDir + "/" + themedAppIcon,
        projectRoot + "/resources/icons/" + themedAppIcon,
        mountedAppDir + "/io.github.tangsangsimida.easykiconverter.png",
        runtimeAppDir + "/io.github.tangsangsimida.easykiconverter.png",
        projectRoot + "/io.github.tangsangsimida.easykiconverter.png",
        QStringLiteral(":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/app_icon.png"),
        QStringLiteral(":/resources/icons/app_icon.png"),
        projectRoot + "/resources/icons/app_icon.png",
    };

    QStringList iconPaths = isAppImage
                                ? appImageIconPaths + qrcIconPaths + devIconPaths + genericPaths + systemIconPaths
                                : qrcIconPaths + devIconPaths + appImageIconPaths + genericPaths + systemIconPaths;
    iconPaths.removeDuplicates();
    return iconPaths;
}

/** @brief 从候选路径中加载图标并应用到应用程序和窗口。 */
bool applyTaskbarIcon(bool darkMode, QWindow* window = nullptr, const char* context = "应用程序") {
    const QStringList iconPaths = themedTaskbarIconPaths(darkMode);

    for (const QString& iconPath : iconPaths) {
        const bool canTryIconTheme = !iconPath.contains('/');
        if (!canTryIconTheme && !QFile::exists(iconPath)) {
            continue;
        }

        const QIcon icon = canTryIconTheme ? QIcon::fromTheme(iconPath) : QIcon(iconPath);
        if (icon.isNull()) {
            continue;
        }

        QGuiApplication::setWindowIcon(icon);
        if (auto* application = qobject_cast<QApplication*>(QCoreApplication::instance())) {
            application->setWindowIcon(icon);
        }
        if (window) {
            window->setIcon(icon);
        }

        qDebug() << context << "任务栏图标已设置:" << iconPath << "darkMode=" << darkMode
                 << "availableSizes=" << icon.availableSizes();
        return true;
    }

    qWarning() << context << "任务栏图标设置失败，darkMode=" << darkMode << "尝试路径数:" << iconPaths.size();
    return false;
}

}  // anonymous namespace

/** @brief 初始化应用程序、命令行模式和图形界面模式。 */
int main(int argc, char* argv[]) {
    QSurfaceFormat surfaceFormat = QSurfaceFormat::defaultFormat();
    surfaceFormat.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);
    QQuickWindow::setDefaultAlphaBuffer(true);

#ifdef _WIN32
    // Windows 透明无边框窗口在 Vulkan 后端下可能出现圆角透明区域被合成为黑色。
    // Direct3D 11 是 Qt Quick 在 Windows 上更稳定的默认路径。
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    qInfo() << "已将 Windows 渲染后端设置为 Direct3D 11，并启用 alpha buffer";
#else
    // 非 Windows 平台保留 Vulkan 优先策略。
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    qInfo() << "已将渲染后端优先设置为 Vulkan";
#endif

    // 设置 stdout 为无缓冲模式，确保日志颜色正常显示
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    // 在 QApplication 构造函数之前检查命令行参数
    bool showHelp = false;
    bool showVersion = false;
    bool cliModeRequested = false;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        // 支持 Unix 风格（-h, --help）和 Windows 风格（/h, /help, /?, ?）的参数
        if (arg == "--help" || arg == "-h" || arg == "/h" || arg == "/help" || arg == "/?" || arg == "?") {
            showHelp = true;
        } else if (arg == "--version" || arg == "-v" || arg == "/v" || arg == "/version") {
            showVersion = true;
        }
    }

    // 创建命令行参数解析器（在 QApplication 之前，以支持纯 CLI 模式）
    EasyKiConverter::CommandLineParser cmdParser(argc, argv);

    // 早期解析：检查是否为纯 CLI 模式（不需要 GUI）
    bool parsed = cmdParser.parse();
    bool isCompletionMode = parsed && cmdParser.isCompletionRequested();
    bool isCompleteMode = parsed && cmdParser.isCompleteRequested();
    bool isCliMode = parsed && cmdParser.isCliMode();
    bool isConvertCommand = parsed && cmdParser.hasConvertCommand();

    // 纯 CLI 模式：--completion, --complete, --help, --version, convert 子命令
    // 这些模式不需要 QApplication，只需要 QCoreApplication
    cliModeRequested = isCompletionMode || isCompleteMode || showHelp || showVersion || isCliMode || isConvertCommand;

    if (cliModeRequested) {
        // 纯 CLI 模式：使用 QCoreApplication
        QCoreApplication app(argc, argv);
        app.setApplicationName("EasyKiConverter");
        app.setApplicationVersion(EasyKiConverter::APP_VERSION);
        app.setOrganizationName("EasyKiConverter");
        app.setOrganizationDomain("easykiconverter.com");

        if (showHelp) {
            QString helpText = cmdParser.helpText();
            QTextStream consoleOut(stdout);
            consoleOut << helpText;
            return 0;
        }

        if (showVersion) {
            QString versionText = "EasyKiConverter " + app.applicationVersion() + "\n";
            QTextStream consoleOut(stdout);
            consoleOut << versionText;
            return 0;
        }

        if (!parsed) {
            QTextStream err(stderr);
            err << QCoreApplication::translate("main", "错误: 无效的命令行参数") << "\n";
            err << cmdParser.helpText();
            return 1;
        }

        // 尽早初始化语言管理器，使后续验证错误消息能正确翻译
        {
            auto* langManager = EasyKiConverter::LanguageManager::instance();
            QString cliLang = cmdParser.language();
            if (!cliLang.isEmpty() && (cliLang == "zh_CN" || cliLang == "en")) {
                langManager->setLanguage(cliLang, /*force=*/true);
            }
        }

        if (!cmdParser.validate()) {
            QTextStream err(stderr);
            err << QCoreApplication::translate("main", "错误: 参数值无效") << "\n";
            err << cmdParser.validationError() << "\n\n";
            err << cmdParser.helpText();
            return 1;
        }

        const QString configFilePath = resolveConfigFilePath(cmdParser);
        EasyKiConverter::TempFileManager::recoverIncompleteTransactions();
        EasyKiConverter::ConfigService::instance()->loadConfig(configFilePath);
        applyCacheConfiguration(cmdParser);

        // 处理补全请求
        if (isCompletionMode) {
            QString shell = cmdParser.completionShell();
            auto shellType = EasyKiConverter::CompletionGenerator::parseShell(shell);
            QTextStream out(stdout);
            out << EasyKiConverter::CompletionGenerator::generate(shellType);
            return 0;
        }

        // 处理动态补全请求
        if (isCompleteMode) {
            QString completeType = cmdParser.completeType();
            QTextStream out(stdout);

            if (completeType == "lcsc-id") {
                // 输出缓存中的 LCSC ID 列表
                auto* cache = EasyKiConverter::ComponentCacheService::instance();
                if (cache) {
                    QStringList ids = cache->getCachedComponentIds();
                    for (const QString& id : ids) {
                        out << id << "\n";
                    }
                }
            }

            return 0;
        }

        // CLI 转换模式
        if (isCliMode) {
#ifdef _WIN32
            // 尝试附加到父进程的控制台
            if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
                AllocConsole();
                SetConsoleTitleA("EasyKiConverter - CLI");
            }
            (void)reopenConsoleStream(stdout, "CONOUT$", "w", "stdout");
            (void)reopenConsoleStream(stderr, "CONOUT$", "w", "stderr");
#endif
            // 初始化日志系统（CLI 模式也需要日志）
            bool debugMode = isDebugMode(cmdParser);
            setupLogging(debugMode, cmdParser.logLevel(), cmdParser.logFile(), cmdParser.isSyncLogging());

            // 创建并执行 CLI 转换器
            EasyKiConverter::CliConverter cliConverter(cmdParser);
            bool success = cliConverter.execute();

            if (!success) {
                QTextStream err(stderr);
                err << QCoreApplication::translate("main", "错误: ") << cliConverter.errorMessage() << "\n";
            }

            // 清理日志
            EasyKiConverter::QtLogAdapter::uninstall();
            auto* logger = EasyKiConverter::Logger::instance();
            if (logger) {
                logger->flush();
                logger->close();
            }

            return success ? 0 : 1;
        }

        // convert 命令存在但无效子命令（如 easykiconverter convert abc）
        if (isConvertCommand && !isCliMode) {
            QTextStream err(stderr);
            err << QCoreApplication::translate("main", "错误: 无效的 convert 子命令") << "\n";
            err << QCoreApplication::translate("main", "有效的子命令: bom, component, batch") << "\n\n";
            err << cmdParser.cliHelpText();
            return 1;
        }

        // 到达此处的唯一可能是 isConvertCommand 但 isCliMode=false（无效子命令）
        // 该路径已在上面返回 1，此处不会执行到此
        // 防御性断言：确保所有 CLI 模式请求都在上面正确处理
        Q_ASSERT_X(false, "main", "Unhandled CLI mode path");
        return 1;
    }

    // GUI 模式：使用完整的 QApplication
    // 设置 QML 样式为 Basic，以消除原生样式自定义警告
    QQuickStyle::setStyle("Basic");
    // High DPI 适配：使用 PassThrough 策略避免分数缩放下的模糊和舍入误差
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    // 自动检测并记录实际生效的渲染后端
    QTimer::singleShot(0, []() {
        QQuickWindow tempWindow;
        const char* apiName = "Unknown";
        /** @brief 将 Qt 图形 API 枚举转换为日志中的可读名称。 */
        switch (tempWindow.graphicsApi()) {
            case QSGRendererInterface::Unknown:
                apiName = "Unknown";
                break;
            case QSGRendererInterface::Software:
                apiName = "Software";
                break;
            case QSGRendererInterface::OpenVG:
                apiName = "OpenVG";
                break;
            case QSGRendererInterface::OpenGL:
                apiName = "OpenGL";
                break;
            case QSGRendererInterface::Direct3D11:
                apiName = "Direct3D 11";
                break;
            case QSGRendererInterface::Vulkan:
                apiName = "Vulkan";
                break;
            case QSGRendererInterface::Metal:
                apiName = "Metal";
                break;
            case QSGRendererInterface::Null:
                apiName = "Null (headless)";
                break;
            case QSGRendererInterface::Direct3D12:
                apiName = "Direct3D 12 (DX12)";
                break;
            default:
                apiName = "Other/Unknown";
                break;
        }
        qInfo() << "当前实际运行的渲染 API:" << apiName;
    });

    // 设置应用程序信息
    app.setApplicationName("EasyKiConverter");
    app.setApplicationVersion(EasyKiConverter::APP_VERSION);
    app.setOrganizationName("EasyKiConverter");
    app.setOrganizationDomain("easykiconverter.com");

    // 注意：所有 CLI 模式处理（--help, --version, --completion, --complete, convert 子命令）
    // 已在 QCoreApplication 阶段处理完毕。如果到达此处，说明是纯 GUI 启动。
    // 命令行参数已在 CLI 路径解析并验证，无需重复解析。

    // 检查调试模式（命令行参数优先，环境变量向后兼容）
    bool debugMode = isDebugMode(cmdParser);

    // 初始化日志系统（在 QApplication 创建后立即初始化）
    setupLogging(debugMode, cmdParser.logLevel(), cmdParser.logFile(), cmdParser.isSyncLogging());
    EasyKiConverter::TempFileManager::recoverIncompleteTransactions();

    // 尝试设置应用程序图标（使用 QGuiApplication::setWindowIcon 确保可靠性）
    QString appImagePath = qgetenv("APPIMAGE");
    QString appDir = resolveRuntimeAppDir(appImagePath);

    qDebug() << "应用目录:" << appDir;

    // 处理命令行参数 - 配置文件路径
    QString configFilePath = resolveConfigFilePath(cmdParser);
    if (!cmdParser.configFile().isEmpty()) {
        qDebug() << "使用命令行指定的配置文件:" << configFilePath;
    } else if (cmdParser.isPortableMode()) {
        qDebug() << "便携模式：配置文件保存在程序目录:" << configFilePath;
    }

    // 初始化配置服务（必须在加载图标之前，以便获取主题设置）
    EasyKiConverter::ConfigService::instance()->loadConfig(configFilePath);
    applyCacheConfiguration(cmdParser);

    // 处理命令行参数 - 调试模式设置（优先于环境变量）
    // 调试模式只通过命令行参数和环境变量控制，不会保存到配置文件
    if (cmdParser.isDebugMode()) {
        auto* configService = EasyKiConverter::ConfigService::instance();
        // 设置内存中的调试模式（不保存到配置文件）
        configService->setDebugMode(true, false);
        qDebug() << "通过命令行参数启用调试模式（仅当前会话有效）";
    }

    // 处理命令行参数 - 语言设置
    if (!cmdParser.language().isEmpty()) {
        auto* langManager = EasyKiConverter::LanguageManager::instance();
        QString lang = cmdParser.language();
        if (lang == "zh_CN" || lang == "en") {
            langManager->setLanguage(lang);
            qDebug() << "通过命令行设置语言为:" << lang;
        } else {
            qWarning() << "无效的语言设置:" << lang << "，支持的选项: zh_CN, en";
        }
    } else {
        // 初始化语言管理器（使用默认语言）
        EasyKiConverter::LanguageManager::instance();
    }

    // 处理命令行参数 - 主题设置（仅在用户显式指定时应用）
    if (cmdParser.isThemeSet()) {
        QString theme = cmdParser.theme();
        auto* configService = EasyKiConverter::ConfigService::instance();
        if (theme == "dark") {
            configService->setDarkMode(true);
            qDebug() << "通过命令行设置主题为: dark";
        } else if (theme == "light") {
            configService->setDarkMode(false);
            qDebug() << "通过命令行设置主题为: light";
        } else {
            qWarning() << "无效的主题设置:" << theme << "，支持的选项: dark, light";
        }
    }

    // 加载应用图标（根据当前主题选择浅色/深色任务栏图标）。
    applyTaskbarIcon(EasyKiConverter::ConfigService::instance()->getDarkMode(), nullptr, "启动");

    // 创建 Service 实例（使用流水线架构，不设置 parent，手动管理生命周期）
    // 预先初始化缓存服务，避免首次添加组件时阻塞UI
    (void)EasyKiConverter::ComponentCacheService::instance();
    // 预先初始化统一网络客户端，避免首次请求时的冷启动抖动
    (void)EasyKiConverter::NetworkClient::instance();
    EasyKiConverter::ComponentService* componentService = new EasyKiConverter::ComponentService();
    EasyKiConverter::ParallelExportService* exportService = new EasyKiConverter::ParallelExportService();
    exportService->setComponentService(componentService);

    // 注册自定义类型以支持跨线程信号槽的 QueuedConnection
    qRegisterMetaType<EasyKiConverter::ExportItemStatus>();
    qRegisterMetaType<EasyKiConverter::ExportTypeProgress>();
    qRegisterMetaType<EasyKiConverter::PreloadProgress>();
    qRegisterMetaType<EasyKiConverter::ExportOverallProgress>();

    // 创建 ViewModel 实例（不设置 parent，手动管理生命周期）
    EasyKiConverter::ComponentListViewModel* componentListViewModel =
        new EasyKiConverter::ComponentListViewModel(componentService);
    EasyKiConverter::ExportSettingsViewModel* exportSettingsViewModel =
        new EasyKiConverter::ExportSettingsViewModel(exportService);
    EasyKiConverter::ExportProgressViewModel* exportProgressViewModel =
        new EasyKiConverter::ExportProgressViewModel(exportService, componentService, componentListViewModel);
    EasyKiConverter::ThemeSettingsViewModel* themeSettingsViewModel = new EasyKiConverter::ThemeSettingsViewModel();
    EasyKiConverter::UpdateCheckerService* updateCheckerService = new EasyKiConverter::UpdateCheckerService();
    EasyKiConverter::ExportTargetModel* exportTargetModel = new EasyKiConverter::ExportTargetModel();
    exportTargetModel->loadPlugins(":/qt/qml/EasyKiconverter_Cpp_Version/src/resources/export_plugins.json");
    exportSettingsViewModel->setTargetModel(exportTargetModel);

    // 创建 QML 引擎
    auto* engine = new QQmlApplicationEngine();

    // 注册 LanguageManager 到 QML
    qmlRegisterSingletonType<QObject>(
        "EasyKiconverter_Cpp_Version", 1, 0, "LanguageManager", [](QQmlEngine*, QJSEngine*) -> QObject* {
            return EasyKiConverter::LanguageManager::instance();
        });

    // 连接语言管理器的刷新信号到引擎的重新翻译
    QObject::connect(
        EasyKiConverter::LanguageManager::instance(),
        &EasyKiConverter::LanguageManager::refreshRequired,
        engine,
        [engine]() { engine->retranslate(); },
        Qt::QueuedConnection);

    // 将 ViewModel 和 Service 注册到 QML 上下文
    engine->rootContext()->setContextProperty("componentListViewModel", componentListViewModel);
    engine->rootContext()->setContextProperty("exportSettingsViewModel", exportSettingsViewModel);
    engine->rootContext()->setContextProperty("exportProgressViewModel", exportProgressViewModel);
    engine->rootContext()->setContextProperty("themeSettingsViewModel", themeSettingsViewModel);
    engine->rootContext()->setContextProperty("updateCheckerService", updateCheckerService);
    engine->rootContext()->setContextProperty("exportTargetModel", exportTargetModel);
    engine->rootContext()->setContextProperty("configService", EasyKiConverter::ConfigService::instance());
    engine->rootContext()->setContextProperty("componentCacheService",
                                              EasyKiConverter::ComponentCacheService::instance());
    engine->rootContext()->setContextProperty("fileUtils", new EasyKiConverter::FileUtils(engine));

    // 连接对象创建失败信号
    QObject::connect(
        engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [](const QUrl& url) {
            qCritical() << "创建QML对象失败：" << url;
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    // 加载 QML 文件
    const QUrl url("qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/Main.qml");
    engine->load(url);

    if (engine->rootObjects().isEmpty()) {
        qCritical() << "严重错误：QML引擎根对象加载后为空！";
        delete engine;
        return -1;
    }

    // 获取根窗口对象并设置窗口位置和图标
    auto* rootObject = engine->rootObjects().first();
    if (auto* window = qobject_cast<QQuickWindow*>(rootObject)) {
        auto refreshTaskbarIcon = [window]() {
            const bool isDarkModeForIcon = EasyKiConverter::ConfigService::instance()->getDarkMode();
            applyTaskbarIcon(isDarkModeForIcon, window, "窗口");
        };

        refreshTaskbarIcon();

        QObject::connect(themeSettingsViewModel,
                         &EasyKiConverter::ThemeSettingsViewModel::darkModeChanged,
                         window,
                         refreshTaskbarIcon,
                         Qt::QueuedConnection);

        QPointer<QQuickWindow> guardedWindow(window);
        QTimer::singleShot(500, [guardedWindow]() {
            if (!guardedWindow)
                return;
            if (!guardedWindow->isVisible()) {
                qDebug() << "Startup fallback: window still hidden after 500ms,"
                         << "visibility=" << guardedWindow->visibility() << "position=" << guardedWindow->position()
                         << "size=" << guardedWindow->size() << "screen="
                         << (guardedWindow->screen() ? guardedWindow->screen()->name() : QStringLiteral("null"));
                guardedWindow->show();
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
                guardedWindow->raise();
                guardedWindow->requestActivate();
#endif
                QCoreApplication::processEvents();
            }

            // 窗口显示后再次设置任务栏图标
            const bool isDarkModeForIcon = EasyKiConverter::ConfigService::instance()->getDarkMode();
            applyTaskbarIcon(isDarkModeForIcon, guardedWindow.data(), "窗口显示后");
        });
    }

    // 点击关闭按钮时直接退出应用程序
    app.setQuitOnLastWindowClosed(true);

    // 启动检查遵循自动检查开关和 24 小时默认间隔，设置页的手动检查会强制刷新。
    QTimer::singleShot(
        2000, updateCheckerService, [updateCheckerService]() { updateCheckerService->checkForUpdates(false); });

    // 连接应用程序的 aboutToQuit 信号，确保退出前清理所有资源
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        qDebug() << "Application is about to quit, performing cleanup...";

        // 1. 取消导出服务（如果正在导出）
        if (exportService && exportService->isRunning()) {
            qDebug() << "Cancelling export service...";
            exportService->cancelExport();
        }

        // 2. 取消所有挂起的组件请求
        if (componentService) {
            qDebug() << "Cancelling all pending component requests...";
            componentService->cancelAllPendingRequests();
        }

        qDebug() << "Cleanup completed, application can now exit.";
    });

    // 运行事件循环
    int exitCode = app.exec();

    // === 重要：显式按顺序销毁对象，防止退出时崩溃 ===

    // 1. 清除 QML 引擎的上下文属性，防止 QML 组件访问已销毁的对象
    qDebug() << "Clearing QML context properties...";
    engine->rootContext()->setContextProperty("componentListViewModel", nullptr);
    engine->rootContext()->setContextProperty("exportSettingsViewModel", nullptr);
    engine->rootContext()->setContextProperty("exportProgressViewModel", nullptr);
    engine->rootContext()->setContextProperty("themeSettingsViewModel", nullptr);
    engine->rootContext()->setContextProperty("updateCheckerService", nullptr);
    engine->rootContext()->setContextProperty("exportTargetModel", nullptr);
    engine->rootContext()->setContextProperty("componentCacheService", nullptr);

    // 2. 先销毁 QML 引擎，阻止事件继续投递到 QML/QObject 图树
    qDebug() << "Destroying QML engine...";
    delete engine;
    engine = nullptr;

    // 3. 在销毁 C++ 对象前清空挂起事件
    qDebug() << "Draining pending events before object teardown...";
    QCoreApplication::processEvents();

    // 4. 销毁 ViewModel
    qDebug() << "Destroying ViewModels...";
    delete updateCheckerService;
    updateCheckerService = nullptr;
    delete themeSettingsViewModel;
    themeSettingsViewModel = nullptr;
    delete exportProgressViewModel;
    exportProgressViewModel = nullptr;
    delete exportTargetModel;
    exportTargetModel = nullptr;
    delete exportSettingsViewModel;
    exportSettingsViewModel = nullptr;
    delete componentListViewModel;
    componentListViewModel = nullptr;

    // 5. 销毁服务（析构函数会自动调用 cancelExport 和 cleanup）
    qDebug() << "Destroying services...";
    delete exportService;
    exportService = nullptr;
    delete componentService;
    componentService = nullptr;

    qDebug() << "Destroying network client singleton...";
    EasyKiConverter::NetworkClient::destroyInstance();

    // 6. 最后清理日志
    qDebug() << "Cleaning up logging...";
    EasyKiConverter::QtLogAdapter::uninstall();
    auto* logger = EasyKiConverter::Logger::instance();
    if (logger) {
        logger->flush();
        logger->close();
    }

    qDebug() << "Cleanup completed, exiting...";
    return exitCode;
}
