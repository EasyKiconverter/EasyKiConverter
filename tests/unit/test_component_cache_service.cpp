#include "services/CacheRepository.h"
#include "services/CacheSafety.h"
#include "services/ComponentCacheService.h"
#include "services/ComponentCacheWritePolicy.h"
#include "services/LcscImageService.h"

#include <QAtomicInt>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

class TestComponentCacheService : public QObject {
    Q_OBJECT

private slots:

    // 为每个测试准备独立的缓存目录并清理旧状态。
    /**
     * @brief 为测试用例初始化隔离环境。
     */
    void init() {
        QVERIFY(m_tempDir.isValid());
        m_cache = ComponentCacheService::instance();
        QSignalSpy warningSpy(m_cache, &ComponentCacheService::cacheMaintenanceWarning);
        // cacheDirSet：测试使用的文件系统路径。
        const bool cacheDirSet = m_cache->setCacheDir(canonicalTempPath(m_tempDir));
        const QString warning =
            warningSpy.isEmpty() ? QStringLiteral("未提供诊断") : warningSpy.constLast().at(0).toString();
        QVERIFY2(cacheDirSet, qPrintable(warning));
        m_cache->clearAllCache();
    }

    // 清理测试期间产生的一级和二级缓存。
    /**
     * @brief 清理测试用例使用的资源。
     */
    void cleanup() {
        if (m_cache != nullptr) {
            m_cache->clearAllCache();
        }
        // 测试会故意创建无法验证归属的文件；这些文件必须只在隔离的临时目录中清理。
        QVERIFY(QDir(m_tempDir.path()).removeRecursively());
        QVERIFY(QDir().mkpath(m_tempDir.path()));
    }

    // 验证预览图 URL 经过保存和读取后保持规范化结果。
    /**
     * @brief 验证 testPreviewImageUrlsRoundTrip 测试场景。
     */
    void testPreviewImageUrlsRoundTrip() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C12345");
        const QStringList previewUrls = {QStringLiteral("https://image.lceda.cn/a.jpg"),
                                         QStringLiteral("/image.lceda.cn/components/b.jpg")};
        const QStringList normalizedUrls = {QStringLiteral("https://image.lceda.cn/a.jpg"),
                                            QStringLiteral("https://image.lceda.cn/components/b.jpg")};

        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Test Component"));
        data.setPreviewImages(previewUrls);

        m_cache->saveComponentMetadata(componentId, data);

        // memoryMetadata：测试使用的文本或数据内容。
        const QJsonObject memoryMetadata = m_cache->loadMetadataFromMemory(componentId);
        QVERIFY(!memoryMetadata.isEmpty());
        // memoryPreviewUrls：内存缓存中的预览图 URL 集合。
        const QJsonArray memoryPreviewUrls = memoryMetadata.value(QStringLiteral("previewImages")).toArray();
        QCOMPARE(memoryPreviewUrls.size(), previewUrls.size());
        QCOMPARE(memoryPreviewUrls.at(0).toString(), previewUrls.at(0));
        QCOMPARE(memoryPreviewUrls.at(1).toString(), normalizedUrls.at(1));

        // loadedData：测试使用的文本或数据内容。
        QSharedPointer<ComponentData> loadedData = m_cache->loadComponentData(componentId);
        QVERIFY(loadedData != nullptr);
        QCOMPARE(loadedData->name(), QStringLiteral("Test Component"));
        QCOMPARE(loadedData->previewImages(), normalizedUrls);
    }

    // 验证异常预览图 URL 会在读取时被规范化。
    /**
     * @brief 验证 testMalformedPreviewImageUrlNormalization 测试场景。
     */
    void testMalformedPreviewImageUrlNormalization() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54321");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Malformed URL Component"));
        data.setPreviewImages({QStringLiteral("https://image.lceda.cn//image.lceda.cn/components/c.jpg")});

        m_cache->saveComponentMetadata(componentId, data);

        // loadedData：测试使用的文本或数据内容。
        QSharedPointer<ComponentData> loadedData = m_cache->loadComponentData(componentId);
        QVERIFY(loadedData != nullptr);
        QCOMPARE(loadedData->previewImages().value(0), QStringLiteral("https://image.lceda.cn/components/c.jpg"));
    }

    // 验证错误页和非图片字节不会进入预览图缓存。
    /**
     * @brief 验证 testInvalidPreviewImageDataIsRejected 测试场景。
     */
    void testInvalidPreviewImageDataIsRejected() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54322");
        m_cache->savePreviewImage(componentId, QByteArrayLiteral("<html>403</html>"), 0);
        QVERIFY(m_cache->loadPreviewImage(componentId, 0).isEmpty());
        QVERIFY(!QFileInfo::exists(m_cache->previewImagePath(componentId, 0)));
    }

    // 验证缓存写入策略同时拒绝过期代次和 tombstone 请求。
    /**
     * @brief 验证 testCacheWritePolicyRejectsStaleRequests 测试场景。
     */
    void testCacheWritePolicyRejectsStaleRequests() {
        // tombstones：缓存删除墓碑集合。
        CacheTombstoneRegistry tombstones;
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C13579");

        QVERIFY(ComponentCacheWritePolicy::isAllowed(7, 0, tombstones, componentId));
        QVERIFY(ComponentCacheWritePolicy::isAllowed(7, 7, tombstones, componentId));
        QVERIFY(!ComponentCacheWritePolicy::isAllowed(8, 7, tombstones, componentId));

        tombstones.blockComponent(componentId);
        QVERIFY(!ComponentCacheWritePolicy::isAllowed(7, 7, tombstones, componentId));
        tombstones.clearComponent(componentId);
        QVERIFY(ComponentCacheWritePolicy::isAllowed(7, 7, tombstones, componentId));
    }

    // 验证图片服务不会把损坏的缓存文件报告为可用预览图。
    /**
     * @brief 验证 testImageServiceRejectsCorruptCachedPreview 测试场景。
     */
    void testImageServiceRejectsCorruptCachedPreview() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54326");
        // metadata：测试使用的文本或数据内容。
        ComponentData metadata;
        metadata.setLcscId(componentId);
        metadata.setName(QStringLiteral("Corrupt preview fixture"));
        m_cache->saveComponentMetadata(componentId, metadata);

        // imagePath：测试使用的文件系统路径。
        const QString imagePath = m_cache->previewImagePath(componentId, 0);
        QVERIFY(QDir().mkpath(QFileInfo(imagePath).absolutePath()));
        QFile imageFile(imagePath);
        QVERIFY(imageFile.open(QIODevice::WriteOnly));
        QVERIFY(imageFile.write(QByteArrayLiteral("<html>403</html>")) > 0);
        imageFile.close();

        // imageService：被测服务实例。
        LcscImageService imageService;
        QSignalSpy errorSpy(&imageService, &LcscImageService::error);
        imageService.fetchPreviewImages(componentId);

        QVERIFY2(errorSpy.wait(3000), "Corrupt cached preview should report an error");
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).toString(), componentId);
        QCOMPARE(errorSpy.at(0).at(1).toString(), QStringLiteral("No images downloaded"));
    }

    // 验证取消全部请求后，单个新请求可以重新加载已有预览图缓存。
    /**
     * @brief 验证 testImageServiceCanRestartSingleRequestAfterCancelAll 测试场景。
     */
    void testImageServiceCanRestartSingleRequestAfterCancelAll() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54327");
        // metadata：测试使用的文本或数据内容。
        ComponentData metadata;
        metadata.setLcscId(componentId);
        metadata.setName(QStringLiteral("Restart preview fixture"));
        m_cache->saveComponentMetadata(componentId, metadata);

        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::blue);
        // buffer：测试用缓冲区。
        QBuffer buffer;
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "PNG"));
        m_cache->savePreviewImage(componentId, buffer.data(), 0);

        // imageService：被测服务实例。
        LcscImageService imageService;
        imageService.cancelAll();
        QSignalSpy readySpy(&imageService, &LcscImageService::allImagesReady);
        imageService.fetchPreviewImages(componentId);

        QVERIFY2(readySpy.wait(3000), "A restarted request should load cached preview images");
        QCOMPARE(readySpy.count(), 1);
        QCOMPARE(readySpy.at(0).at(0).toString(), componentId);

        // 预览缓存属于本用例的临时 fixture，验证异步读取后清理，避免未知状态影响后续安全性测试。
        QVERIFY(QDir(m_cache->componentCacheDir(componentId)).removeRecursively());
    }

    // 验证既不是 PDF 也不是 HTML 的响应不会进入数据手册缓存。
    /**
     * @brief 验证 testInvalidPdfDatasheetDataIsRejected 测试场景。
     */
    void testInvalidPdfDatasheetDataIsRejected() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54323");
        m_cache->saveDatasheet(componentId, QByteArrayLiteral("not-a-datasheet"), QStringLiteral("pdf"));
        QVERIFY(m_cache->loadDatasheet(componentId).isEmpty());
    }

    // 验证清空一级缓存会使清理前捕获的异步写入代次失效。
    /**
     * @brief 验证 testClearMemoryCacheInvalidatesPendingGeneration 测试场景。
     */
    void testClearMemoryCacheInvalidatesPendingGeneration() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54329");
        // oldGeneration：删除前的缓存代次。
        const uint64_t oldGeneration = m_cache->currentGeneration();
        m_cache->clearMemoryCache();
        // newGeneration：写入使用的新缓存代次。
        const uint64_t newGeneration = m_cache->currentGeneration();

        QVERIFY(newGeneration > oldGeneration);
        // staleData：测试使用的文本或数据内容。
        ComponentData staleData;
        staleData.setLcscId(componentId);
        staleData.setName(QStringLiteral("stale"));
        m_cache->saveComponentMetadata(componentId, staleData, oldGeneration);
        QVERIFY(m_cache->loadComponentData(componentId) == nullptr);
    }

    // 验证未知数据手册格式不会写入错误的 PDF 缓存路径。
    /**
     * @brief 验证 testUnsupportedDatasheetFormatIsRejected 测试场景。
     */
    void testUnsupportedDatasheetFormatIsRejected() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54324");
        // htmlData：测试使用的文本或数据内容。
        const QByteArray htmlData = QByteArrayLiteral("<html><body>unsupported</body></html>");

        m_cache->saveDatasheet(componentId, htmlData, QStringLiteral("doc"));

        QVERIFY(m_cache->loadDatasheet(componentId).isEmpty());
        QVERIFY(!QFileInfo::exists(m_tempDir.filePath(componentId + QStringLiteral("/datasheet.pdf"))));
        QVERIFY(!QFileInfo::exists(m_tempDir.filePath(componentId + QStringLiteral("/datasheet.html"))));
    }

    // 验证大小写不同的元器件编号使用同一缓存目录和内存键，并可被完整删除。
    /**
     * @brief 验证 testComponentCacheNormalizesIdCaseForRemoval 测试场景。
     */
    void testComponentCacheNormalizesIdCaseForRemoval() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("c54328");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Case normalized component"));

        m_cache->saveComponentMetadata(componentId, data);

        QVERIFY(m_cache->hasCache(componentId));
        QVERIFY(QFileInfo::exists(m_tempDir.filePath(QStringLiteral("C54328/component.json"))));
        QSignalSpy cacheSizeSpy(m_cache, &ComponentCacheService::cacheSizeChanged);
        m_cache->removeCache(componentId);

        QVERIFY(!m_cache->hasCache(componentId));
        QVERIFY(m_cache->loadComponentData(componentId) == nullptr);
        QVERIFY(!QFileInfo::exists(m_tempDir.filePath(QStringLiteral("C54328"))));
        QVERIFY(!cacheSizeSpy.isEmpty());
        QCOMPARE(cacheSizeSpy.last().at(0).toLongLong(), m_cache->getCacheSize());
    }

    // 验证缓存大小信号回调中可以安全查询当前磁盘占用。
    /**
     * @brief 验证 testCacheSizeSignalAllowsReentrantSizeQuery 测试场景。
     */
    void testCacheSizeSignalAllowsReentrantSizeQuery() {
        // callbackCompleted：回调是否已完成。
        bool callbackCompleted = false;
        // callbackSize：测试使用的数量或索引值。
        qint64 callbackSize = -1;
        const QMetaObject::Connection connection =
            connect(m_cache, &ComponentCacheService::cacheSizeChanged, this, [&]() {
                callbackSize = m_cache->getCacheSize();
                callbackCompleted = true;
            });

        m_cache->clearAllCache();

        disconnect(connection);
        QVERIFY(callbackCompleted);
        QVERIFY(callbackSize >= 0);
    }

    // 验证切换缓存目录时的内存缓存信号回调可以安全查询磁盘占用。
    /**
     * @brief 验证 testCacheDirChangeSignalAllowsReentrantSizeQuery 测试场景。
     */
    void testCacheDirChangeSignalAllowsReentrantSizeQuery() {
        // newCacheDir：测试使用的文件系统路径。
        QTemporaryDir newCacheDir;
        QVERIFY(newCacheDir.isValid());

        // callbackCompleted：回调是否已完成。
        bool callbackCompleted = false;
        // callbackSize：测试使用的数量或索引值。
        qint64 callbackSize = -1;
        const QMetaObject::Connection connection =
            connect(m_cache, &ComponentCacheService::memoryCacheSizeChanged, this, [&](qint64) {
                callbackSize = m_cache->getCacheSize();
                callbackCompleted = true;
            });

        m_cache->setCacheDir(canonicalTempPath(newCacheDir), false);

        disconnect(connection);
        QVERIFY(callbackCompleted);
        QCOMPARE(callbackSize, 0);
    }

    // 验证升级前的小写缓存目录可读取但不会因缺少所有权清单而被维护操作接管。
    /**
     * @brief 验证 testLegacyLowercaseComponentCacheRemainsAccessible 测试场景。
     */
    void testLegacyLowercaseComponentCacheRemainsAccessible() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54329");
        // legacyDir：测试使用的文件系统路径。
        const QString legacyDir = m_tempDir.filePath(QStringLiteral("c54329"));
        QVERIFY(QDir().mkpath(legacyDir));

        // metadata：测试使用的文本或数据内容。
        QJsonObject metadata;
        metadata.insert(QStringLiteral("lcscId"), componentId);
        metadata.insert(QStringLiteral("name"), QStringLiteral("Legacy component"));
        QFile metadataFile(QDir(legacyDir).filePath(QStringLiteral("component.json")));
        QVERIFY(metadataFile.open(QIODevice::WriteOnly));
        QVERIFY(metadataFile.write(QJsonDocument(metadata).toJson(QJsonDocument::Compact)) > 0);
        metadataFile.close();

        // loaded：从输出中读取的数据。
        const QSharedPointer<ComponentData> loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QStringLiteral("Legacy component"));
        QVERIFY(!m_cache->getCachedComponentIds().contains(componentId));

        m_cache->removeCache(componentId);
        QVERIFY(QFileInfo::exists(legacyDir));

        // 旧缓存目录属于本用例的临时 fixture，验证保留行为后再清理，避免污染后续用例。
        QVERIFY(QDir(legacyDir).removeRecursively());
    }

    // 验证清空缓存只处理已验证文件，组件目录中的用户文件保持不变。
    /**
     * @brief 验证 testClearCachePreservesUnknownComponentFiles 测试场景。
     */
    void testClearCachePreservesUnknownComponentFiles() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54331");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Owned component"));
        m_cache->saveComponentMetadata(componentId, data);

        const QString unknownPath =
            QDir(m_cache->componentCacheDir(componentId)).filePath(QStringLiteral("user-note.txt"));
        QFile unknown(unknownPath);
        QVERIFY(unknown.open(QIODevice::WriteOnly));
        QVERIFY(unknown.write("do not remove") > 0);
        unknown.close();

        m_cache->clearAllCache();

        QVERIFY(QFileInfo::exists(unknownPath));
        QVERIFY(QFileInfo::exists(m_cache->componentCacheDir(componentId)));

        // 该测试使用的是临时缓存根目录；保留断言完成后清理 fixture，避免影响后续用例的同名组件。
        QVERIFY(QFile::remove(unknownPath));
        QVERIFY(QDir().rmdir(m_cache->componentCacheDir(componentId)));
    }

    // 验证大小写目录并存时缓存枚举不会返回重复的元器件编号。
    /**
     * @brief 验证 testCachedComponentIdsDeduplicateCaseVariants 测试场景。
     */
    void testCachedComponentIdsDeduplicateCaseVariants() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54330");
        for (const QString& directoryName : {QStringLiteral("C54330"), QStringLiteral("c54330")}) {
            // directoryPath：测试使用的文件系统路径。
            const QString directoryPath = m_tempDir.filePath(directoryName);
            QVERIFY(QDir().mkpath(directoryPath));
            // metadata：测试使用的文本或数据内容。
            QJsonObject metadata;
            metadata.insert(QStringLiteral("lcscId"), componentId);
            metadata.insert(QStringLiteral("name"), QStringLiteral("Duplicate variant"));
            metadata.insert(QStringLiteral("cacheOwner"), QStringLiteral("EasyKiConverter"));
            metadata.insert(QStringLiteral("cacheEntryVersion"), 1);
            QFile metadataFile(QDir(directoryPath).filePath(QStringLiteral("component.json")));
            QVERIFY(metadataFile.open(QIODevice::WriteOnly));
            QVERIFY(metadataFile.write(QJsonDocument(metadata).toJson(QJsonDocument::Compact)) > 0);
            metadataFile.close();
        }

        // cachedIds：测试使用的元器件标识或名称。
        const QStringList cachedIds = m_cache->getCachedComponentIds();
        QCOMPARE(cachedIds.count(componentId), 1);

        // invalidId：测试使用的元器件标识或名称。
        const QString invalidId = QStringLiteral("C54335");
        // invalidDir：测试使用的文件系统路径。
        const QString invalidDir = m_tempDir.filePath(invalidId);
        QVERIFY(QDir().mkpath(invalidDir));
        QFile invalidMetadata(QDir(invalidDir).filePath(QStringLiteral("component.json")));
        QVERIFY(invalidMetadata.open(QIODevice::WriteOnly));
        QVERIFY(invalidMetadata.write(QByteArrayLiteral("not-json")) > 0);
        invalidMetadata.close();

        QVERIFY(!m_cache->getCachedComponentIds().contains(invalidId));
        QVERIFY(QDir(invalidDir).removeRecursively());
    }

    // 验证 CAD 缓存写入和读取都会拒绝结构损坏的 JSON。
    /**
     * @brief 验证 testInvalidCadDataIsRejected 测试场景。
     */
    void testInvalidCadDataIsRejected() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54331");
        // metadata：测试使用的文本或数据内容。
        ComponentData metadata;
        metadata.setLcscId(componentId);
        metadata.setName(QStringLiteral("CAD validation component"));
        m_cache->saveComponentMetadata(componentId, metadata);
        m_cache->saveCadDataJson(componentId, QByteArrayLiteral("{\"shape\":}"));

        QVERIFY(!m_cache->hasSymbolFootprintCache(componentId));
        QVERIFY(m_cache->loadCadDataJson(componentId).isEmpty());
        QVERIFY(!QFileInfo::exists(m_tempDir.filePath(componentId + QStringLiteral("/cad_data.json"))));

        m_cache->saveCadDataJson(componentId, QByteArrayLiteral("{\"shape\":[]}"));
        QVERIFY(m_cache->hasSymbolFootprintCache(componentId));
        QCOMPARE(m_cache->loadCadDataJson(componentId), QByteArrayLiteral("{\"shape\":[]}"));
    }

    // 验证数据手册下载不会直接返回格式无效的磁盘缓存。
    /**
     * @brief 验证 testDownloadDatasheetRemovesInvalidCachedData 测试场景。
     */
    void testDownloadDatasheetRemovesInvalidCachedData() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54325");
        // metadata：测试使用的文本或数据内容。
        ComponentData metadata;
        metadata.setLcscId(componentId);
        metadata.setName(QStringLiteral("Invalid datasheet fixture"));
        m_cache->saveComponentMetadata(componentId, metadata);

        QVERIFY(QDir().mkpath(m_tempDir.filePath(componentId)));
        // cachedPath：测试使用的文件系统路径。
        const QString cachedPath = m_tempDir.filePath(componentId + QStringLiteral("/datasheet.pdf"));
        QFile cachedFile(cachedPath);
        QVERIFY(cachedFile.open(QIODevice::WriteOnly));
        QVERIFY(cachedFile.write(QByteArrayLiteral("not-a-pdf")) > 0);
        cachedFile.close();

        QAtomicInt cancelled(1);
        // format：当前目标格式。
        QString format;
        // data：测试使用的文本或数据内容。
        const QByteArray data = m_cache->downloadDatasheet(
            componentId, QStringLiteral("https://example.com/manual.pdf"), &format, nullptr, &cancelled);

        QVERIFY(data.isEmpty());
        QVERIFY(!QFileInfo::exists(cachedPath));
    }

    // 验证 PDF URL 对应的 HTML 回退缓存可以被后续下载请求复用。
    /**
     * @brief 验证 testDownloadDatasheetUsesHtmlFallbackCache 测试场景。
     */
    void testDownloadDatasheetUsesHtmlFallbackCache() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54327");
        // htmlData：测试使用的文本或数据内容。
        const QByteArray htmlData = QByteArrayLiteral("<html><body>cached</body></html>");
        m_cache->saveDatasheet(componentId, htmlData, QStringLiteral("pdf"));

        QAtomicInt cancelled(1);
        // format：当前目标格式。
        QString format;
        // data：测试使用的文本或数据内容。
        const QByteArray data = m_cache->downloadDatasheet(
            componentId, QStringLiteral("https://example.com/manual.pdf"), &format, nullptr, &cancelled);

        QCOMPARE(data, htmlData);
        QCOMPARE(format, QStringLiteral("html"));
    }

    // 验证超出预览图范围的索引会在网络请求前被拒绝。
    /**
     * @brief 验证 testDownloadPreviewRejectsOutOfRangeIndex 测试场景。
     */
    void testDownloadPreviewRejectsOutOfRangeIndex() {
        QAtomicInt cancelled(0);

        // data：测试使用的文本或数据内容。
        const QByteArray data = m_cache->downloadPreviewImage(
            QStringLiteral("C54326"), QStringLiteral("https://example.com/preview.jpg"), 3, nullptr, &cancelled);

        QVERIFY(data.isEmpty());
    }

    // 验证异步预览图入口会在网络请求前拒绝超出范围的索引。
    /**
     * @brief 验证 testAsyncPreviewRejectsOutOfRangeIndex 测试场景。
     */
    void testAsyncPreviewRejectsOutOfRangeIndex() {
        QAtomicInt cancelled(0);
        // callbackCalled：回调是否已被调用。
        bool callbackCalled = false;
        // callbackData：测试使用的文本或数据内容。
        QByteArray callbackData;
        // callbackDiag：回调产生的诊断信息。
        ComponentExportStatus::NetworkDiagnostics callbackDiag;

        CacheRepository::instance()->fetchPreviewImageAsync(
            QStringLiteral("C54328"),
            QStringLiteral("https://example.com/preview.jpg"),
            3,
            &cancelled,
            false,
            [&](const QByteArray& data, const ComponentExportStatus::NetworkDiagnostics& diag) {
                callbackCalled = true;
                callbackData = data;
                callbackDiag = diag;
            });

        QVERIFY(callbackCalled);
        QVERIFY(callbackData.isEmpty());
        QCOMPARE(callbackDiag.errorString, QStringLiteral("Invalid preview image index"));
    }

    // 验证缓存自愈会清理非空但格式无效的媒体和三维文件。
    /**
     * @brief 验证 testCacheHealthRemovesInvalidNonEmptyFiles 测试场景。
     */
    void testCacheHealthRemovesInvalidNonEmptyFiles() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54324");
        // modelUuid：测试使用的元器件标识或名称。
        const QString modelUuid = QStringLiteral("health-invalid-model");
        // metadata：测试使用的文本或数据内容。
        ComponentData metadata;
        metadata.setLcscId(componentId);
        metadata.setName(QStringLiteral("Health Check Component"));
        m_cache->saveComponentMetadata(componentId, metadata);

        // componentDir：测试使用的文件系统路径。
        const QString componentDir = m_tempDir.filePath(componentId);
        QFile previewFile(QDir(componentDir).filePath(QStringLiteral("preview_0.jpg")));
        QVERIFY(previewFile.open(QIODevice::WriteOnly));
        QVERIFY(previewFile.write(QByteArrayLiteral("<html>403</html>")) > 0);
        previewFile.close();

        QFile datasheetFile(QDir(componentDir).filePath(QStringLiteral("datasheet.pdf")));
        QVERIFY(datasheetFile.open(QIODevice::WriteOnly));
        QVERIFY(datasheetFile.write(QByteArrayLiteral("not-a-pdf")) > 0);
        datasheetFile.close();

        QFile cadFile(QDir(componentDir).filePath(QStringLiteral("cad_data.json")));
        QVERIFY(cadFile.open(QIODevice::WriteOnly));
        QVERIFY(cadFile.write(QByteArrayLiteral("{\"cad\":}")) > 0);
        cadFile.close();

        // invalidModelPath：测试使用的文件系统路径。
        const QString invalidModelPath = m_tempDir.filePath(QStringLiteral("model3d/health-invalid-model.obj"));
        QFile invalidModelFile(invalidModelPath);
        QVERIFY(QDir().mkpath(QFileInfo(invalidModelPath).absolutePath()));
        QVERIFY(invalidModelFile.open(QIODevice::WriteOnly));
        QVERIFY(invalidModelFile.write(QByteArrayLiteral("not-an-obj")) > 0);
        invalidModelFile.close();
        QVERIFY(QFileInfo::exists(invalidModelPath));

        m_cache->setCacheDir(canonicalTempPath(m_tempDir), false);

        QVERIFY(!QFileInfo::exists(previewFile.fileName()));
        QVERIFY(!QFileInfo::exists(datasheetFile.fileName()));
        QVERIFY(!QFileInfo::exists(cadFile.fileName()));
        QVERIFY(m_cache->loadModel3D(modelUuid, QStringLiteral("obj")).isEmpty());
    }

    // 验证读取三维缓存时会拒绝并删除结构无效的模型文件。
    /**
     * @brief 验证 testLoadModel3DRejectsInvalidContent 测试场景。
     */
    void testLoadModel3DRejectsInvalidContent() {
        // modelUuid：测试使用的元器件标识或名称。
        const QString modelUuid = QStringLiteral("load-invalid-model");
        // invalidModelPath：测试使用的文件系统路径。
        const QString invalidModelPath = m_tempDir.filePath(QStringLiteral("model3d/load-invalid-model.obj"));
        QFile invalidModelFile(invalidModelPath);
        QVERIFY(QDir().mkpath(QFileInfo(invalidModelPath).absolutePath()));
        QVERIFY(invalidModelFile.open(QIODevice::WriteOnly));
        QVERIFY(invalidModelFile.write(QByteArrayLiteral("not-an-obj")) > 0);
        invalidModelFile.close();

        QVERIFY(m_cache->loadModel3D(modelUuid, QStringLiteral("obj")).isEmpty());
        QVERIFY(!QFileInfo::exists(m_tempDir.filePath(QStringLiteral("model3d/load-invalid-model.obj"))));
    }

    // 验证仅由封装数据携带的 3D 模型也能完整写入并恢复缓存元数据。
    /**
     * @brief 验证 testFootprintModel3DMetadataRoundTrip 测试场景。
     */
    void testFootprintModel3DMetadataRoundTrip() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54321");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Footprint Model Component"));

        // footprint：测试用封装数据。
        auto footprint = QSharedPointer<FootprintData>::create();
        // model：测试使用的配置值或状态。
        Model3DData model;
        model.setUuid(QStringLiteral("footprint-only-model"));
        model.setName(QStringLiteral("FOOTPRINT_ONLY"));
        model.setTranslation(Model3DBase(1.0, 2.0, 3.0));
        model.setRotation(Model3DBase(4.0, 5.0, 6.0));
        footprint->setModel3D(model);
        data.setFootprintData(footprint);

        m_cache->saveComponentMetadata(componentId, data, 0, true);

        // metadata：测试使用的文本或数据内容。
        const QJsonObject metadata = m_cache->loadMetadataFromMemory(componentId);
        QCOMPARE(metadata.value(QStringLiteral("model3duuid")).toString(), QStringLiteral("footprint-only-model"));
        // loadedData：测试使用的文本或数据内容。
        const QSharedPointer<ComponentData> loadedData = m_cache->loadComponentData(componentId);
        QVERIFY(loadedData != nullptr);
        QVERIFY(loadedData->model3DData() != nullptr);
        QCOMPARE(loadedData->model3DData()->uuid(), QStringLiteral("footprint-only-model"));
        QCOMPARE(loadedData->model3DData()->translation().z, 3.0);
        QCOMPARE(loadedData->model3DData()->rotation().x, 4.0);
    }

    // 回归测试：removeCache tombstone 阻止旧写入，但 clearTombstone 后允许新写入
    /**
     * @brief 验证 testRemoveCacheTombstoneBlocksStaleWrites 测试场景。
     */
    void testRemoveCacheTombstoneBlocksStaleWrites() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C99999");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Tombstoned Component"));

        // 先正常写入
        m_cache->saveComponentMetadata(componentId, data);
        // loaded：从输出中读取的数据。
        QSharedPointer<ComponentData> loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QStringLiteral("Tombstoned Component"));

        // generationBeforeRemove：删除前记录的缓存代次。
        const uint64_t generationBeforeRemove = m_cache->currentGeneration();

        // removeCache 会设置 tombstone
        m_cache->removeCache(componentId);
        QVERIFY(m_cache->currentGeneration() > generationBeforeRemove);
        loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded == nullptr);

        // 当前 generation 的写入在 tombstone 保留期间也应被阻止
        const uint64_t deletionGeneration = m_cache->currentGeneration();
        data.setName(QStringLiteral("Blocked By Tombstone"));
        m_cache->saveComponentMetadata(componentId, data, deletionGeneration);
        loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded == nullptr);

        // 删除前 generation 的写入即使解除 tombstone 也应被阻止
        const uint64_t staleGen = generationBeforeRemove;
        data.setName(QStringLiteral("Should Not Write"));
        m_cache->clearTombstone(componentId);
        m_cache->saveComponentMetadata(componentId, data, staleGen);
        loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded == nullptr);  // generation 不匹配阻止了旧写入

        // clearTombstone 后，新写入应成功
        data.setName(QStringLiteral("Should Write Now"));
        m_cache->saveComponentMetadata(componentId, data);
        loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QStringLiteral("Should Write Now"));
    }

    // 验证无效元器件编号不会污染 tombstone 状态。
    /**
     * @brief 验证 testRemoveCacheIgnoresInvalidComponentId 测试场景。
     */
    void testRemoveCacheIgnoresInvalidComponentId() {
        // invalidComponentId：测试使用的元器件标识或名称。
        const QString invalidComponentId = QStringLiteral("not-a-component");

        m_cache->clearGlobalTombstone();
        m_cache->removeCache(invalidComponentId);

        QVERIFY(!m_cache->isTombstoned(invalidComponentId));
    }

    // 验证解除全局 tombstone 时不会误删单组件 tombstone。
    /**
     * @brief 验证 testClearGlobalTombstonePreservesComponentTombstone 测试场景。
     */
    void testClearGlobalTombstonePreservesComponentTombstone() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C88887");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Component Tombstone"));

        m_cache->saveComponentMetadata(componentId, data);
        m_cache->removeCache(componentId);
        QVERIFY(m_cache->isTombstoned(componentId));

        m_cache->clearGlobalTombstone();

        QVERIFY(m_cache->isTombstoned(componentId));
        m_cache->clearTombstone(componentId);
        QVERIFY(!m_cache->isTombstoned(componentId));
    }

    // 回归测试：clearAllCache 全局 tombstone 阻止所有写入，clearGlobalTombstone 解除
    /**
     * @brief 验证 testGlobalTombstoneBlocksAllWrites 测试场景。
     */
    void testGlobalTombstoneBlocksAllWrites() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C88888");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Global Tombstone Test"));

        // clearAllCache 设置全局 tombstone
        m_cache->clearAllCache();

        // 带 generation 的写入应被阻止
        const uint64_t gen = m_cache->currentGeneration();
        m_cache->saveComponentMetadata(componentId, data, gen);
        // loaded：从输出中读取的数据。
        QSharedPointer<ComponentData> loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded == nullptr);  // 全局 tombstone 阻止了写入

        // clearGlobalTombstone 后，新写入应成功
        m_cache->clearGlobalTombstone();
        m_cache->saveComponentMetadata(componentId, data);
        loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QStringLiteral("Global Tombstone Test"));
    }

    // 验证清空全部缓存时会保留无法证明归属的根目录文件。
    /**
     * @brief 验证 testClearAllCacheRemovesRootFiles 测试场景。
     */
    void testClearAllCacheRemovesRootFiles() {
        // rootFilePath：测试使用的文件系统路径。
        const QString rootFilePath = QDir(m_cache->cacheDir()).filePath(QStringLiteral("legacy-cache.json"));
        QFile rootFile(rootFilePath);
        QVERIFY(rootFile.open(QIODevice::WriteOnly));
        QVERIFY(rootFile.write(QByteArrayLiteral("legacy")) > 0);
        rootFile.close();
        QVERIFY(QFileInfo::exists(rootFilePath));

        m_cache->clearAllCache();

        QVERIFY(QFileInfo::exists(rootFilePath));
        QVERIFY(QFile::remove(rootFilePath));
    }

    // 回归测试：generation 不匹配时写入被丢弃
    /**
     * @brief 验证 testStaleGenerationBlocksWrite 测试场景。
     */
    void testStaleGenerationBlocksWrite() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C77777");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Stale Gen Test"));

        // 捕获当前 generation
        const uint64_t oldGen = m_cache->currentGeneration();

        // clearAllCache 递增 generation
        m_cache->clearAllCache();
        m_cache->clearGlobalTombstone();  // 解除全局 tombstone

        // 用旧 generation 写入应被丢弃
        m_cache->saveComponentMetadata(componentId, data, oldGen);
        // loaded：从输出中读取的数据。
        QSharedPointer<ComponentData> loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded == nullptr);  // generation 不匹配，写入被丢弃

        // 用新 generation 写入应成功
        const uint64_t newGen = m_cache->currentGeneration();
        m_cache->saveComponentMetadata(componentId, data, newGen);
        loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QStringLiteral("Stale Gen Test"));
    }

    // 验证缓存目录迁移后元数据和预览图仍可读取。
    /**
     * @brief 验证 testCacheDirMigrationPreservesExistingCache 测试场景。
     */
    void testCacheDirMigrationPreservesExistingCache() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C24680");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Migrated Component"));

        m_cache->saveComponentMetadata(componentId, data);
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        // imageBuffer：读取到的图像字节缓冲区。
        QBuffer imageBuffer;
        QVERIFY(imageBuffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&imageBuffer, "PNG"));
        // previewData：测试使用的文本或数据内容。
        const QByteArray previewData = imageBuffer.data();
        m_cache->savePreviewImage(componentId, previewData, 0);

        // newCacheDir：测试使用的文件系统路径。
        QTemporaryDir newCacheDir;
        QVERIFY(newCacheDir.isValid());

        m_cache->setCacheDir(canonicalTempPath(newCacheDir), /*migrateExistingCache=*/true);

        // loadedData：测试使用的文本或数据内容。
        QSharedPointer<ComponentData> loadedData = m_cache->loadComponentData(componentId);
        QVERIFY(loadedData != nullptr);
        QCOMPARE(loadedData->name(), QStringLiteral("Migrated Component"));
        QCOMPARE(m_cache->loadPreviewImage(componentId, 0), previewData);
    }

    // 验证没有新版本根标记的旧缓存仍可迁移，并在目标目录补齐所有权信息。
    /**
     * @brief 验证 testLegacyCacheDirMigrationPreservesExistingCache 测试场景。
     */
    void testLegacyCacheDirMigrationPreservesExistingCache() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C24682");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Legacy migrated component"));
        m_cache->saveComponentMetadata(componentId, data);
        // legacyMetadataPath：等价旧版本缓存的元数据路径。
        const QString legacyMetadataPath =
            QDir(m_cache->componentCacheDir(componentId)).filePath(QStringLiteral("component.json"));
        QFile legacyMetadata(legacyMetadataPath);
        QVERIFY(legacyMetadata.open(QIODevice::ReadOnly | QIODevice::Text));
        const QJsonObject legacyMetadataObject =
            QJsonDocument::fromJson(legacyMetadata.readAll()).object();  // 旧版本元数据对象。
        legacyMetadata.close();
        QJsonObject oldLayoutMetadata = legacyMetadataObject;  // 去除当前版本所有权字段的旧版元数据。
        oldLayoutMetadata.remove(QStringLiteral("cacheOwner"));
        oldLayoutMetadata.remove(QStringLiteral("cacheEntryVersion"));
        QVERIFY(legacyMetadata.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        QVERIFY(legacyMetadata.write(QJsonDocument(oldLayoutMetadata).toJson(QJsonDocument::Compact)) > 0);
        legacyMetadata.close();
        QVERIFY(QFile::remove(QDir(m_cache->cacheDir()).filePath(CacheSafety::ownershipMarkerName())));

        // newCacheDir：测试使用的文件系统路径。
        QTemporaryDir newCacheDir;
        QVERIFY(newCacheDir.isValid());
        QVERIFY(m_cache->setCacheDir(canonicalTempPath(newCacheDir), /*migrateExistingCache=*/true));

        // loadedData：测试使用的文本或数据内容。
        const QSharedPointer<ComponentData> loadedData = m_cache->loadComponentData(componentId);
        QVERIFY(loadedData != nullptr);
        QCOMPARE(loadedData->name(), QStringLiteral("Legacy migrated component"));
        QVERIFY(CacheSafety::isOwnedRoot(canonicalTempPath(newCacheDir)));

        // 清理该迁移场景之前由同一测试 fixture 产生的旧预览目录，避免无标记根目录被误判为用户目录。
        QVERIFY(QDir(m_tempDir.filePath(QStringLiteral("C54327"))).removeRecursively());
    }

    // 回归测试：迁移发现目标冲突时不应先搬走其他组件的源文件。
    /**
     * @brief 验证 testCacheDirMigrationConflictPreservesSourceEntries 测试场景。
     */
    void testCacheDirMigrationConflictPreservesSourceEntries() {
        // firstComponentId：测试使用的元器件标识或名称。
        const QString firstComponentId = QStringLiteral("C24683");
        // conflictingComponentId：测试使用的元器件标识或名称。
        const QString conflictingComponentId = QStringLiteral("C24684");
        for (const QString& componentId : {firstComponentId, conflictingComponentId}) {
            // data：测试使用的文本或数据内容。
            ComponentData data;
            data.setLcscId(componentId);
            data.setName(QStringLiteral("Conflict migration fixture"));
            m_cache->saveComponentMetadata(componentId, data);
        }

        // targetCacheDir：测试使用的文件系统路径。
        QTemporaryDir targetCacheDir;
        QVERIFY(targetCacheDir.isValid());
        // ownershipError：测试期间收集的错误或诊断信息。
        QString ownershipError;
        QVERIFY2(CacheSafety::ensureOwnedRoot(canonicalTempPath(targetCacheDir), &ownershipError),
                 qPrintable(ownershipError));

        // targetComponentDir：测试使用的文件系统路径。
        const QString targetComponentDir = targetCacheDir.filePath(conflictingComponentId);
        QVERIFY(QDir().mkpath(targetComponentDir));
        QFile targetMetadata(QDir(targetComponentDir).filePath(QStringLiteral("component.json")));
        QVERIFY(targetMetadata.open(QIODevice::WriteOnly | QIODevice::Text));
        const QJsonObject metadata{{QStringLiteral("lcscId"), conflictingComponentId},
                                   {QStringLiteral("cacheOwner"), QStringLiteral("EasyKiConverter")},
                                   {QStringLiteral("cacheEntryVersion"), 1}};
        QVERIFY(targetMetadata.write(QJsonDocument(metadata).toJson(QJsonDocument::Compact)) > 0);
        targetMetadata.close();

        QVERIFY(!m_cache->setCacheDir(canonicalTempPath(targetCacheDir), true));
        QCOMPARE(QFileInfo(m_cache->cacheDir()).canonicalFilePath(), QFileInfo(m_tempDir.path()).canonicalFilePath());
        QVERIFY(QFileInfo::exists(m_tempDir.filePath(firstComponentId + QStringLiteral("/component.json"))));
        QVERIFY(QFileInfo::exists(m_tempDir.filePath(conflictingComponentId + QStringLiteral("/component.json"))));
        QVERIFY(QFileInfo::exists(targetMetadata.fileName()));
    }

    // 验证缓存枚举和磁盘大小统计在目录迁移前后保持一致。
    /**
     * @brief 验证 testCacheEnumerationAndSizeAfterMigration 测试场景。
     */
    void testCacheEnumerationAndSizeAfterMigration() {
        // firstComponentId：测试使用的元器件标识或名称。
        const QString firstComponentId = QStringLiteral("C24681");
        // secondComponentId：测试使用的元器件标识或名称。
        const QString secondComponentId = QStringLiteral("C24682");

        for (const QString& componentId : {firstComponentId, secondComponentId}) {
            // data：测试使用的文本或数据内容。
            ComponentData data;
            data.setLcscId(componentId);
            data.setName(QStringLiteral("Enumerated Component %1").arg(componentId));
            m_cache->saveComponentMetadata(componentId, data);
            m_cache->saveCadDataJson(componentId, QByteArrayLiteral("{\"shape\":[]}"));
        }

        // cachedIdsBeforeMigration：测试使用的元器件标识或名称。
        const QStringList cachedIdsBeforeMigration = m_cache->getCachedComponentIds();
        QVERIFY(cachedIdsBeforeMigration.contains(firstComponentId));
        QVERIFY(cachedIdsBeforeMigration.contains(secondComponentId));
        // sizeBeforeMigration：测试使用的数量或索引值。
        const qint64 sizeBeforeMigration = m_cache->getCacheSize();
        QVERIFY(sizeBeforeMigration > 0);

        // newCacheDir：测试使用的文件系统路径。
        QTemporaryDir newCacheDir;
        QVERIFY(newCacheDir.isValid());
        m_cache->setCacheDir(canonicalTempPath(newCacheDir), /*migrateExistingCache=*/true);

        // cachedIdsAfterMigration：测试使用的元器件标识或名称。
        const QStringList cachedIdsAfterMigration = m_cache->getCachedComponentIds();
        QVERIFY(cachedIdsAfterMigration.contains(firstComponentId));
        QVERIFY(cachedIdsAfterMigration.contains(secondComponentId));
        QVERIFY(m_cache->getCacheSize() > 0);
    }

    // 验证缓存目录迁移后三维模型文件仍可读取。
    /**
     * @brief 验证 testCacheDirMigrationPreservesObjModelCache 测试场景。
     */
    void testCacheDirMigrationPreservesObjModelCache() {
        // uuid：测试使用的元器件标识或名称。
        const QString uuid = QStringLiteral("migrated-model-13579");
        // objData：测试使用的文本或数据内容。
        const QByteArray objData = QByteArrayLiteral("v 0 0 0\nf 1 2 3");
        m_cache->saveModel3D(uuid, objData, QStringLiteral("obj"));

        // newCacheDir：测试使用的文件系统路径。
        QTemporaryDir newCacheDir;
        QVERIFY(newCacheDir.isValid());
        m_cache->setCacheDir(canonicalTempPath(newCacheDir), /*migrateExistingCache=*/true);

        QVERIFY(m_cache->hasModel3DCached(uuid, QStringLiteral("obj")));
        QCOMPARE(m_cache->loadModel3D(uuid, QStringLiteral("obj")), objData);
    }

    // 验证切换缓存目录会使原一级缓存失效。
    /**
     * @brief 验证 testCacheDirChangeInvalidatesL1Cache 测试场景。
     */
    void testCacheDirChangeInvalidatesL1Cache() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C11223");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Old Directory Component"));
        m_cache->saveComponentMetadata(componentId, data);
        m_cache->saveSymbolData(componentId, QByteArrayLiteral("{\"symbol\":{}}"));
        QVERIFY(m_cache->hasInMemoryCache(componentId));

        // newCacheDir：测试使用的文件系统路径。
        QTemporaryDir newCacheDir;
        QVERIFY(newCacheDir.isValid());
        m_cache->setCacheDir(canonicalTempPath(newCacheDir));

        QVERIFY(!m_cache->hasInMemoryCache(componentId));
        QCOMPARE(m_cache->getMemoryCacheSize(), qint64(0));
        QVERIFY(m_cache->loadSymbolData(componentId).isEmpty());
        QVERIFY(m_cache->loadComponentData(componentId) == nullptr);
    }

    // 验证符号和封装缓存拒绝损坏内容并清理已有无效文件。
    /**
     * @brief 验证 testInvalidSymbolAndFootprintDataIsRejected 测试场景。
     */
    void testInvalidSymbolAndFootprintDataIsRejected() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54332");
        // metadata：测试使用的文本或数据内容。
        ComponentData metadata;
        metadata.setLcscId(componentId);
        metadata.setName(QStringLiteral("Symbol footprint validation component"));
        m_cache->saveComponentMetadata(componentId, metadata);

        // componentDir：测试使用的文件系统路径。
        const QString componentDir = m_cache->componentCacheDir(componentId);
        QFile symbolFile(QDir(componentDir).filePath(QStringLiteral("symbol.json")));
        QVERIFY(symbolFile.open(QIODevice::WriteOnly));
        QVERIFY(symbolFile.write(QByteArrayLiteral("{\"symbol\":}")) > 0);
        symbolFile.close();
        QFile footprintFile(QDir(componentDir).filePath(QStringLiteral("footprint.json")));
        QVERIFY(footprintFile.open(QIODevice::WriteOnly));
        QVERIFY(footprintFile.write(QByteArrayLiteral("not-json")) > 0);
        footprintFile.close();

        QVERIFY(m_cache->loadSymbolData(componentId).isEmpty());
        QVERIFY(m_cache->loadFootprintData(componentId).isEmpty());
        QVERIFY(!QFileInfo::exists(symbolFile.fileName()));
        QVERIFY(!QFileInfo::exists(footprintFile.fileName()));

        m_cache->saveSymbolData(componentId, QByteArrayLiteral("{\"symbol\":{}}"));
        m_cache->saveFootprintData(componentId, QByteArrayLiteral("{\"footprint\":{}}"));
        QVERIFY(!m_cache->loadSymbolData(componentId).isEmpty());
        QVERIFY(!m_cache->loadFootprintData(componentId).isEmpty());

        // memoryOnlyId：测试使用的元器件标识或名称。
        const QString memoryOnlyId = QStringLiteral("C54333");
        m_cache->saveSymbolDataToMemory(memoryOnlyId, QByteArrayLiteral("invalid-symbol"));
        m_cache->saveFootprintDataToMemory(memoryOnlyId, QByteArrayLiteral("invalid-footprint"));
        QVERIFY(m_cache->loadSymbolDataFromMemory(memoryOnlyId).isEmpty());
        QVERIFY(m_cache->loadFootprintDataFromMemory(memoryOnlyId).isEmpty());
    }

    // 验证损坏的 CAD 原始缓存不会被完整缓存检查误判为命中。
    /**
     * @brief 验证 testHasCacheRejectsInvalidCadData 测试场景。
     */
    void testHasCacheRejectsInvalidCadData() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54334");
        // metadata：测试使用的文本或数据内容。
        ComponentData metadata;
        metadata.setLcscId(componentId);
        metadata.setName(QStringLiteral("Invalid CAD cache component"));
        m_cache->saveComponentMetadata(componentId, metadata);

        // metadataPath：测试使用的文件系统路径。
        const QString metadataPath = QDir(m_cache->componentCacheDir(componentId)).filePath("component.json");
        QFile metadataFile(metadataPath);
        QVERIFY(metadataFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(metadataFile.write(QByteArrayLiteral("{\"cachedAt\":\"test\"}")) > 0);
        metadataFile.close();

        // cadPath：测试使用的文件系统路径。
        const QString cadPath = QDir(m_cache->componentCacheDir(componentId)).filePath("cad_data.json");
        QFile cadFile(cadPath);
        QVERIFY(cadFile.open(QIODevice::WriteOnly));
        QVERIFY(cadFile.write(QByteArrayLiteral("{\"cad\":}")) > 0);
        cadFile.close();

        QVERIFY(!m_cache->hasCache(componentId));
    }

    // 验证异常三维元数据会使对应缓存失效并触发自愈。
    /**
     * @brief 验证 testMalformedModel3DMetadataInvalidatesCache 测试场景。
     */
    void testMalformedModel3DMetadataInvalidatesCache() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C13579");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Malformed 3D Component"));
        m_cache->saveComponentMetadata(componentId, data);

        // metadataPath：测试使用的文件系统路径。
        const QString metadataPath = QDir(m_cache->componentCacheDir(componentId)).filePath("component.json");
        QFile metadataFile(metadataPath);
        QVERIFY(metadataFile.open(QIODevice::ReadOnly));
        // parseError：测试期间收集的错误或诊断信息。
        QJsonParseError parseError;
        // document：测试用文档对象。
        QJsonDocument document = QJsonDocument::fromJson(metadataFile.readAll(), &parseError);
        metadataFile.close();
        QCOMPARE(parseError.error, QJsonParseError::NoError);

        // metadata：测试使用的文本或数据内容。
        QJsonObject metadata = document.object();
        metadata.insert(QStringLiteral("model3duuid"), QStringLiteral("uuid-13579"));
        // malformedTranslation：构造的非法三维变换。
        QJsonObject malformedTranslation;
        malformedTranslation.insert(QStringLiteral("x"), QStringLiteral("invalid"));
        malformedTranslation.insert(QStringLiteral("y"), 0.0);
        malformedTranslation.insert(QStringLiteral("z"), 0.0);
        metadata.insert(QStringLiteral("model3dTranslation"), malformedTranslation);

        QVERIFY(metadataFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(metadataFile.write(QJsonDocument(metadata).toJson()) > 0);
        metadataFile.close();

        // 自愈流程应将无法验证的数据保留，避免误删用户内容。
        m_cache->setCacheDir(canonicalTempPath(m_tempDir));
        QVERIFY(QFileInfo::exists(metadataPath));
        QVERIFY(!m_cache->hasCache(componentId));
        QVERIFY(m_cache->loadComponentData(componentId) == nullptr);
    }

    // 验证 STEP、OBJ 和 WRL 三种模型文件均可往返磁盘缓存。
    /**
     * @brief 验证 testModel3DArtifactsRoundTripThroughDiskCache 测试场景。
     */
    void testModel3DArtifactsRoundTripThroughDiskCache() {
        // uuid：测试使用的元器件标识或名称。
        const QString uuid = QStringLiteral("model-cache-13579");
        const QList<QPair<QString, QByteArray>> artifacts = {
            {QStringLiteral("step"), QByteArray("ISO-10303-21; cached step END-ISO-10303-21;")},
            {QStringLiteral("obj"), QByteArray("v 0 0 0\nf 1 2 3")},
            {QStringLiteral("wrl"),
             QByteArray("#VRML V2.0 utf8\nShape { geometry IndexedFaceSet { coord Coordinate { point [0 0 0, 1 0 0, "
                        "0 1 0] } coordIndex [0, 1, 2, -1] } }")},
        };

        for (const auto& artifact : artifacts) {
            m_cache->saveModel3D(uuid, artifact.second, artifact.first);
            QVERIFY(m_cache->hasModel3DCached(uuid, artifact.first));
            QCOMPARE(m_cache->loadModel3D(uuid, artifact.first), artifact.second);
        }
    }

    // 验证复制三维模型时拒绝空缓存文件。
    /**
     * @brief 验证 testCopyModel3DRejectsEmptyCacheFile 测试场景。
     */
    void testCopyModel3DRejectsEmptyCacheFile() {
        // uuid：测试使用的元器件标识或名称。
        const QString uuid = QStringLiteral("empty-model-13579");
        // modelPath：测试使用的文件系统路径。
        const QString modelPath = QDir(m_cache->cacheDir()).filePath(QStringLiteral("model3d/%1.step").arg(uuid));
        QVERIFY(QDir().mkpath(QFileInfo(modelPath).absolutePath()));
        QFile modelFile(modelPath);
        QVERIFY(modelFile.open(QIODevice::WriteOnly));
        modelFile.close();

        // outputDir：测试使用的文件系统路径。
        QTemporaryDir outputDir;
        QVERIFY(outputDir.isValid());
        // destinationPath：测试使用的文件系统路径。
        const QString destinationPath = QDir(outputDir.path()).filePath(QStringLiteral("model.step"));
        QVERIFY(!m_cache->copyModel3DToFile(uuid, QStringLiteral("step"), destinationPath));
        QVERIFY(!QFileInfo::exists(destinationPath));
    }

    // 验证三维缓存命中和复制接口都会拒绝结构无效的模型文件。
    /**
     * @brief 验证 testModel3DCacheInterfacesRejectInvalidContent 测试场景。
     */
    void testModel3DCacheInterfacesRejectInvalidContent() {
        // uuid：测试使用的元器件标识或名称。
        const QString uuid = QStringLiteral("invalid-interface-model-13579");
        // modelPath：测试使用的文件系统路径。
        const QString modelPath = m_tempDir.filePath(QStringLiteral("model3d/invalid-interface-model-13579.obj"));
        QFile modelFile(modelPath);
        QVERIFY(QDir().mkpath(QFileInfo(modelPath).absolutePath()));
        QVERIFY(modelFile.open(QIODevice::WriteOnly));
        QVERIFY(modelFile.write(QByteArrayLiteral("not-an-obj")) > 0);
        modelFile.close();
        QVERIFY(!m_cache->hasModel3DCached(uuid, QStringLiteral("obj")));

        QVERIFY(modelFile.open(QIODevice::WriteOnly));
        QVERIFY(modelFile.write(QByteArrayLiteral("not-an-obj")) > 0);
        modelFile.close();
        // outputDir：测试使用的文件系统路径。
        QTemporaryDir outputDir;
        QVERIFY(outputDir.isValid());
        // destinationPath：测试使用的文件系统路径。
        const QString destinationPath = QDir(outputDir.path()).filePath(QStringLiteral("model.obj"));
        QVERIFY(!m_cache->copyModel3DToFile(uuid, QStringLiteral("obj"), destinationPath));
        QVERIFY(!QFileInfo::exists(destinationPath));
    }

    // 验证三维缓存不会接受项目未支持的文件扩展名。
    /**
     * @brief 验证 testModel3DRejectsUnsupportedExtension 测试场景。
     */
    void testModel3DRejectsUnsupportedExtension() {
        // uuid：测试使用的元器件标识或名称。
        const QString uuid = QStringLiteral("unsupported-extension-model-13579");
        // objData：测试使用的文本或数据内容。
        const QByteArray objData = QByteArrayLiteral("v 0 0 0\nf 1 2 3");

        m_cache->saveModel3D(uuid, objData, QStringLiteral("txt"));

        QVERIFY(!m_cache->hasModel3DCached(uuid, QStringLiteral("txt")));
    }

    // 验证三维缓存扩展名大小写不同仍能命中同一个缓存文件。
    /**
     * @brief 验证 testModel3DNormalizesExtensionCase 测试场景。
     */
    void testModel3DNormalizesExtensionCase() {
        // uuid：测试使用的元器件标识或名称。
        const QString uuid = QStringLiteral("case-normalized-model-13579");
        const QByteArray stepData = QByteArrayLiteral("ISO-10303-21;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n");

        m_cache->saveModel3D(uuid, stepData, QStringLiteral("STEP"));

        QVERIFY(m_cache->hasModel3DCached(uuid, QStringLiteral("step")));
        QCOMPARE(m_cache->loadModel3D(uuid, QStringLiteral("step")), stepData);
    }

    // 验证权威 CAD 元数据移除模型后会清理旧三维关联。
    /**
     * @brief 验证 testAuthoritativeCadMetadataClearsRemovedModel3D 测试场景。
     */
    void testAuthoritativeCadMetadataClearsRemovedModel3D() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C24681");
        // withModel：测试使用的配置值或状态。
        ComponentData withModel;
        withModel.setLcscId(componentId);
        withModel.setName(QStringLiteral("With model"));
        // model：测试使用的配置值或状态。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(QStringLiteral("model-to-remove"));
        withModel.setModel3DData(model);
        m_cache->saveComponentMetadata(componentId, withModel);

        // withoutModel：测试使用的配置值或状态。
        ComponentData withoutModel;
        withoutModel.setLcscId(componentId);
        withoutModel.setName(QStringLiteral("Without model"));
        m_cache->saveComponentMetadata(componentId, withoutModel, 0, /*replaceModel3DMetadata=*/true);

        // loaded：从输出中读取的数据。
        const QSharedPointer<ComponentData> loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QVERIFY(!loaded->model3DData());
        QCOMPARE(loaded->name(), QStringLiteral("Without model"));
    }

private:
    // m_tempDir：测试使用的文件系统路径。
    QTemporaryDir m_tempDir;
    // m_cache：测试使用的缓存或元器件对象。
    ComponentCacheService* m_cache = nullptr;
};

QTEST_GUILESS_MAIN(TestComponentCacheService)
#include "test_component_cache_service.moc"
