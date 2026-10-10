#include "utils/CommandLineParser.h"
#include "utils/cli/BatchConverter.h"
#include "utils/cli/BomConverter.h"
#include "utils/cli/CliContext.h"
#include "utils/cli/CliConverter.h"
#include "utils/cli/ComponentConverter.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestCliConverters : public QObject {
    Q_OBJECT

private:
    // m_argStorage：命令行参数字符串存储。
    QList<QByteArray> m_argStorage;
    // m_argvPtrs：命令行参数指针数组。
    QList<char*> m_argvPtrs;
    int m_argc{0};
    char** m_argv{nullptr};

    /**
     * @brief 验证 makeParser 测试场景。
     * @param args 命令行参数列表。
     */
    CommandLineParser* makeParser(const QStringList& args) {
        m_argStorage.clear();
        m_argvPtrs.clear();
        m_argc = args.size();

        for (const QString& arg : args) {
            m_argStorage.append(arg.toUtf8());
        }

        for (const QByteArray& a : m_argStorage) {
            m_argvPtrs.append(const_cast<char*>(a.constData()));
        }
        m_argv = m_argvPtrs.isEmpty() ? nullptr : m_argvPtrs.data();

        return new CommandLineParser(m_argc, m_argv);
    }

private slots:

    // ========== CliConverter 分发 ==========

    /**
     * @brief 验证 cliConverterRejectsUnknownCliMode 测试场景。
     */
    void cliConverterRejectsUnknownCliMode() {
        // parser：负责解析测试输入的解析器实例。
        auto* parser = makeParser({"easykiconverter", "convert", "unknown", "-o", "out"});
        QVERIFY(parser->parse());

        CliConverter converter(*parser);
        QVERIFY(!converter.execute());
        QVERIFY(!converter.errorMessage().isEmpty());

        delete parser;
    }

    /**
     * @brief 验证 cliConverterDispatchesToBomConverter 测试场景。
     */
    void cliConverterDispatchesToBomConverter() {
        // outDir：测试使用的文件系统路径。
        QTemporaryDir outDir;
        QVERIFY(outDir.isValid());

        auto* parser =
            makeParser({"easykiconverter", "convert", "bom", "-i", "/nonexistent/bom.xlsx", "-o", outDir.path()});
        QVERIFY(parser->parse());

        CliConverter converter(*parser);
        QVERIFY(!converter.execute());  // 文件不存在
        QVERIFY(!converter.errorMessage().isEmpty());

        delete parser;
    }

    /**
     * @brief 验证 cliConverterDispatchesToBatchConverter 测试场景。
     */
    void cliConverterDispatchesToBatchConverter() {
        // outDir：测试使用的文件系统路径。
        QTemporaryDir outDir;
        QVERIFY(outDir.isValid());

        auto* parser =
            makeParser({"easykiconverter", "convert", "batch", "-i", "/nonexistent/list.txt", "-o", outDir.path()});
        QVERIFY(parser->parse());

        CliConverter converter(*parser);
        QVERIFY(!converter.execute());  // 文件不存在
        QVERIFY(!converter.errorMessage().isEmpty());

        delete parser;
    }

    // ========== ComponentConverter 前置验证 ==========

    /**
     * @brief 验证 componentConverterFailsWhenComponentIdIsEmpty 测试场景。
     */
    void componentConverterFailsWhenComponentIdIsEmpty() {
        // parser：负责解析测试输入的解析器实例。
        auto* parser = makeParser({"easykiconverter", "convert", "component", "-o", "/tmp/out"});
        QVERIFY(parser->parse());

        CliContext context(*parser);
        ComponentConverter converter(&context);
        QVERIFY(!converter.execute());
        QVERIFY(!converter.errorMessage().isEmpty());

        delete parser;
    }

    /**
     * @brief 验证 componentConverterErrorMessageForEmptyId 测试场景。
     */
    void componentConverterErrorMessageForEmptyId() {
        // parser：负责解析测试输入的解析器实例。
        auto* parser = makeParser({"easykiconverter", "convert", "component", "-o", "/tmp/out"});
        QVERIFY(parser->parse());

        CliContext context(*parser);
        ComponentConverter converter(&context);
        QVERIFY(!converter.execute());
        QCOMPARE(converter.errorMessage(), QCoreApplication::translate("CliConverter", "未指定元器件编号"));

        delete parser;
    }

    // ========== BomConverter 前置验证 ==========

    /**
     * @brief 验证 bomConverterFailsWhenInputFileIsMissing 测试场景。
     */
    void bomConverterFailsWhenInputFileIsMissing() {
        // outDir：测试使用的文件系统路径。
        QTemporaryDir outDir;
        QVERIFY(outDir.isValid());

        auto* parser =
            makeParser({"easykiconverter", "convert", "bom", "-i", "/nonexistent/bom.xlsx", "-o", outDir.path()});
        QVERIFY(parser->parse());

        CliContext context(*parser);
        BomConverter converter(&context);
        QVERIFY(!converter.execute());
        QVERIFY(!converter.errorMessage().isEmpty());

        delete parser;
    }

    // ========== BatchConverter 前置验证 ==========

    /**
     * @brief 验证 batchConverterFailsWhenInputFileIsMissing 测试场景。
     */
    void batchConverterFailsWhenInputFileIsMissing() {
        // outDir：测试使用的文件系统路径。
        QTemporaryDir outDir;
        QVERIFY(outDir.isValid());

        auto* parser =
            makeParser({"easykiconverter", "convert", "batch", "-i", "/nonexistent/list.txt", "-o", outDir.path()});
        QVERIFY(parser->parse());

        CliContext context(*parser);
        BatchConverter converter(&context);
        QVERIFY(!converter.execute());
        QVERIFY(!converter.errorMessage().isEmpty());

        delete parser;
    }
};

QTEST_GUILESS_MAIN(TestCliConverters)
#include "test_cli_converters.moc"
