#include "tests/common/TestPaths.hpp"

#include <QFileInfo>
#include <QTest>

using namespace EasyKiConverter::Test;

class TestTestPaths : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 验证 testTestsRootExists 测试场景。
     */
    void testTestsRootExists() {
        QVERIFY2(QFileInfo::exists(TestPaths::testsRoot()), qPrintable(TestPaths::testsRoot()));
    }

    /**
     * @brief 验证 testFixtureAndGoldenPaths 测试场景。
     */
    void testFixtureAndGoldenPaths() {
        QCOMPARE(TestPaths::fixturePath(QStringLiteral("easyeda/.gitkeep")),
                 QDir::cleanPath(TestPaths::testsRoot() + QStringLiteral("/fixtures/easyeda/.gitkeep")));
        QCOMPARE(TestPaths::goldenPath(QStringLiteral("kicad/.gitkeep")),
                 QDir::cleanPath(TestPaths::testsRoot() + QStringLiteral("/golden/kicad/.gitkeep")));
    }

    /**
     * @brief 验证 testCompareTextToGolden 测试场景。
     */
    void testCompareTextToGolden() {
        // error：测试期间收集的错误或诊断信息。
        QString error;
        // expected：测试期望的结果数据。
        const QString expected = TestPaths::readText(TestPaths::goldenPath(QStringLiteral("README.md")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        QVERIFY2(TestPaths::compareTextToGolden(expected, QStringLiteral("README.md"), &error), qPrintable(error));
        QVERIFY2(
            !TestPaths::compareTextToGolden(expected + QStringLiteral("\nextra"), QStringLiteral("README.md"), &error),
            "Mismatch should be reported for modified content");
        QVERIFY2(error.contains(QStringLiteral("Golden file mismatch")), qPrintable(error));
    }

    /**
     * @brief 验证 testNormalizeGoldenMetadataStripsGeneratorVersionComment 测试场景。
     */
    void testNormalizeGoldenMetadataStripsGeneratorVersionComment() {
        const QString withMetadata =
            QStringLiteral("; (generator_version \"3.1.8\")\n(kicad_symbol_lib\n  (version 20211014)\n)\n");
        // withoutMetadata：测试使用的文本或数据内容。
        const QString withoutMetadata = QStringLiteral("(kicad_symbol_lib\n  (version 20211014)\n)\n");

        QCOMPARE(TestPaths::normalizeGoldenMetadata(withMetadata), withoutMetadata);
        QCOMPARE(TestPaths::normalizeGoldenMetadata(withoutMetadata), withoutMetadata);
    }
};

QTEST_GUILESS_MAIN(TestTestPaths)
#include "test_test_paths.moc"
