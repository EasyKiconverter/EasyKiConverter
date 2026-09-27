#include "services/CacheSafety.h"
#include "services/ConfigService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

namespace {
QString canonicalTempPath(const QTemporaryDir& tempDir) {
    return QFileInfo(tempDir.path()).canonicalFilePath();
}
}  // namespace

class TestCacheSafety : public QObject {
    Q_OBJECT

private slots:
    void rejectsNonEmptyUnownedDirectory();
    void rejectsHomeDirectory();
    void acceptsEmptyHomeChildDirectory();
    void rejectsSymlinkParentDirectory();
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
    const QString tempPath = canonicalTempPath(tempDir);
    const QString filePath = QDir(tempPath).filePath(QStringLiteral("user-data.txt"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("user data") > 0);
    file.close();

    QString normalized;
    QString error;
    QVERIFY(!CacheSafety::validateSelection(tempPath, &normalized, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(QFileInfo::exists(filePath));
}

// 验证用户主目录会被安全边界明确拒绝。
void TestCacheSafety::rejectsHomeDirectory() {
    QString normalized;
    QString error;
    QVERIFY(!CacheSafety::validateSelection(QDir::homePath(), &normalized, &error));
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!CacheSafety::validateSelection(QDir::rootPath(), &normalized, &error));
    QVERIFY(!error.isEmpty());
}

// 验证用户主目录下的空子目录可以被选择并在接管后建立所有权标记。
void TestCacheSafety::acceptsEmptyHomeChildDirectory() {
    const QString homeChild =
        QDir(QDir::homePath())
            .filePath(QStringLiteral(".easykiconverter-cache-test-%1").arg(QCoreApplication::applicationPid()));
    QDir(homeChild).removeRecursively();
    QVERIFY(QDir().mkpath(homeChild));

    QString normalized;
    QString error;
    QVERIFY2(CacheSafety::validateSelection(homeChild, &normalized, &error), qPrintable(error));
    QVERIFY2(CacheSafety::ensureOwnedRoot(homeChild, &error), qPrintable(error));
    QVERIFY(CacheSafety::isOwnedRoot(homeChild));

    QVERIFY(QDir(homeChild).removeRecursively());
}

// 验证父级符号链接不能把缓存目录绕过真实路径安全边界。
void TestCacheSafety::rejectsSymlinkParentDirectory() {
    QTemporaryDir targetDir;
    QTemporaryDir linkParent;
    QVERIFY(targetDir.isValid());
    QVERIFY(linkParent.isValid());

    const QString linkPath = QDir(linkParent.path()).filePath(QStringLiteral("linked-cache-parent"));
    if (!QFile::link(targetDir.path(), linkPath))
        QSKIP("当前平台不支持创建目录符号链接");
    if (!QFileInfo(linkPath).isDir() || !QFileInfo(linkPath).isSymLink())
        QSKIP("当前平台未创建 Qt 可识别的目录符号链接");

    QString normalized;
    QString error;
    const QString candidate = QDir(linkPath).filePath(QStringLiteral("child"));
    QVERIFY(!CacheSafety::validateSelection(candidate, &normalized, &error));
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
    const QString tempPath = canonicalTempPath(tempDir);
    const QString componentPath = QDir(tempPath).filePath(QStringLiteral("C10003"));
    QVERIFY(QDir().mkpath(componentPath));
    QFile metadata(QDir(componentPath).filePath(QStringLiteral("component.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral("{\"lcscId\":\"C10003\"}")) > 0);
    metadata.close();

    const QString modelPath = QDir(tempPath).filePath(QStringLiteral("model3d/model.step"));
    QVERIFY(QDir().mkpath(QFileInfo(modelPath).absolutePath()));
    QFile model(modelPath);
    QVERIFY(model.open(QIODevice::WriteOnly));
    QVERIFY(model.write("step") > 0);
    model.close();

    QString error;
    QVERIFY(CacheSafety::canAdoptLegacyRoot(tempPath));
    QVERIFY(CacheSafety::ensureOwnedRoot(tempPath, &error));
    QVERIFY2(CacheSafety::ensureOwnedModel3DDirectory(tempPath, &error), qPrintable(error));
    QVERIFY(CacheSafety::isOwnedRoot(tempPath));
    QVERIFY(CacheSafety::isOwnedComponentDirectory(tempPath, componentPath));
    QVERIFY(CacheSafety::isOwnedModel3DFile(tempPath, modelPath));
}

// 验证已托管缓存条目通过可注入回收站移动，并且原路径消失。
void TestCacheSafety::movesOwnedEntryThroughInjectedTrash() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString tempPath = canonicalTempPath(tempDir);
    QVERIFY(CacheSafety::ensureOwnedRoot(tempPath));
    const QString entryPath = QDir(tempPath).filePath(QStringLiteral("C10000"));
    QVERIFY(QDir().mkpath(entryPath));
    QFile metadata(QDir(entryPath).filePath(QStringLiteral("component.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral(
                "{\"lcscId\":\"C10000\",\"cacheOwner\":\"EasyKiConverter\",\"cacheEntryVersion\":1}")) > 0);
    metadata.close();

    QStringList moved;
    const CacheSafety::TrashFunction trash = [&](const QString& path, QString*) {
        moved.append(path);
        return QDir(tempPath).rename(QFileInfo(path).fileName(), QStringLiteral("trashed-entry"));
    };
    QString error;
    QVERIFY(CacheSafety::moveToTrash(entryPath, &error, trash));
    QCOMPARE(moved, QStringList{entryPath});
    QVERIFY(!QFileInfo::exists(entryPath));
    QVERIFY(QFileInfo::exists(QDir(tempPath).filePath(QStringLiteral("trashed-entry/component.json"))));
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
    const QString tempPath = canonicalTempPath(tempDir);
    QVERIFY(CacheSafety::ensureOwnedRoot(tempPath));
    const QString ownedPath = QDir(tempPath).filePath(QStringLiteral("C10001"));
    QVERIFY(QDir().mkpath(ownedPath));
    QFile metadata(QDir(ownedPath).filePath(QStringLiteral("component.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral(
                "{\"lcscId\":\"C10001\",\"cacheOwner\":\"EasyKiConverter\",\"cacheEntryVersion\":1}")) > 0);
    metadata.close();

    const QString unknownPath = QDir(tempPath).filePath(QStringLiteral("user-dir"));
    QVERIFY(QDir().mkpath(unknownPath));
    const QString linkPath = QDir(tempPath).filePath(QStringLiteral("C10002"));
    QVERIFY(QFile::link(ownedPath, linkPath));

    const QStringList owned = CacheSafety::ownedComponentDirectories(tempPath);
    QCOMPARE(owned, QStringList{ownedPath});
    QVERIFY(QFileInfo::exists(unknownPath));
    QVERIFY(QFileInfo::exists(linkPath));
}

QTEST_GUILESS_MAIN(TestCacheSafety)
#include "test_cache_safety.moc"
