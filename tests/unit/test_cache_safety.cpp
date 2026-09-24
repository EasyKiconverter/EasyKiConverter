#include "services/CacheSafety.h"
#include "services/ConfigService.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestCacheSafety : public QObject {
    Q_OBJECT

private slots:
    void rejectsNonEmptyUnownedDirectory();
    void rejectsHomeDirectory();
    void acceptsApplicationDefaultCachePath();
    void adoptsRecognizableLegacyCacheRoot();
    void movesOwnedEntryThroughInjectedTrash();
    void preservesEntryWhenTrashFails();
    void ignoresUnknownAndSymlinkEntries();
};

// 验证非空且未托管的目录不能被选作缓存目录。
void TestCacheSafety::rejectsNonEmptyUnownedDirectory() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("user-data.txt"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("user data") > 0);
    file.close();

    QString normalized;
    QString error;
    QVERIFY(!CacheSafety::validateSelection(tempDir.path(), &normalized, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(QFileInfo::exists(filePath));
}

// 验证用户主目录会被安全边界明确拒绝。
void TestCacheSafety::rejectsHomeDirectory() {
    QString normalized;
    QString error;
    QVERIFY(!CacheSafety::validateSelection(QDir::homePath(), &normalized, &error));
    QVERIFY(!error.isEmpty());

    const QString homeChild = QDir(QDir::homePath()).filePath(QStringLiteral("easykiconverter-cache-child"));
    error.clear();
    QVERIFY(!CacheSafety::validateSelection(homeChild, &normalized, &error));
    QVERIFY(!error.isEmpty());
}

// 验证应用默认缓存目录虽位于应用数据目录下，但不会被用户主目录保护规则误拒绝。
void TestCacheSafety::acceptsApplicationDefaultCachePath() {
    QString normalized;
    QString error;
    QVERIFY2(CacheSafety::validateSelection(ConfigService::defaultCacheDir(), &normalized, &error), qPrintable(error));
}

// 验证旧版本可识别缓存根目录能够安全补齐当前所有权标记。
void TestCacheSafety::adoptsRecognizableLegacyCacheRoot() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString componentPath = QDir(tempDir.path()).filePath(QStringLiteral("C10003"));
    QVERIFY(QDir().mkpath(componentPath));
    QFile metadata(QDir(componentPath).filePath(QStringLiteral("component.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral("{\"lcscId\":\"C10003\"}")) > 0);
    metadata.close();

    const QString modelPath = QDir(tempDir.path()).filePath(QStringLiteral("model3d/model.step"));
    QVERIFY(QDir().mkpath(QFileInfo(modelPath).absolutePath()));
    QFile model(modelPath);
    QVERIFY(model.open(QIODevice::WriteOnly));
    QVERIFY(model.write("step") > 0);
    model.close();

    QString error;
    QVERIFY(CacheSafety::canAdoptLegacyRoot(tempDir.path()));
    QVERIFY(CacheSafety::ensureOwnedRoot(tempDir.path(), &error));
    QVERIFY2(CacheSafety::ensureOwnedModel3DDirectory(tempDir.path(), &error), qPrintable(error));
    QVERIFY(CacheSafety::isOwnedRoot(tempDir.path()));
    QVERIFY(CacheSafety::isOwnedComponentDirectory(tempDir.path(), componentPath));
    QVERIFY(CacheSafety::isOwnedModel3DFile(tempDir.path(), modelPath));
}

// 验证已托管缓存条目通过可注入回收站移动，并且原路径消失。
void TestCacheSafety::movesOwnedEntryThroughInjectedTrash() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(CacheSafety::ensureOwnedRoot(tempDir.path()));
    const QString entryPath = QDir(tempDir.path()).filePath(QStringLiteral("C10000"));
    QVERIFY(QDir().mkpath(entryPath));
    QFile metadata(QDir(entryPath).filePath(QStringLiteral("component.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral(
                "{\"lcscId\":\"C10000\",\"cacheOwner\":\"EasyKiConverter\",\"cacheEntryVersion\":1}")) > 0);
    metadata.close();

    QStringList moved;
    const CacheSafety::TrashFunction trash = [&](const QString& path, QString*) {
        moved.append(path);
        return QDir(tempDir.path()).rename(QFileInfo(path).fileName(), QStringLiteral("trashed-entry"));
    };
    QString error;
    QVERIFY(CacheSafety::moveToTrash(entryPath, &error, trash));
    QCOMPARE(moved, QStringList{entryPath});
    QVERIFY(!QFileInfo::exists(entryPath));
    QVERIFY(QFileInfo::exists(QDir(tempDir.path()).filePath(QStringLiteral("trashed-entry/component.json"))));
}

// 验证回收站失败时原始缓存条目仍然保留。
void TestCacheSafety::preservesEntryWhenTrashFails() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString entryPath = QDir(tempDir.path()).filePath(QStringLiteral("owned.bin"));
    QFile file(entryPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("data") > 0);
    file.close();

    QString error;
    const CacheSafety::TrashFunction trash = [](const QString&, QString* reason) {
        if (reason)
            *reason = QStringLiteral("mock trash failure");
        return false;
    };
    QVERIFY(!CacheSafety::moveToTrash(entryPath, &error, trash));
    QCOMPARE(error, QStringLiteral("mock trash failure"));
    QVERIFY(QFileInfo::exists(entryPath));
}

// 验证未知目录和符号链接不会被枚举为可维护缓存条目。
void TestCacheSafety::ignoresUnknownAndSymlinkEntries() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(CacheSafety::ensureOwnedRoot(tempDir.path()));
    const QString ownedPath = QDir(tempDir.path()).filePath(QStringLiteral("C10001"));
    QVERIFY(QDir().mkpath(ownedPath));
    QFile metadata(QDir(ownedPath).filePath(QStringLiteral("component.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral(
                "{\"lcscId\":\"C10001\",\"cacheOwner\":\"EasyKiConverter\",\"cacheEntryVersion\":1}")) > 0);
    metadata.close();

    const QString unknownPath = QDir(tempDir.path()).filePath(QStringLiteral("user-dir"));
    QVERIFY(QDir().mkpath(unknownPath));
    const QString linkPath = QDir(tempDir.path()).filePath(QStringLiteral("C10002"));
    QVERIFY(QFile::link(ownedPath, linkPath));

    const QStringList owned = CacheSafety::ownedComponentDirectories(tempDir.path());
    QCOMPARE(owned, QStringList{ownedPath});
    QVERIFY(QFileInfo::exists(unknownPath));
    QVERIFY(QFileInfo::exists(linkPath));
}

QTEST_GUILESS_MAIN(TestCacheSafety)
#include "test_cache_safety.moc"
