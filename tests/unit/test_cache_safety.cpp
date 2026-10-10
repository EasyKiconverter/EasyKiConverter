#include "services/CacheSafety.h"
#include "services/ConfigService.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

namespace {
/**
 * @brief 验证 canonicalTempPath 测试场景。
 * @param tempDir 待处理的文件系统路径。
 */
QString canonicalTempPath(const QTemporaryDir& tempDir) {
    return QFileInfo(tempDir.path()).canonicalFilePath();
}
}  // namespace

class TestCacheSafety : public QObject {
    Q_OBJECT

private slots:
    void rejectsNonEmptyUnownedDirectory();
    void rejectsHomeDirectory();
    void acceptsEmptyCustomChildDirectory();
    void rejectsSymlinkParentDirectory();
    void acceptsApplicationDefaultCachePath();
    void adoptsRecognizableLegacyCacheRoot();
    void rejectsSymlinkedModel3DDirectory();
    void movesOwnedEntryThroughInjectedTrash();
    void preservesEntryWhenTrashFails();
    void ignoresUnknownAndSymlinkEntries();
};

// 验证非空且未托管的目录不能被选作缓存目录。
/**
 * @brief 验证 rejectsNonEmptyUnownedDirectory 测试场景。
 */
void TestCacheSafety::rejectsNonEmptyUnownedDirectory() {
    // tempDir：测试使用的文件系统路径。
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    // tempPath：测试使用的文件系统路径。
    const QString tempPath = canonicalTempPath(tempDir);
    // filePath：测试使用的文件系统路径。
    const QString filePath = QDir(tempPath).filePath(QStringLiteral("user-data.txt"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("user data") > 0);
    file.close();

    // normalized：规范化后的测试数据。
    QString normalized;
    // error：测试期间收集的错误或诊断信息。
    QString error;
    QVERIFY(!CacheSafety::validateSelection(tempPath, &normalized, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(QFileInfo::exists(filePath));
}

// 验证用户主目录会被安全边界明确拒绝。
/**
 * @brief 验证 rejectsHomeDirectory 测试场景。
 */
void TestCacheSafety::rejectsHomeDirectory() {
    // normalized：规范化后的测试数据。
    QString normalized;
    // error：测试期间收集的错误或诊断信息。
    QString error;
    QVERIFY(!CacheSafety::validateSelection(QDir::homePath(), &normalized, &error));
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!CacheSafety::validateSelection(QDir::rootPath(), &normalized, &error));
    QVERIFY(!error.isEmpty());
}

// 验证隔离临时目录下的空子目录可以被选择并在接管后建立所有权标记。
/**
 * @brief 验证 acceptsEmptyCustomChildDirectory 测试场景。
 */
void TestCacheSafety::acceptsEmptyCustomChildDirectory() {
    // tempDir：隔离的临时文件系统根目录。
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    // tempPath：去除 macOS 临时目录别名后的真实临时根目录。
    const QString tempPath = canonicalTempPath(tempDir);
    // customChild：临时根目录下的空自定义缓存子目录。
    const QString customChild = QDir(tempPath).filePath(QStringLiteral("custom-cache"));
    QVERIFY(QDir().mkpath(customChild));

    // normalized：规范化后的测试数据。
    QString normalized;
    // error：测试期间收集的错误或诊断信息。
    QString error;
    QVERIFY2(CacheSafety::validateSelection(customChild, &normalized, &error), qPrintable(error));
    QVERIFY2(CacheSafety::ensureOwnedRoot(customChild, &error), qPrintable(error));
    QVERIFY(CacheSafety::isOwnedRoot(customChild));
}

// 验证父级符号链接不能把缓存目录绕过真实路径安全边界。
/**
 * @brief 验证 rejectsSymlinkParentDirectory 测试场景。
 */
void TestCacheSafety::rejectsSymlinkParentDirectory() {
    // targetDir：测试使用的文件系统路径。
    QTemporaryDir targetDir;
    // linkParent：用于创建目录符号链接的父目录。
    QTemporaryDir linkParent;
    QVERIFY(targetDir.isValid());
    QVERIFY(linkParent.isValid());

    // linkPath：测试使用的文件系统路径。
    const QString linkPath = QDir(linkParent.path()).filePath(QStringLiteral("linked-cache-parent"));
    if (!QFile::link(targetDir.path(), linkPath))
        QSKIP("当前平台不支持创建目录符号链接");
    if (!QFileInfo(linkPath).isDir() || !QFileInfo(linkPath).isSymLink())
        QSKIP("当前平台未创建 Qt 可识别的目录符号链接");

    // normalized：规范化后的测试数据。
    QString normalized;
    // error：测试期间收集的错误或诊断信息。
    QString error;
    // candidate：测试使用的元器件标识或名称。
    const QString candidate = QDir(linkPath).filePath(QStringLiteral("child"));
    QVERIFY(!CacheSafety::validateSelection(candidate, &normalized, &error));
    QVERIFY(!error.isEmpty());
}

// 验证应用默认缓存目录虽位于应用数据目录下，但不会被用户主目录保护规则误拒绝。
/**
 * @brief 验证 acceptsApplicationDefaultCachePath 测试场景。
 */
void TestCacheSafety::acceptsApplicationDefaultCachePath() {
    // normalized：规范化后的测试数据。
    QString normalized;
    // error：测试期间收集的错误或诊断信息。
    QString error;
    QVERIFY2(CacheSafety::validateSelection(ConfigService::defaultCacheDir(), &normalized, &error), qPrintable(error));
}

// 验证旧版本可识别缓存根目录能够安全补齐当前所有权标记。
/**
 * @brief 验证 adoptsRecognizableLegacyCacheRoot 测试场景。
 */
void TestCacheSafety::adoptsRecognizableLegacyCacheRoot() {
    // tempDir：测试使用的文件系统路径。
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    // tempPath：测试使用的文件系统路径。
    const QString tempPath = canonicalTempPath(tempDir);
    // componentPath：测试使用的文件系统路径。
    const QString componentPath = QDir(tempPath).filePath(QStringLiteral("C10003"));
    QVERIFY(QDir().mkpath(componentPath));
    QFile metadata(QDir(componentPath).filePath(QStringLiteral("component.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral("{\"lcscId\":\"C10003\"}")) > 0);
    metadata.close();

    // modelPath：测试使用的文件系统路径。
    const QString modelPath = QDir(tempPath).filePath(QStringLiteral("model3d/model.step"));
    QVERIFY(QDir().mkpath(QFileInfo(modelPath).absolutePath()));
    QFile model(modelPath);
    QVERIFY(model.open(QIODevice::WriteOnly));
    QVERIFY(model.write("step") > 0);
    model.close();

    // error：测试期间收集的错误或诊断信息。
    QString error;
    QVERIFY(CacheSafety::canAdoptLegacyRoot(tempPath));
    QVERIFY(CacheSafety::ensureOwnedRoot(tempPath, &error));
    QVERIFY2(CacheSafety::ensureOwnedModel3DDirectory(tempPath, &error), qPrintable(error));
    QVERIFY(CacheSafety::isOwnedRoot(tempPath));
    QVERIFY(CacheSafety::isOwnedComponentDirectory(tempPath, componentPath));
    QVERIFY(CacheSafety::isOwnedModel3DFile(tempPath, modelPath));
}

// 验证模型目录符号链接不会被标记或枚举为应用拥有的缓存目录。
/**
 * @brief 验证 rejectsSymlinkedModel3DDirectory 测试场景。
 */
void TestCacheSafety::rejectsSymlinkedModel3DDirectory() {
    // rootDir：应用缓存根目录。
    QTemporaryDir rootDir;
    // externalDir：符号链接指向的外部目录。
    QTemporaryDir externalDir;
    QVERIFY(rootDir.isValid());
    QVERIFY(externalDir.isValid());
    const QString rootPath = canonicalTempPath(rootDir);  // 规范化后的应用缓存根目录。
    const QString modelPath = QDir(rootPath).filePath(QStringLiteral("model3d"));  // 符号链接模型目录路径。
    QVERIFY(CacheSafety::ensureOwnedRoot(rootPath));
    if (!QFile::link(externalDir.path(), modelPath))
        QSKIP("当前平台不支持创建目录符号链接");
    if (!QFileInfo(modelPath).isSymLink())
        QSKIP("当前平台未创建 Qt 可识别的目录符号链接");

    // error：测试期间收集的错误或诊断信息。
    QString error;
    QVERIFY(!CacheSafety::ensureOwnedModel3DDirectory(rootPath, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(QDir(externalDir.path()).filePath(CacheSafety::model3DMarkerName())));
}

// 验证已托管缓存条目通过可注入回收站移动，并且原路径消失。
/**
 * @brief 验证 movesOwnedEntryThroughInjectedTrash 测试场景。
 */
void TestCacheSafety::movesOwnedEntryThroughInjectedTrash() {
    // tempDir：测试使用的文件系统路径。
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    // tempPath：测试使用的文件系统路径。
    const QString tempPath = canonicalTempPath(tempDir);
    QVERIFY(CacheSafety::ensureOwnedRoot(tempPath));
    // entryPath：测试使用的文件系统路径。
    const QString entryPath = QDir(tempPath).filePath(QStringLiteral("C10000"));
    QVERIFY(QDir().mkpath(entryPath));
    QFile metadata(QDir(entryPath).filePath(QStringLiteral("component.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral(
                "{\"lcscId\":\"C10000\",\"cacheOwner\":\"EasyKiConverter\",\"cacheEntryVersion\":1}")) > 0);
    metadata.close();

    // moved：回收站 mock 记录的已移动路径列表。
    QStringList moved;
    const CacheSafety::TrashFunction trash = [&](const QString& path, QString*) {
        moved.append(path);
        return QDir(tempPath).rename(QFileInfo(path).fileName(), QStringLiteral("trashed-entry"));
    };
    // error：测试期间收集的错误或诊断信息。
    QString error;
    QVERIFY(CacheSafety::moveToTrash(entryPath, &error, trash));
    QCOMPARE(moved, QStringList{entryPath});
    QVERIFY(!QFileInfo::exists(entryPath));
    QVERIFY(QFileInfo::exists(QDir(tempPath).filePath(QStringLiteral("trashed-entry/component.json"))));
}

// 验证回收站失败时原始缓存条目仍然保留。
/**
 * @brief 验证 preservesEntryWhenTrashFails 测试场景。
 */
void TestCacheSafety::preservesEntryWhenTrashFails() {
    // tempDir：测试使用的文件系统路径。
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    // entryPath：测试使用的文件系统路径。
    const QString entryPath = QDir(tempDir.path()).filePath(QStringLiteral("owned.bin"));
    QFile file(entryPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("data") > 0);
    file.close();

    // error：测试期间收集的错误或诊断信息。
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
/**
 * @brief 验证 ignoresUnknownAndSymlinkEntries 测试场景。
 */
void TestCacheSafety::ignoresUnknownAndSymlinkEntries() {
    // tempDir：测试使用的文件系统路径。
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    // tempPath：测试使用的文件系统路径。
    const QString tempPath = canonicalTempPath(tempDir);
    QVERIFY(CacheSafety::ensureOwnedRoot(tempPath));
    // ownedPath：测试使用的文件系统路径。
    const QString ownedPath = QDir(tempPath).filePath(QStringLiteral("C10001"));
    QVERIFY(QDir().mkpath(ownedPath));
    QFile metadata(QDir(ownedPath).filePath(QStringLiteral("component.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral(
                "{\"lcscId\":\"C10001\",\"cacheOwner\":\"EasyKiConverter\",\"cacheEntryVersion\":1}")) > 0);
    metadata.close();

    // unknownPath：测试使用的文件系统路径。
    const QString unknownPath = QDir(tempPath).filePath(QStringLiteral("user-dir"));
    QVERIFY(QDir().mkpath(unknownPath));
    // linkPath：测试使用的文件系统路径。
    const QString linkPath = QDir(tempPath).filePath(QStringLiteral("C10002"));
    QVERIFY(QFile::link(ownedPath, linkPath));

    // owned：已通过所有权校验的缓存条目路径。
    const QStringList owned = CacheSafety::ownedComponentDirectories(tempPath);
    QCOMPARE(owned, QStringList{ownedPath});
    QVERIFY(QFileInfo::exists(unknownPath));
    QVERIFY(QFileInfo::exists(linkPath));
}

QTEST_GUILESS_MAIN(TestCacheSafety)
#include "test_cache_safety.moc"
