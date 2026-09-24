#include "services/CachePruner.h"
#include "services/CacheSafety.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestCachePruner : public QObject {
    Q_OBJECT

private slots:
    void testModel3DExcludedFromSize();
    void testPruneDoesNotDeleteModel3D();

private:
    void writeFile(const QString& path, qsizetype size);
};

// 创建测试文件，为缓存配额计算提供确定大小的输入。
void TestCachePruner::writeFile(const QString& path, qsizetype size) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArray(size, 'x')) == size);
}

// 验证配额统计只计算已验证的组件缓存，不把三维模型重复计入。
void TestCachePruner::testModel3DExcludedFromSize() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(CacheSafety::ensureOwnedRoot(tempDir.path()));
    QVERIFY(CacheSafety::ensureOwnedModel3DDirectory(tempDir.path()));

    QDir root(tempDir.path());
    QVERIFY(root.mkpath(QStringLiteral("C123")));
    QVERIFY(root.mkpath(QStringLiteral("model3d")));

    QFile metadata(root.filePath(QStringLiteral("C123/component.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral(
                "{\"lcscId\":\"C123\",\"cacheOwner\":\"EasyKiConverter\",\"cacheEntryVersion\":1}")) > 0);
    metadata.close();
    writeFile(root.filePath(QStringLiteral("model3d/model.step")), 1000);

    CachePruner pruner(tempDir.path());
    QCOMPARE(pruner.currentCacheSize(), QFileInfo(root.filePath(QStringLiteral("C123/component.json"))).size());
}

// 验证配额裁剪不会处理模型目录中的三维模型文件。
void TestCachePruner::testPruneDoesNotDeleteModel3D() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(CacheSafety::ensureOwnedRoot(tempDir.path()));
    QVERIFY(CacheSafety::ensureOwnedModel3DDirectory(tempDir.path()));

    QDir root(tempDir.path());
    QVERIFY(root.mkpath(QStringLiteral("C123")));
    QVERIFY(root.mkpath(QStringLiteral("model3d")));

    const QString componentPath = root.filePath(QStringLiteral("C123/component.json"));
    const QString modelPath = root.filePath(QStringLiteral("model3d/model.step"));
    QFile metadata(componentPath);
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    QVERIFY(metadata.write(QByteArrayLiteral(
                "{\"lcscId\":\"C123\",\"cacheOwner\":\"EasyKiConverter\",\"cacheEntryVersion\":1}")) > 0);
    metadata.close();
    writeFile(modelPath, 1000);

    CachePruner pruner(tempDir.path());
    QCOMPARE(pruner.pruneTo(0), qint64(0));

    QVERIFY(!QFileInfo::exists(componentPath));
    QVERIFY(QFileInfo::exists(modelPath));
}

QTEST_GUILESS_MAIN(TestCachePruner)
#include "test_cache_pruner.moc"
