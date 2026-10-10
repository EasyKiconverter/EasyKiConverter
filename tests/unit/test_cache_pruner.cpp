#include "services/CachePruner.h"
#include "services/CacheSafety.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

namespace {
/**
 * @brief 验证 canonicalTempPath 测试场景。
 * @param tempDir 待处理的文件系统路径。
 * @param size 测试数据尺寸。
 */
QString canonicalTempPath(const QTemporaryDir& tempDir) {
    return QFileInfo(tempDir.path()).canonicalFilePath();
}
}  // namespace

class TestCachePruner : public QObject {
    Q_OBJECT

private slots:
    void testModel3DExcludedFromSize();
    void testPruneDoesNotDeleteModel3D();
    void testPrunePreservesEntryWhenTrashFails();

private:
    void writeFile(const QString& path, qsizetype size);
};

// 创建测试文件，为缓存配额计算提供确定大小的输入。
/**
 * @brief 验证 writeFile 测试场景。
 * @param path 待处理的文件系统路径。
 */
void TestCachePruner::writeFile(const QString& path, qsizetype size) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArray(size, 'x')) == size);
}

// 验证配额统计只计算已验证的组件缓存，不把三维模型重复计入。
/**
 * @brief 验证 testModel3DExcludedFromSize 测试场景。
 */
void TestCachePruner::testModel3DExcludedFromSize() {
    // tempDir：测试使用的文件系统路径。
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    // tempPath：测试使用的文件系统路径。
    const QString tempPath = canonicalTempPath(tempDir);
    QVERIFY(CacheSafety::ensureOwnedRoot(tempPath));
    QVERIFY(CacheSafety::ensureOwnedModel3DDirectory(tempPath));

    QDir root(tempPath);
    QVERIFY(root.mkpath(QStringLiteral("C123")));
    QVERIFY(root.mkpath(QStringLiteral("model3d")));

    QFile metadata(root.filePath(QStringLiteral("C123/component.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral(
                "{\"lcscId\":\"C123\",\"cacheOwner\":\"EasyKiConverter\",\"cacheEntryVersion\":1}")) > 0);
    metadata.close();
    writeFile(root.filePath(QStringLiteral("model3d/model.step")), 1000);

    CachePruner pruner(tempPath);
    QCOMPARE(pruner.currentCacheSize(), QFileInfo(root.filePath(QStringLiteral("C123/component.json"))).size());
}

// 验证配额裁剪不会处理模型目录中的三维模型文件。
/**
 * @brief 验证 testPruneDoesNotDeleteModel3D 测试场景。
 */
void TestCachePruner::testPruneDoesNotDeleteModel3D() {
    // tempDir：测试使用的文件系统路径。
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    // tempPath：测试使用的文件系统路径。
    const QString tempPath = canonicalTempPath(tempDir);
    QVERIFY(CacheSafety::ensureOwnedRoot(tempPath));
    QVERIFY(CacheSafety::ensureOwnedModel3DDirectory(tempPath));

    QDir root(tempPath);
    QVERIFY(root.mkpath(QStringLiteral("C123")));
    QVERIFY(root.mkpath(QStringLiteral("model3d")));

    // componentPath：测试使用的文件系统路径。
    const QString componentPath = root.filePath(QStringLiteral("C123/component.json"));
    // modelPath：测试使用的文件系统路径。
    const QString modelPath = root.filePath(QStringLiteral("model3d/model.step"));
    QFile metadata(componentPath);
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral(
                "{\"lcscId\":\"C123\",\"cacheOwner\":\"EasyKiConverter\",\"cacheEntryVersion\":1}")) > 0);
    metadata.close();
    writeFile(modelPath, 1000);

    CachePruner pruner(tempPath);
    QCOMPARE(pruner.pruneTo(0), qint64(0));

    QVERIFY(!QFileInfo::exists(componentPath));
    QVERIFY(QFileInfo::exists(modelPath));
}

/**
 * @brief 验证回收站失败时配额裁剪不会永久删除缓存文件。
 */
void TestCachePruner::testPrunePreservesEntryWhenTrashFails() {
    // tempDir：测试使用的隔离文件系统路径。
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    // tempPath：测试使用的规范化缓存根目录。
    const QString tempPath = canonicalTempPath(tempDir);
    QVERIFY(CacheSafety::ensureOwnedRoot(tempPath));

    // root：用于创建已验证组件目录和缓存文件的目录对象。
    QDir root(tempPath);
    QVERIFY(root.mkpath(QStringLiteral("C123")));
    // metadata：组件缓存所有权元数据文件。
    QFile metadata(root.filePath(QStringLiteral("C123/component.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral(
                "{\"lcscId\":\"C123\",\"cacheOwner\":\"EasyKiConverter\",\"cacheEntryVersion\":1}")) > 0);
    metadata.close();
    // symbolPath：用于触发配额裁剪的已知缓存文件。
    const QString symbolPath = root.filePath(QStringLiteral("C123/symbol.json"));
    writeFile(symbolPath, 1000);

    // trash：始终失败的回收站替身，用于验证不会回退到永久删除。
    const CacheSafety::TrashFunction trash = [](const QString&, QString* error) {
        if (error)
            *error = QStringLiteral("模拟回收站失败");
        return false;
    };
    // pruner：使用失败回收站实现的缓存裁剪器。
    CachePruner pruner(tempPath, trash);
    const qint64 sizeBefore = pruner.currentCacheSize();

    QCOMPARE(pruner.pruneTo(0), sizeBefore);
    QVERIFY(QFileInfo::exists(symbolPath));
}

QTEST_GUILESS_MAIN(TestCachePruner)
#include "test_cache_pruner.moc"
/*
 * 我本就是一个不喜欢主动的人，虽然灵魂有趣，但不爱表达，还死倔，也慢热。
 * 遇到懂我的人是幸运，遇不到也正常。喜欢独处，三观正。
 * 习惯了任何人的忽冷忽热，也看淡了任何人的渐行渐远。
 * 我比你想象的深情，也比你以为的冷漠。
 * 有人理解我之幸，无人理解我独行
 */
