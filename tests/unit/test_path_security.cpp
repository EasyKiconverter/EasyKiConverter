#include "utils/PathSecurity.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestPathSecurity : public QObject {
    Q_OBJECT

private slots:

    // 验证路径组件会拒绝空值、遍历片段和非法字符。
    /**
     * @brief 验证 validatesPathComponents 测试场景。
     */
    void validatesPathComponents() {
        QVERIFY(PathSecurity::isValidPathComponent(QStringLiteral("R0603")));
        QVERIFY(!PathSecurity::isValidPathComponent(QString()));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral(".")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("..")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("../etc/passwd")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("foo/bar")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("foo\\bar")));
        QVERIFY(PathSecurity::isValidPathComponent(QStringLiteral("work#1")));
        QVERIFY(PathSecurity::isValidPathComponent(QStringLiteral("器件_0603")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("foo?bar")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("foo%1bar").arg(QChar(0x01))));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("CON.txt")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("component.")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("component ")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("zero%1width").arg(QChar(0x200B))));
    }

    // 验证目标路径检查能够阻止离开基准目录的访问。
    /**
     * @brief 验证 detectsPathsOutsideBaseDirectory 测试场景。
     */
    void detectsPathsOutsideBaseDirectory() {
        // baseDir：测试使用的文件系统路径。
        QTemporaryDir baseDir;
        // outsideDir：测试使用的文件系统路径。
        QTemporaryDir outsideDir;
        QVERIFY(baseDir.isValid());
        QVERIFY(outsideDir.isValid());

        // childPath：测试使用的文件系统路径。
        const QString childPath = QDir(baseDir.path()).filePath(QStringLiteral("child/file.txt"));
        // traversalPath：测试使用的文件系统路径。
        const QString traversalPath = QDir(baseDir.path()).filePath(QStringLiteral("../outside.txt"));

        QVERIFY(PathSecurity::isSafePath(baseDir.path(), baseDir.path()));
        QVERIFY(PathSecurity::isSafePath(childPath, baseDir.path()));
        QVERIFY(!PathSecurity::isSafePath(outsideDir.path(), baseDir.path()));
        QVERIFY(!PathSecurity::isSafePath(traversalPath, baseDir.path()));
        QVERIFY(!PathSecurity::isSafePath(QString(), baseDir.path()));
    }

    // 验证文件名清洗会替换非法字符并处理保留名称。
    /**
     * @brief 验证 sanitizesFilenames 测试场景。
     */
    void sanitizesFilenames() {
        QCOMPARE(PathSecurity::sanitizeFilename(QStringLiteral("foo/bar:baz*?.kicad_mod")),
                 QStringLiteral("foo_bar_baz__.kicad_mod"));
        QCOMPARE(PathSecurity::sanitizeFilename(QStringLiteral("  CON  ")), QStringLiteral("CON_safe"));
        QCOMPARE(PathSecurity::sanitizeFilename(QStringLiteral("\x01\x02")), QStringLiteral("unnamed_component"));
        QCOMPARE(PathSecurity::sanitizeFilename(QStringLiteral("zero%1width").arg(QChar(0x200B))),
                 QStringLiteral("zerowidth"));
    }

    // 验证递归删除会遵守文件数量上限并清理目标目录。
    /**
     * @brief 验证 safeRemoveRecursivelyHonorsFileLimit 测试场景。
     */
    void safeRemoveRecursivelyHonorsFileLimit() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QDir dir(tempDir.path());
        QVERIFY(dir.mkpath(QStringLiteral("target")));

        QFile first(dir.filePath(QStringLiteral("target/one.txt")));
        QVERIFY(first.open(QIODevice::WriteOnly));
        first.write("1");
        first.close();

        QFile second(dir.filePath(QStringLiteral("target/two.txt")));
        QVERIFY(second.open(QIODevice::WriteOnly));
        second.write("2");
        second.close();

        // targetPath：测试使用的文件系统路径。
        const QString targetPath = dir.filePath(QStringLiteral("target"));
        QVERIFY(!PathSecurity::safeRemoveRecursively(targetPath, 1));
        QVERIFY(QDir(targetPath).exists());
        QVERIFY(PathSecurity::safeRemoveRecursively(targetPath, 10));
        QVERIFY(!QDir(targetPath).exists());
    }
};

QTEST_GUILESS_MAIN(TestPathSecurity)
#include "test_path_security.moc"
