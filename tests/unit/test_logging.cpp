#include "utils/logging/Log.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>
#include <QTextStream>

using namespace EasyKiConverter;

class TestLogging : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // LogLevel 测试
    void testLogLevelToString();
    void testLogLevelFromString();

    // LogModule 测试
    void testLogModuleBitwise();
    void testLogModuleToString();
    void testLogModuleFromString();

    // LogRecord 测试
    void testLogRecordTimestamp();
    void testLogRecordShortFileName();

    // PatternFormatter 测试
    void testPatternFormatterBasic();
    void testPatternFormatterPlaceholders();
    void testPatternFormatterClone();

    // ConsoleAppender 测试
    void testConsoleAppenderCreation();
    void testConsoleAppenderColors();

    // FileAppender 测试
    void testFileAppenderCreation();
    void testFileAppenderWrite();
    void testFileAppenderRollOver();

    // Logger 测试
    void testLoggerSingleton();
    void testLoggerGlobalLevel();
    void testLoggerModuleLevel();
    void testLoggerShouldLog();
    void testLoggerLogRecord();
    void testLoggerAppenderManagement();

    // 日志宏测试
    void testLogMacros();

    // QtLogAdapter 测试
    void testQtLogAdapter();

private:
    QString m_tempLogPath;
};

void TestLogging::initTestCase() {
    m_tempLogPath = QDir::tempPath() + "/easykiconverter_test.log";

    // 清理之前的测试文件
    QFile::remove(m_tempLogPath);
}

void TestLogging::cleanupTestCase() {
    // 清理测试文件
    QFile::remove(m_tempLogPath);

    // 清理 Logger 状态
    Logger::instance()->clearAppenders();
    Logger::instance()->clearAllModuleLevels();
}

// ========== LogLevel 测试 ==========

void TestLogging::testLogLevelToString() {
    QCOMPARE(logLevelToString(LogLevel::Trace), QString("TRACE"));
    QCOMPARE(logLevelToString(LogLevel::Debug), QString("DEBUG"));
    QCOMPARE(logLevelToString(LogLevel::Info), QString("INFO"));
    QCOMPARE(logLevelToString(LogLevel::Warn), QString("WARN"));
    QCOMPARE(logLevelToString(LogLevel::Error), QString("ERROR"));
    QCOMPARE(logLevelToString(LogLevel::Fatal), QString("FATAL"));
    QCOMPARE(logLevelToString(LogLevel::Off), QString("OFF"));
}

void TestLogging::testLogLevelFromString() {
    QCOMPARE(logLevelFromString("TRACE"), LogLevel::Trace);
    QCOMPARE(logLevelFromString("debug"), LogLevel::Debug);
    QCOMPARE(logLevelFromString("INFO"), LogLevel::Info);
    QCOMPARE(logLevelFromString("Warning"), LogLevel::Warn);
    QCOMPARE(logLevelFromString("ERROR"), LogLevel::Error);
    QCOMPARE(logLevelFromString("FATAL"), LogLevel::Fatal);
    QCOMPARE(logLevelFromString("OFF"), LogLevel::Off);
    QCOMPARE(logLevelFromString("unknown"), LogLevel::Info);  // 默认
}

// ========== LogModule 测试 ==========

void TestLogging::testLogModuleBitwise() {
    // 测试位运算
    LogModule combined = LogModule::Network | LogModule::Export;
    QVERIFY(hasModule(combined, LogModule::Network));
    QVERIFY(hasModule(combined, LogModule::Export));
    QVERIFY(!hasModule(combined, LogModule::UI));

    // 测试 &= 运算
    LogModule result = combined & LogModule::Network;
    QVERIFY(hasModule(result, LogModule::Network));
    QVERIFY(!hasModule(result, LogModule::Export));
}

void TestLogging::testLogModuleToString() {
    QCOMPARE(logModuleToString(LogModule::Core), QString("Core"));
    QCOMPARE(logModuleToString(LogModule::Network), QString("Network"));
    QCOMPARE(logModuleToString(LogModule::Export), QString("Export"));
    QCOMPARE(logModuleToString(LogModule::None), QString("None"));
    QCOMPARE(logModuleToString(LogModule::All), QString("All"));
}

void TestLogging::testLogModuleFromString() {
    QCOMPARE(logModuleFromString("CORE"), LogModule::Core);
    QCOMPARE(logModuleFromString("network"), LogModule::Network);
    QCOMPARE(logModuleFromString("Export"), LogModule::Export);
    QCOMPARE(logModuleFromString("unknown"), LogModule::Core);  // 默认
}

// ========== LogRecord 测试 ==========

void TestLogging::testLogRecordTimestamp() {
    LogRecord record;
    record.timestamp = QDateTime::currentMSecsSinceEpoch();

    QString formatted = record.formattedTimestamp();
    QVERIFY(formatted.contains("20"));  // 年份
    QVERIFY(formatted.contains(":"));  // 时间分隔符
}

void TestLogging::testLogRecordShortFileName() {
    LogRecord record;

    // Unix 路径
    record.fileName = "/home/user/project/src/file.cpp";
    QCOMPARE(record.shortFileName(), QString("file.cpp"));

    // Windows 路径
    record.fileName = "C:\\Users\\test\\src\\file.cpp";
    QCOMPARE(record.shortFileName(), QString("file.cpp"));

    // 无路径
    record.fileName = "file.cpp";
    QCOMPARE(record.shortFileName(), QString("file.cpp"));
}

// ========== PatternFormatter 测试 ==========

void TestLogging::testPatternFormatterBasic() {
    PatternFormatter formatter;

    LogRecord record;
    record.level = LogLevel::Info;
    record.module = LogModule::Network;
    record.message = "Test message";
    record.timestamp = QDateTime::currentMSecsSinceEpoch();
    record.threadId = reinterpret_cast<Qt::HANDLE>(0x1234);
    record.fileName = "test.cpp";
    record.line = 42;

    QString result = formatter.format(record);
    QVERIFY(result.contains("INFO"));
    QVERIFY(result.contains("Network"));
    QVERIFY(result.contains("Test message"));
    QVERIFY(result.contains("test.cpp"));
    QVERIFY(result.contains("42"));
}

void TestLogging::testPatternFormatterPlaceholders() {
    PatternFormatter formatter("%{level} - %{module} - %{message}");

    LogRecord record;
    record.level = LogLevel::Debug;
    record.module = LogModule::Export;
    record.message = "Hello World";
    record.timestamp = QDateTime::currentMSecsSinceEpoch();

    QString result = formatter.format(record);
    QCOMPARE(result, QString("DEBUG - Export - Hello World"));
}

void TestLogging::testPatternFormatterClone() {
    PatternFormatter original("%{level} %{message}");
    auto cloned = original.clone();

    QVERIFY(cloned != nullptr);

    // 验证克隆的格式化器能正常工作
    LogRecord record;
    record.level = LogLevel::Debug;
    record.message = "Test";
    record.timestamp = QDateTime::currentMSecsSinceEpoch();

    QString originalResult = original.format(record);
    QString clonedResult = cloned->format(record);
    QCOMPARE(originalResult, clonedResult);
}

// ========== ConsoleAppender 测试 ==========

void TestLogging::testConsoleAppenderCreation() {
    const QByteArray termBefore = qgetenv("TERM");
    ConsoleAppender appender(true);
    // useColors() 可能返回 false，取决于控制台支持
    // 重要的是构造函数不会崩溃
    QCOMPARE(qgetenv("TERM"), termBefore);

    ConsoleAppender appenderNoColor(false);
    QVERIFY(!appenderNoColor.useColors());
}

void TestLogging::testConsoleAppenderColors() {
    ConsoleAppender appender(true);

    // 设置自定义颜色
    appender.setLevelColor(LogLevel::Error, "\033[31m");
    appender.setLevelBackground(LogLevel::Fatal, "\033[41m");

    // 验证不会崩溃
    LogRecord record;
    record.level = LogLevel::Info;
    record.message = "Test";
    appender.append(record, "Test message");

    appender.flush();
}

// ========== FileAppender 测试 ==========

void TestLogging::testFileAppenderCreation() {
    FileAppender appender(m_tempLogPath, 1024, 3, false);
    QCOMPARE(appender.filePath(), m_tempLogPath);
    QCOMPARE(appender.maxSize(), 1024LL);
    QCOMPARE(appender.maxFiles(), 3);
    QVERIFY(!appender.isAsync());
}

void TestLogging::testFileAppenderWrite() {
    QString testPath = QDir::tempPath() + "/test_write.log";
    QFile::remove(testPath);

    {
        FileAppender appender(testPath, 1024 * 1024, 1, false);

        LogRecord record;
        record.level = LogLevel::Info;
        record.message = "Test write";

        appender.append(record, "Test write message");
        appender.flush();
    }

    // 验证文件内容
    QFile file(testPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains("Test write message"));
    file.close();

    QFile::remove(testPath);
}

void TestLogging::testFileAppenderRollOver() {
    QString testPath = QDir::tempPath() + "/test_rollover.log";
    QFile::remove(testPath);

    // 创建一个小文件限制的 appender
    FileAppender appender(testPath, 100, 2, false);  // 100 bytes max

    // 写入足够触发滚动的内容
    for (int i = 0; i < 20; ++i) {
        LogRecord record;
        record.level = LogLevel::Info;
        appender.append(record, QString("Test message number %1 with enough content").arg(i));
    }

    appender.close();
    QFile::remove(testPath);
}

// ========== Logger 测试 ==========

void TestLogging::testLoggerSingleton() {
    Logger* instance1 = Logger::instance();
    Logger* instance2 = Logger::instance();
    QCOMPARE(instance1, instance2);
}

void TestLogging::testLoggerGlobalLevel() {
    Logger::instance()->setGlobalLevel(LogLevel::Debug);
    QCOMPARE(Logger::instance()->globalLevel(), LogLevel::Debug);

    Logger::instance()->setGlobalLevel(LogLevel::Info);
    QCOMPARE(Logger::instance()->globalLevel(), LogLevel::Info);
}

void TestLogging::testLoggerModuleLevel() {
    Logger::instance()->setModuleLevel(LogModule::Network, LogLevel::Warn);
    QCOMPARE(Logger::instance()->moduleLevel(LogModule::Network), LogLevel::Warn);

    Logger::instance()->clearModuleLevel(LogModule::Network);
    // 清除后应该返回全局级别
    QCOMPARE(Logger::instance()->moduleLevel(LogModule::Network), Logger::instance()->globalLevel());
}

void TestLogging::testLoggerShouldLog() {
    Logger::instance()->setGlobalLevel(LogLevel::Info);

    // Info 级别应该记录
    QVERIFY(Logger::instance()->shouldLog(LogLevel::Info, LogModule::Core));
    QVERIFY(Logger::instance()->shouldLog(LogLevel::Warn, LogModule::Core));
    QVERIFY(Logger::instance()->shouldLog(LogLevel::Error, LogModule::Core));

    // Debug 级别不应该记录
    QVERIFY(!Logger::instance()->shouldLog(LogLevel::Debug, LogModule::Core));
    QVERIFY(!Logger::instance()->shouldLog(LogLevel::Trace, LogModule::Core));

    // 模块级别覆盖
    Logger::instance()->setModuleLevel(LogModule::Network, LogLevel::Debug);
    QVERIFY(Logger::instance()->shouldLog(LogLevel::Debug, LogModule::Network));
    Logger::instance()->clearModuleLevel(LogModule::Network);
}

void TestLogging::testLoggerLogRecord() {
    QSignalSpy spy(Logger::instance(), &Logger::logRecord);

    Logger::instance()->setGlobalLevel(LogLevel::Debug);
    Logger::instance()->clearAppenders();

    Logger::instance()->log(LogLevel::Info, LogModule::Core, "Test message");

    QCOMPARE(spy.count(), 1);
    LogRecord record = spy.at(0).at(0).value<LogRecord>();
    QCOMPARE(record.level, LogLevel::Info);
    QCOMPARE(record.module, LogModule::Core);
    QCOMPARE(record.message, QString("Test message"));
}

void TestLogging::testLoggerAppenderManagement() {
    // 清除现有 appenders
    Logger::instance()->clearAppenders();
    QCOMPARE(Logger::instance()->appenders().size(), 0);

    // 添加 appender
    auto appender = QSharedPointer<ConsoleAppender>::create(false);
    Logger::instance()->addAppender(appender);
    QCOMPARE(Logger::instance()->appenders().size(), 1);

    // 重复添加不应该增加
    Logger::instance()->addAppender(appender);
    QCOMPARE(Logger::instance()->appenders().size(), 1);

    // 移除 appender
    Logger::instance()->removeAppender(appender);
    QCOMPARE(Logger::instance()->appenders().size(), 0);
}

// ========== 日志宏测试 ==========

void TestLogging::testLogMacros() {
    Logger::instance()->clearAppenders();
    Logger::instance()->setGlobalLevel(LogLevel::Trace);

    QSignalSpy spy(Logger::instance(), &Logger::logRecord);

    LOG_TRACE(LogModule::Core, "Trace message");
    LOG_DEBUG(LogModule::Network, "Debug message");
    LOG_INFO(LogModule::Export, "Info message");
    LOG_WARN(LogModule::UI, "Warn message");
    LOG_ERROR(LogModule::Pipeline, "Error message");

    QCOMPARE(spy.count(), 5);

    // 验证级别
    QCOMPARE(spy.at(0).at(0).value<LogRecord>().level, LogLevel::Trace);
    QCOMPARE(spy.at(1).at(0).value<LogRecord>().level, LogLevel::Debug);
    QCOMPARE(spy.at(2).at(0).value<LogRecord>().level, LogLevel::Info);
    QCOMPARE(spy.at(3).at(0).value<LogRecord>().level, LogLevel::Warn);
    QCOMPARE(spy.at(4).at(0).value<LogRecord>().level, LogLevel::Error);
}

// ========== QtLogAdapter 测试 ==========

void TestLogging::testQtLogAdapter() {
    Logger::instance()->clearAppenders();
    Logger::instance()->setGlobalLevel(LogLevel::Debug);

    QSignalSpy spy(Logger::instance(), &Logger::logRecord);

    // 安装适配器
    QtLogAdapter::install();
    QVERIFY2(QtLogAdapter::isInstalled(), "QtLogAdapter should be installed");
    QtLogAdapter::setDefaultModule(LogModule::Core);

    // 使用 Qt 日志 — 用 qInfo/qWarning 而非 qDebug，因为 CI Qt release 构建下 qDebug 可能是 no-op
    qInfo() << "Qt info message";
    qWarning() << "Qt warning message";

    // 验证 Qt 日志被捕获 — QTRY_VERIFY 适应 offscreen 平台下可能的异步投递
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() == 2, 1000);

    // 卸载适配器
    QtLogAdapter::uninstall();
}

QTEST_MAIN(TestLogging)
#include "test_logging.moc"
