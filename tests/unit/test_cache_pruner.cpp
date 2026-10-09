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

QTEST_GUILESS_MAIN(TestCachePruner)
#include "test_cache_pruner.moc"
