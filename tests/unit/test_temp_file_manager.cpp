#include "services/export/TempFileManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestTempFileManager : public QObject {
    Q_OBJECT

private slots:

    // 验证符号临时文件提交后会移动到目标位置并注销临时文件。
    /**
     * @brief 验证 symbolTempFileCommitMovesFileAndUnregistersIt 测试场景。
     */
    void symbolTempFileCommitMovesFileAndUnregistersIt() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // manager：被测管理器实例。
        TempFileManager manager;
        manager.setOutputPath(tempDir.path());
        QSignalSpy commitSpy(&manager, &TempFileManager::commitCompleted);

        const QString tempPath =
            manager.createSymbolTempPath(QStringLiteral("FixtureLib"), QStringLiteral(".kicad_sym"));
        QVERIFY(!tempPath.isEmpty());
        QVERIFY(writeFile(tempPath, QByteArrayLiteral("symbol library")));

        // finalPath：测试使用的文件系统路径。
        const QString finalPath = QDir(tempDir.path()).filePath(QStringLiteral("FixtureLib.kicad_sym"));
        QVERIFY(manager.commit(finalPath));

        QCOMPARE(commitSpy.count(), 1);
        QCOMPARE(commitSpy.at(0).at(0).toString(), finalPath);
        QCOMPARE(commitSpy.at(0).at(1).toBool(), true);
        QCOMPARE(readFile(finalPath), QByteArrayLiteral("symbol library"));
        QVERIFY(!QFile::exists(tempPath));
        QVERIFY(!QDir(QDir(tempDir.path()).filePath(QStringLiteral(".tmp"))).exists());
        QVERIFY(manager.registeredTempFiles().isEmpty());
    }

    // 验证一个管理器提交时不会删除其他管理器仍在使用的临时目录。
    /**
     * @brief 验证 sharedTempDirectorySurvivesOtherManagerCommit 测试场景。
     */
    void sharedTempDirectorySurvivesOtherManagerCommit() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // firstManager：被测管理器实例。
        TempFileManager firstManager;
        // secondManager：被测管理器实例。
        TempFileManager secondManager;
        firstManager.setOutputPath(tempDir.path());
        secondManager.setOutputPath(tempDir.path());

        const QString firstTempPath =
            firstManager.createSymbolTempPath(QStringLiteral("First"), QStringLiteral(".kicad_sym"));
        QVERIFY(writeFile(firstTempPath, QByteArrayLiteral("first")));
        // firstFinalPath：测试使用的文件系统路径。
        const QString firstFinalPath = QDir(tempDir.path()).filePath(QStringLiteral("First.kicad_sym"));
        QVERIFY(firstManager.commit(firstFinalPath));
        QVERIFY(QDir(firstManager.tempDirectory()).exists());

        const QString secondTempPath =
            secondManager.createSymbolTempPath(QStringLiteral("Second"), QStringLiteral(".kicad_sym"));
        QVERIFY(writeFile(secondTempPath, QByteArrayLiteral("second")));
        secondManager.rollbackAll();
        QVERIFY(QDir(firstManager.tempDirectory()).exists());
    }

    // 验证共享临时目录清理不会删除其他管理器仍在使用的文件。
    /**
     * @brief 验证 sharedTempDirectorySkipsCleanupWhileInUse 测试场景。
     */
    void sharedTempDirectorySkipsCleanupWhileInUse() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // firstManager：被测管理器实例。
        TempFileManager firstManager;
        // secondManager：被测管理器实例。
        TempFileManager secondManager;
        firstManager.setOutputPath(tempDir.path());
        secondManager.setOutputPath(tempDir.path());

        const QString tempPath =
            firstManager.createSymbolTempPath(QStringLiteral("Shared"), QStringLiteral(".kicad_sym"));
        QVERIFY(writeFile(tempPath, QByteArrayLiteral("shared")));

        secondManager.cleanupTempDirectory();
        QVERIFY(QFile::exists(tempPath));

        secondManager.cleanupOrphanedTempFiles();
        QVERIFY(QFile::exists(tempPath));
    }

    // 验证临时目录提交会替换已有目标目录。
    /**
     * @brief 验证 tempDirectoryCommitMovesDirectoryAndReplacesExistingTarget 测试场景。
     */
    void tempDirectoryCommitMovesDirectoryAndReplacesExistingTarget() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // manager：被测管理器实例。
        TempFileManager manager;
        manager.setOutputPath(tempDir.path());

        // finalDirPath：测试使用的文件系统路径。
        const QString finalDirPath = QDir(tempDir.path()).filePath(QStringLiteral("Fixture.pretty"));
        QVERIFY(QDir().mkpath(finalDirPath));
        QVERIFY(writeFile(QDir(finalDirPath).filePath(QStringLiteral("old.kicad_mod")), QByteArrayLiteral("old")));

        // tempDirPath：测试使用的文件系统路径。
        const QString tempDirPath = manager.createTempDirectoryPath(QStringLiteral("Fixture.pretty"));
        QVERIFY(!tempDirPath.isEmpty());
        QVERIFY(QDir().mkpath(tempDirPath));
        QVERIFY(writeFile(QDir(tempDirPath).filePath(QStringLiteral("new.kicad_mod")), QByteArrayLiteral("new")));

        QVERIFY(manager.commitDirectory(finalDirPath));

        QVERIFY(!QDir(tempDirPath).exists());
        QVERIFY(!QDir(QDir(tempDir.path()).filePath(QStringLiteral(".tmp"))).exists());
        QVERIFY(!QFile::exists(QDir(finalDirPath).filePath(QStringLiteral("old.kicad_mod"))));
        QCOMPARE(readFile(QDir(finalDirPath).filePath(QStringLiteral("new.kicad_mod"))), QByteArrayLiteral("new"));
        QVERIFY(manager.registeredTempFiles().isEmpty());
    }

    // 验证文件提交会备份已有目标文件。
    /**
     * @brief 验证 fileCommitBacksUpExistingTarget 测试场景。
     */
    void fileCommitBacksUpExistingTarget() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // manager：被测管理器实例。
        TempFileManager manager;
        manager.setOutputPath(tempDir.path());

        // finalPath：测试使用的文件系统路径。
        const QString finalPath = QDir(tempDir.path()).filePath(QStringLiteral("FixtureLib.kicad_sym"));
        QVERIFY(writeFile(finalPath, QByteArrayLiteral("old symbol library")));

        const QString tempPath =
            manager.createSymbolTempPath(QStringLiteral("FixtureLib"), QStringLiteral(".kicad_sym"));
        QVERIFY(writeFile(tempPath, QByteArrayLiteral("new symbol library")));

        QVERIFY(manager.commit(finalPath));

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("new symbol library"));
        // backupPath：测试使用的文件系统路径。
        const QString backupPath = committedBackupPathFor(finalPath);
        QVERIFY2(!backupPath.isEmpty(), "Committed manifest should retain the old file backup");
        QCOMPARE(readFile(backupPath), QByteArrayLiteral("old symbol library"));
        QDir(QFileInfo(backupPath).absoluteDir().absolutePath()).removeRecursively();
    }

    // 验证目录提交会备份已有目标目录。
    /**
     * @brief 验证 directoryCommitBacksUpExistingTarget 测试场景。
     */
    void directoryCommitBacksUpExistingTarget() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // manager：被测管理器实例。
        TempFileManager manager;
        manager.setOutputPath(tempDir.path());

        // finalDirPath：测试使用的文件系统路径。
        const QString finalDirPath = QDir(tempDir.path()).filePath(QStringLiteral("Fixture.pretty"));
        QVERIFY(QDir().mkpath(finalDirPath));
        QVERIFY(writeFile(QDir(finalDirPath).filePath(QStringLiteral("old.kicad_mod")), QByteArrayLiteral("old")));

        // tempDirPath：测试使用的文件系统路径。
        const QString tempDirPath = manager.createTempDirectoryPath(QStringLiteral("Fixture.pretty"));
        QVERIFY(QDir().mkpath(tempDirPath));
        QVERIFY(writeFile(QDir(tempDirPath).filePath(QStringLiteral("new.kicad_mod")), QByteArrayLiteral("new")));

        QVERIFY(manager.commitDirectory(finalDirPath));

        QCOMPARE(readFile(QDir(finalDirPath).filePath(QStringLiteral("new.kicad_mod"))), QByteArrayLiteral("new"));
        // backupPath：测试使用的文件系统路径。
        const QString backupPath = committedBackupPathFor(finalDirPath);
        QVERIFY2(!backupPath.isEmpty(), "Committed manifest should retain the old directory backup");
        QCOMPARE(readFile(QDir(backupPath).filePath(QStringLiteral("old.kicad_mod"))), QByteArrayLiteral("old"));
        QDir(QFileInfo(backupPath).absoluteDir().absolutePath()).removeRecursively();
    }

    // 验证未完成事务恢复时会还原缺失的最终文件。
    /**
     * @brief 验证 recoverIncompleteTransactionRestoresMissingFinalFile 测试场景。
     */
    void recoverIncompleteTransactionRestoresMissingFinalFile() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // backupDir：测试使用的文件系统路径。
        const QString backupDir = backupRootPath() + QDir::separator() + QStringLiteral("unit_recovery_tx");
        QDir(backupDir).removeRecursively();
        QVERIFY(QDir().mkpath(backupDir));

        // finalPath：测试使用的文件系统路径。
        const QString finalPath = QDir(tempDir.path()).filePath(QStringLiteral("recover.kicad_sym"));
        // backupPath：测试使用的文件系统路径。
        const QString backupPath = QDir(backupDir).filePath(QStringLiteral("recover.kicad_sym"));
        // tempPath：测试使用的文件系统路径。
        const QString tempPath = QDir(tempDir.path()).filePath(QStringLiteral("recover.tmp"));
        QVERIFY(writeFile(backupPath, QByteArrayLiteral("old")));
        QVERIFY(writeFile(tempPath, QByteArrayLiteral("new")));

        // entry：测试使用的缓存或元器件对象。
        QJsonObject entry;
        entry.insert(QStringLiteral("finalPath"), finalPath);
        entry.insert(QStringLiteral("backupPath"), backupPath);
        entry.insert(QStringLiteral("tempPath"), tempPath);
        entry.insert(QStringLiteral("isDirectory"), false);
        entry.insert(QStringLiteral("existedBefore"), true);

        // manifest：导出清单对象。
        QJsonObject manifest;
        manifest.insert(QStringLiteral("transactionId"), QStringLiteral("unit_recovery_tx"));
        manifest.insert(QStringLiteral("outputRoot"), tempDir.path());
        manifest.insert(QStringLiteral("committed"), false);
        manifest.insert(QStringLiteral("entries"), QJsonArray{entry});
        QVERIFY(writeFile(QDir(backupDir).filePath(QStringLiteral("manifest.json")),
                          QJsonDocument(manifest).toJson(QJsonDocument::Indented)));

        QVERIFY(TempFileManager::recoverIncompleteTransactions());

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("old"));
        QVERIFY(!QFile::exists(tempPath));
        QDir(backupDir).removeRecursively();
    }

    // 验证回滚会删除已登记的文件、目录和临时目录。
    /**
     * @brief 验证 rollbackAllDeletesRegisteredFilesAndDirectories 测试场景。
     */
    void rollbackAllDeletesRegisteredFilesAndDirectories() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // manager：被测管理器实例。
        TempFileManager manager;
        manager.setOutputPath(tempDir.path());
        QSignalSpy cleanupSpy(&manager, &TempFileManager::cleanupCompleted);

        const QString tempFilePath =
            manager.createSymbolTempPath(QStringLiteral("RollbackLib"), QStringLiteral(".kicad_sym"));
        QVERIFY(writeFile(tempFilePath, QByteArrayLiteral("temp")));

        // tempDirPath：测试使用的文件系统路径。
        const QString tempDirPath = manager.createTempDirectoryPath(QStringLiteral("Rollback.pretty"));
        QVERIFY(QDir().mkpath(tempDirPath));
        QVERIFY(writeFile(QDir(tempDirPath).filePath(QStringLiteral("temp.kicad_mod")), QByteArrayLiteral("temp")));

        manager.rollbackAll();

        QCOMPARE(cleanupSpy.count(), 1);
        QCOMPARE(cleanupSpy.at(0).at(0).toInt(), 2);
        QVERIFY(!QFile::exists(tempFilePath));
        QVERIFY(!QDir(tempDirPath).exists());
        QVERIFY(!QDir(QDir(tempDir.path()).filePath(QStringLiteral(".tmp"))).exists());
        QVERIFY(manager.registeredTempFiles().isEmpty());
    }

    // 验证清理临时目录会删除未登记文件并移除空目录。
    /**
     * @brief 清理测试用例使用的资源。
     */
    void cleanupTempDirectoryDeletesUntrackedFilesAndRemovesEmptyDirectory() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // manager：被测管理器实例。
        TempFileManager manager;
        manager.setOutputPath(tempDir.path());
        QSignalSpy cleanupSpy(&manager, &TempFileManager::cleanupCompleted);

        // tempDirectory：测试使用的文件系统路径。
        const QString tempDirectory = manager.tempDirectory();
        QVERIFY(QDir().mkpath(tempDirectory));
        QVERIFY(writeFile(QDir(tempDirectory).filePath(QStringLiteral("leftover.tmp")), QByteArrayLiteral("temp")));

        manager.cleanupTempDirectory();

        QCOMPARE(cleanupSpy.count(), 1);
        QCOMPARE(cleanupSpy.at(0).at(0).toInt(), 1);
        QVERIFY(!QDir(tempDirectory).exists());
    }

    // 验证孤立临时文件清理会保留隐藏文件。
    /**
     * @brief 清理测试用例使用的资源。
     * @param argv 进程命令行参数列表。
     */
    void cleanupOrphanedTempFilesKeepsHiddenFiles() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // manager：被测管理器实例。
        TempFileManager manager;
        manager.setOutputPath(tempDir.path());

        // tempDirectory：测试使用的文件系统路径。
        const QString tempDirectory = manager.tempDirectory();
        QVERIFY(QDir().mkpath(tempDirectory));
        // visibleFile：测试使用的文件系统路径。
        const QString visibleFile = QDir(tempDirectory).filePath(QStringLiteral("orphan.tmp"));
        // hiddenFile：测试使用的文件系统路径。
        const QString hiddenFile = QDir(tempDirectory).filePath(QStringLiteral(".keep"));
        QVERIFY(writeFile(visibleFile, QByteArrayLiteral("orphan")));
        QVERIFY(writeFile(hiddenFile, QByteArrayLiteral("hidden")));

        manager.cleanupOrphanedTempFiles();

        QVERIFY(!QFile::exists(visibleFile));
        QCOMPARE(readFile(hiddenFile), QByteArrayLiteral("hidden"));
        QVERIFY(QDir(tempDirectory).exists());
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

    static QString backupRootPath() {
        // appDataPath：测试使用的文件系统路径。
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (appDataPath.isEmpty()) {
            appDataPath = QDir::homePath() + QDir::separator() + QStringLiteral(".easykiconverter");
        }
        return QDir(appDataPath).filePath(QStringLiteral("backups"));
    }

    static QString committedBackupPathFor(const QString& finalPath) {
        QDir backupRoot(backupRootPath());
        // transactionDirs：测试使用的文件系统路径。
        const QFileInfoList transactionDirs = backupRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
        for (const QFileInfo& transactionDir : transactionDirs) {
            QFile manifestFile(QDir(transactionDir.absoluteFilePath()).filePath(QStringLiteral("manifest.json")));
            if (!manifestFile.open(QIODevice::ReadOnly)) {
                continue;
            }
            // manifest：导出清单对象。
            const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
            if (!manifest.value(QStringLiteral("committed")).toBool(false)) {
                continue;
            }
            // entries：测试用条目集合。
            const QJsonArray entries = manifest.value(QStringLiteral("entries")).toArray();
            for (const QJsonValue& value : entries) {
                // entry：测试使用的缓存或元器件对象。
                const QJsonObject entry = value.toObject();
                if (entry.value(QStringLiteral("finalPath")).toString() == finalPath) {
                    return entry.value(QStringLiteral("backupPath")).toString();
                }
            }
        }
        return QString();
    }
};

// 在创建 QCoreApplication 前启用测试路径，避免 Windows 测试读取真实用户数据目录。
/**
 * @brief 验证 main 测试场景。
 * @param argc 进程参数数量。
 * @param argv 进程命令行参数列表。
 */
int main(int argc, char* argv[]) {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication app(argc, argv);
    // testObject：测试构造的 JSON 数据对象。
    TestTempFileManager testObject;
    return QTest::qExec(&testObject, argc, argv);
}

#include "test_temp_file_manager.moc"
