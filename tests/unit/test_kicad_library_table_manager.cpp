#include "services/export/KiCadLibraryTableManager.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

using namespace EasyKiConverter;

class TestKiCadLibraryTableManager : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 验证 registerSymbolLibraryCreatesAndUpdatesProjectTable 测试场景。
     */
    void registerSymbolLibraryCreatesAndUpdatesProjectTable() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // projectRoot：测试使用的文件系统路径。
        const QString projectRoot = tempDir.path();
        QVERIFY(writeText(QDir(projectRoot).filePath(QStringLiteral("Board.kicad_pro")), QStringLiteral("{}\n")));

        // outputDir：测试使用的文件系统路径。
        const QString outputDir = QDir(projectRoot).filePath(QStringLiteral("exports"));
        QVERIFY(QDir().mkpath(outputDir));

        // firstLibraryPath：测试使用的文件系统路径。
        const QString firstLibraryPath = QDir(outputDir).filePath(QStringLiteral("FixtureLib.kicad_sym"));
        // secondLibraryPath：测试使用的文件系统路径。
        const QString secondLibraryPath = QDir(outputDir).filePath(QStringLiteral("FixtureLib_v2.kicad_sym"));

        QCOMPARE(KiCadLibraryTableManager::registerSymbolLibrary(
                     outputDir, QStringLiteral("FixtureLib"), firstLibraryPath, QStringLiteral("first description")),
                 KiCadLibraryTableManager::RegistrationResult::Registered);

        // tablePath：测试使用的文件系统路径。
        const QString tablePath = QDir(projectRoot).filePath(QStringLiteral("sym-lib-table"));
        // content：测试使用的文本或数据内容。
        QString content = readText(tablePath);
        QVERIFY(content.contains(QStringLiteral("(sym_lib_table")));
        QVERIFY(content.contains(QStringLiteral("(version 7)")));
        QVERIFY(content.contains(QStringLiteral("(name \"FixtureLib\")")));
        QVERIFY(content.contains(QStringLiteral("(uri \"${KIPRJMOD}/exports/FixtureLib.kicad_sym\")")));
        QVERIFY(content.contains(QStringLiteral("(descr \"first description\")")));

        QCOMPARE(KiCadLibraryTableManager::registerSymbolLibrary(
                     outputDir, QStringLiteral("FixtureLib"), secondLibraryPath, QStringLiteral("second description")),
                 KiCadLibraryTableManager::RegistrationResult::Registered);

        content = readText(tablePath);
        QCOMPARE(content.count(QStringLiteral("(name \"FixtureLib\")")), 1);
        QVERIFY(content.contains(QStringLiteral("(uri \"${KIPRJMOD}/exports/FixtureLib_v2.kicad_sym\")")));
        QVERIFY(content.contains(QStringLiteral("(descr \"second description\")")));
        QVERIFY(!content.contains(QStringLiteral("first description")));
    }

    /**
     * @brief 验证 registerFootprintLibraryWritesEscapedProjectEntry 测试场景。
     */
    void registerFootprintLibraryWritesEscapedProjectEntry() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // projectRoot：测试使用的文件系统路径。
        const QString projectRoot = tempDir.path();
        QVERIFY(writeText(QDir(projectRoot).filePath(QStringLiteral("Board.kicad_pro")), QStringLiteral("{}\n")));

        // outputDir：测试使用的文件系统路径。
        const QString outputDir = QDir(projectRoot).filePath(QStringLiteral("exports"));
        QVERIFY(QDir().mkpath(outputDir));

        // libName：测试使用的元器件标识或名称。
        const QString libName = QStringLiteral("FP \"Special\"");
        // libDirPath：测试使用的文件系统路径。
        const QString libDirPath = QDir(outputDir).filePath(QStringLiteral("FP Special.pretty"));
        // description：测试用描述文本。
        const QString description = QStringLiteral("contains \"quotes\" and C:\\KiCad");

        QCOMPARE(KiCadLibraryTableManager::registerFootprintLibrary(outputDir, libName, libDirPath, description),
                 KiCadLibraryTableManager::RegistrationResult::Registered);

        // content：测试使用的文本或数据内容。
        const QString content = readText(QDir(projectRoot).filePath(QStringLiteral("fp-lib-table")));
        QVERIFY(content.contains(QStringLiteral("(fp_lib_table")));
        QVERIFY(content.contains(QStringLiteral("(name \"FP \\\"Special\\\"\")")));
        QVERIFY(content.contains(QStringLiteral("(uri \"${KIPRJMOD}/exports/FP Special.pretty\")")));
        QVERIFY(content.contains(QStringLiteral("(descr \"contains \\\"quotes\\\" and C:\\\\KiCad\")")));
    }

    /**
     * @brief 验证 registerLibraryWithoutProjectWritesImportGuide 测试场景。
     */
    void registerLibraryWithoutProjectWritesImportGuide() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // outputDir：测试使用的文件系统路径。
        const QString outputDir = QDir(tempDir.path()).filePath(QStringLiteral("standalone-export"));
        // libFilePath：测试使用的文件系统路径。
        const QString libFilePath = QDir(outputDir).filePath(QStringLiteral("Standalone.kicad_sym"));

        QCOMPARE(KiCadLibraryTableManager::registerSymbolLibrary(
                     outputDir, QStringLiteral("Standalone"), libFilePath, QStringLiteral("standalone description")),
                 KiCadLibraryTableManager::RegistrationResult::ImportGuideWritten);

        // guidePath：测试使用的文件系统路径。
        const QString guidePath = QDir(outputDir).filePath(QStringLiteral("KiCad_IMPORT.md"));
        // guide：测试使用的元器件标识或名称。
        const QString guide = readText(guidePath);
        QVERIFY(guide.contains(QStringLiteral("did not find a KiCad project")));
        QVERIFY(guide.contains(QStringLiteral("sym-lib-table")));
        QVERIFY(guide.contains(QStringLiteral("fp-lib-table")));
    }

private:
    static bool writeText(const QString& path, const QString& content) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTest::qFail(
                qPrintable(QStringLiteral("Unable to open file for writing '%1': %2").arg(path, file.errorString())),
                __FILE__,
                __LINE__);
            return false;
        }
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << content;
        file.close();
        return true;
    }

    static QString readText(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTest::qFail(qPrintable(QStringLiteral("Unable to open file '%1': %2").arg(path, file.errorString())),
                         __FILE__,
                         __LINE__);
            return {};
        }

        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        return in.readAll();
    }
};

QTEST_GUILESS_MAIN(TestKiCadLibraryTableManager)
#include "test_kicad_library_table_manager.moc"
