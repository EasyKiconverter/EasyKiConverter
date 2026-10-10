#include "core/utils/AtomicFileWriter.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestAtomicFileWriter : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 验证 writeAtomicallyWritesDataAndCreatesTempDirectory 测试场景。
     */
    void writeAtomicallyWritesDataAndCreatesTempDirectory() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // workDir：测试使用的文件系统路径。
        const QString workDir = tempDir.path();
        // atomicTempDir：测试使用的文件系统路径。
        const QString atomicTempDir = QDir(workDir).filePath(QStringLiteral(".atomic"));
        // finalPath：测试使用的文件系统路径。
        const QString finalPath = QDir(workDir).filePath(QStringLiteral("output.txt"));

        QVERIFY(AtomicFileWriter::writeAtomically(
            atomicTempDir, finalPath, QStringLiteral(".tmp"), QByteArrayLiteral("new content")));

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("new content"));
        QVERIFY(QDir(atomicTempDir).exists());
        QVERIFY(tempFiles(atomicTempDir).isEmpty());
    }

    /**
     * @brief 验证 writeAtomicallyKeepsExistingFileAndRemovesTempOnWriterFailure 测试场景。
     */
    void writeAtomicallyKeepsExistingFileAndRemovesTempOnWriterFailure() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // workDir：测试使用的文件系统路径。
        const QString workDir = tempDir.path();
        // atomicTempDir：测试使用的文件系统路径。
        const QString atomicTempDir = QDir(workDir).filePath(QStringLiteral(".atomic"));
        // finalPath：测试使用的文件系统路径。
        const QString finalPath = QDir(workDir).filePath(QStringLiteral("output.txt"));
        QVERIFY(writeFile(finalPath, QByteArrayLiteral("old content")));

        QVERIFY(!AtomicFileWriter::writeAtomically(
            atomicTempDir, finalPath, QStringLiteral(".tmp"), [](const QString& tempPath) {
                QFile file(tempPath);
                if (!file.open(QIODevice::WriteOnly)) {
                    return false;
                }
                file.write("partial content");
                file.close();
                return false;
            }));

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("old content"));
        QVERIFY(tempFiles(atomicTempDir).isEmpty());
    }

    /**
     * @brief 验证 writeAtomicallyReplacesExistingFile 测试场景。
     */
    void writeAtomicallyReplacesExistingFile() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // workDir：测试使用的文件系统路径。
        const QString workDir = tempDir.path();
        // atomicTempDir：测试使用的文件系统路径。
        const QString atomicTempDir = QDir(workDir).filePath(QStringLiteral(".atomic"));
        // finalPath：测试使用的文件系统路径。
        const QString finalPath = QDir(workDir).filePath(QStringLiteral("output.txt"));
        QVERIFY(writeFile(finalPath, QByteArrayLiteral("old content")));

        QVERIFY(AtomicFileWriter::writeAtomically(
            atomicTempDir, finalPath, QStringLiteral(".tmp"), QByteArrayLiteral("replacement")));

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("replacement"));
        QVERIFY(tempFiles(atomicTempDir).isEmpty());
    }

    /**
     * @brief 验证 copyAtomicallyCopiesAndReplacesExistingFile 测试场景。
     */
    void copyAtomicallyCopiesAndReplacesExistingFile() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // workDir：测试使用的文件系统路径。
        const QString workDir = tempDir.path();
        // atomicTempDir：测试使用的文件系统路径。
        const QString atomicTempDir = QDir(workDir).filePath(QStringLiteral(".atomic"));
        // sourcePath：测试使用的文件系统路径。
        const QString sourcePath = QDir(workDir).filePath(QStringLiteral("source.step"));
        // finalPath：测试使用的文件系统路径。
        const QString finalPath = QDir(workDir).filePath(QStringLiteral("model.step"));

        QVERIFY(writeFile(sourcePath, QByteArrayLiteral("source model")));
        QVERIFY(writeFile(finalPath, QByteArrayLiteral("old model")));

        QVERIFY(AtomicFileWriter::copyAtomically(sourcePath, finalPath, atomicTempDir));

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("source model"));
        QCOMPARE(readFile(sourcePath), QByteArrayLiteral("source model"));
        QVERIFY(tempFiles(atomicTempDir).isEmpty());
    }

    /**
     * @brief 验证 copyAtomicallyFailsForMissingSourceWithoutTouchingDestination 测试场景。
     */
    void copyAtomicallyFailsForMissingSourceWithoutTouchingDestination() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // workDir：测试使用的文件系统路径。
        const QString workDir = tempDir.path();
        // atomicTempDir：测试使用的文件系统路径。
        const QString atomicTempDir = QDir(workDir).filePath(QStringLiteral(".atomic"));
        // sourcePath：测试使用的文件系统路径。
        const QString sourcePath = QDir(workDir).filePath(QStringLiteral("missing.step"));
        // finalPath：测试使用的文件系统路径。
        const QString finalPath = QDir(workDir).filePath(QStringLiteral("model.step"));
        QVERIFY(writeFile(finalPath, QByteArrayLiteral("old model")));

        QVERIFY(!AtomicFileWriter::copyAtomically(sourcePath, finalPath, atomicTempDir));

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("old model"));
        QVERIFY(!QDir(atomicTempDir).exists());
    }

private:
    static bool writeFile(const QString& path, const QByteArray& data) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        return file.write(data) == data.size();
    }

    static QByteArray readFile(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            QTest::qFail(qPrintable(QStringLiteral("Unable to open file '%1': %2").arg(path, file.errorString())),
                         __FILE__,
                         __LINE__);
            return {};
        }
        return file.readAll();
    }

    static QStringList tempFiles(const QString& path) {
        QDir dir(path);
        if (!dir.exists()) {
            return {};
        }
        return dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    }
};

QTEST_GUILESS_MAIN(TestAtomicFileWriter)
#include "test_atomic_file_writer.moc"
