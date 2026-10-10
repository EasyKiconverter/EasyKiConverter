#include "services/ComponentDataMemoryStore.h"
#include "services/ComponentService.h"
#include "services/LcscProductParser.h"
#include "services/PreviewImageDataEncoder.h"
#include "tests/common/TestPaths.hpp"

#include <QBuffer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestComponentService : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 初始化独立的临时缓存目录。
     */
    void init() {
        QVERIFY(m_tempDir.isValid());
        m_cache = ComponentCacheService::instance();
        m_cache->setCacheDir(m_tempDir.path());
        m_cache->clearAllCache();
    }

    /**
     * @brief 清理测试产生的缓存数据。
     */
    void cleanup() {
        if (m_cache != nullptr) {
            m_cache->clearAllCache();
        }
    }

    /**
     * @brief 验证失效的获取状态会在新请求时重新初始化。
     */
    void testInactiveStaleFetchingEntryIsReinitialized() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C12345");
        saveCorruptSymbolFootprintCache(componentId);

        // service：被测服务实例。
        ComponentService service;
        service.testSetFetchingState(componentId, QString(), false, false);

        service.fetchComponentData(componentId, false);

        QVERIFY(service.testHasFetchingState(componentId));
        QCOMPARE(service.testFetchingComponentId(componentId), componentId);
        QVERIFY(service.testFetchingRequestActive(componentId));
        QVERIFY(!service.testFetchingHasCadData(componentId));

        service.cancelAllPendingRequests();
    }

    /**
     * @brief 验证活动中的重复请求不会被重复调度。
     */
    void testActiveDuplicateRequestIsStillSkipped() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C23456");

        // service：被测服务实例。
        ComponentService service;
        service.testSetFetchingState(componentId, QStringLiteral("original-active-entry"), false, true);

        service.fetchComponentData(componentId, false);

        QVERIFY(service.testHasFetchingState(componentId));
        QCOMPARE(service.testFetchingComponentId(componentId), QStringLiteral("original-active-entry"));
        QVERIFY(service.testFetchingRequestActive(componentId));
    }

    /**
     * @brief 验证内存缓存读写会统一元器件编号大小写。
     */
    void testComponentMemoryCacheNormalizesIdCase() {
        // storedId：测试使用的元器件标识或名称。
        const QString storedId = QStringLiteral("c54330");
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(storedId);
        data.setName(QStringLiteral("Case normalized component"));

        // service：被测服务实例。
        ComponentService service;
        service.updateComponentCache(storedId, data);

        // loadedData：测试使用的文本或数据内容。
        const ComponentData loadedData = service.getComponentData(QStringLiteral("C54330"));
        QCOMPARE(loadedData.name(), QStringLiteral("Case normalized component"));
    }

    /**
     * @brief 验证独立内存存储可以原子更新符号和封装描述。
     */
    void testComponentMemoryStoreUpdatesDescriptions() {
        // store：测试用缓存存储。
        ComponentDataMemoryStore store;
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(QStringLiteral("C54331"));

        // symbol：测试用符号数据。
        const auto symbol = QSharedPointer<SymbolData>::create();
        // symbolInfo：测试用符号数据。
        SymbolInfo symbolInfo = symbol->info();
        symbolInfo.description = QStringLiteral("旧符号描述");
        symbol->setInfo(symbolInfo);
        data.setSymbolData(symbol);

        // footprint：测试用封装数据。
        const auto footprint = QSharedPointer<FootprintData>::create();
        // footprintInfo：测试用封装数据。
        FootprintInfo footprintInfo = footprint->info();
        footprintInfo.description = QStringLiteral("旧封装描述");
        footprint->setInfo(footprintInfo);
        data.setFootprintData(footprint);

        store.set(QStringLiteral("C54331"), data);
        QVERIFY(store.updateDescription(QStringLiteral("C54331"), QStringLiteral("新描述")));

        QCOMPARE(store.value(QStringLiteral("C54331")).symbolData()->info().description, QStringLiteral("新描述"));
        QCOMPARE(store.value(QStringLiteral("C54331")).footprintData()->info().description, QStringLiteral("新描述"));
        QVERIFY(!store.updateDescription(QStringLiteral("C99999"), QStringLiteral("不存在")));
    }

    /**
     * @brief 验证没有活动请求时不会转发过期的图片回调。
     */
    void testImageCallbackWithoutActiveRequestIsDiscarded() {
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::red);
        // buffer：测试用缓冲区。
        QBuffer buffer;
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "PNG"));

        // service：被测服务实例。
        ComponentService service;
        QSignalSpy imageSpy(&service, &ComponentService::previewImageReady);
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "handleImageReady",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("c54333")),
                                          Q_ARG(QByteArray, buffer.data()),
                                          Q_ARG(int, 0)));

        QCOMPARE(imageSpy.count(), 0);
    }

    /**
     * @brief 验证没有活动请求时不会转发过期的组件基础信息回调。
     */
    void testComponentInfoCallbackWithoutActiveRequestIsDiscarded() {
        // service：被测服务实例。
        ComponentService service;
        QSignalSpy infoSpy(&service, &ComponentService::componentInfoReady);
        // data：测试使用的文本或数据内容。
        QJsonObject data;
        data.insert(QStringLiteral("result"), QJsonObject{{QStringLiteral("title"), QStringLiteral("Stale data")}});

        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "handleComponentInfoFetched",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("c54335")),
                                          Q_ARG(QJsonObject, data)));

        QCOMPARE(infoSpy.count(), 0);
    }

    /**
     * @brief 验证缓存代次变化后不会转发过期的预览图错误。
     */
    void testPreviewImageErrorFromStaleGenerationIsDiscarded() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54336");
        // service：被测服务实例。
        ComponentService service;
        service.testSetFetchingState(componentId, componentId, false, true);
        QSignalSpy errorSpy(&service, &ComponentService::previewImageFailed);

        m_cache->clearMemoryCache();

        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "handlePreviewImageError",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, componentId),
                                          Q_ARG(QString, QStringLiteral("Image blocked (403), please retry"))));

        QCOMPARE(errorSpy.count(), 0);
    }

    /**
     * @brief 验证并行上下文不会重复计算同一元器件的完成回调。
     */
    void testParallelContextDeduplicatesFinishedComponents() {
        // context：测试使用的文本或数据内容。
        ParallelFetchContext context;
        QSignalSpy completedSpy(&context, &ParallelFetchContext::allCompleted);
        // data：测试使用的文本或数据内容。
        ComponentData data;

        context.start(1);
        context.markCompleted(QStringLiteral("c54334"), data);
        context.markCompleted(QStringLiteral("C54334"), data);

        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(context.completedCount(), 1);
        QCOMPARE(context.collectedData().size(), 1);
    }

    /**
     * @brief 验证预览图编码器按文件名序号保存原始数据和 Base64 数据。
     */
    void testPreviewImageDataEncoderPreservesImageIndexes() {
        // imageDir：测试使用的文件系统路径。
        QTemporaryDir imageDir;
        QVERIFY(imageDir.isValid());

        // firstPath：测试使用的文件系统路径。
        const QString firstPath = imageDir.filePath(QStringLiteral("preview_0.jpg"));
        // thirdPath：测试使用的文件系统路径。
        const QString thirdPath = imageDir.filePath(QStringLiteral("preview_2.jpg"));
        {
            QFile firstFile(firstPath);
            QVERIFY(firstFile.open(QIODevice::WriteOnly));
            QVERIFY(firstFile.write("first") == 5);
            QFile thirdFile(thirdPath);
            QVERIFY(thirdFile.open(QIODevice::WriteOnly));
            QVERIFY(thirdFile.write("third") == 5);
        }

        // result：测试得到的实际结果数据。
        const PreviewImageDataResult result = PreviewImageDataEncoder::encodeFiles(
            {thirdPath, imageDir.filePath(QStringLiteral("ignored.png")), firstPath});

        QCOMPARE(PreviewImageDataEncoder::imageIndexFromPath(firstPath), 0);
        QCOMPARE(PreviewImageDataEncoder::imageIndexFromPath(QStringLiteral("preview_invalid.jpg")), -1);
        QCOMPARE(result.imageData.size(), 3);
        QCOMPARE(result.imageData.at(0), QByteArray("first"));
        QCOMPARE(result.imageData.at(1), QByteArray());
        QCOMPARE(result.imageData.at(2), QByteArray("third"));
        QCOMPARE(result.encodedImages.at(0), QString::fromLatin1(QByteArray("first").toBase64()));
        QCOMPARE(result.encodedImages.at(2), QString::fromLatin1(QByteArray("third").toBase64()));
    }

    /**
     * @brief 验证 LCSC 产品解析器只选择精确匹配并限制预览图数量。
     */
    void testLcscProductParserSelectsExactProduct() {
        const QByteArray response = R"({
            "result": {
                "productList": [
                    {"component_code": "C99999", "image": "wrong.png"},
                    {"component_code": "c54337", "image": "a.png<$>b.png<$>c.png<$>extra.png",
                     "device_info": {"attributes": {"Manufacturer Part": "MP-54337", "Datasheet": "data.pdf"}}}
                ]
            }
        })";

        // product：测试用产品数据。
        const auto product = LcscProductParser::parse(QStringLiteral("C54337"), response);
        QVERIFY(product.has_value());
        QCOMPARE(product->manufacturerPart, QStringLiteral("MP-54337"));
        QCOMPARE(product->datasheetUrl, QStringLiteral("data.pdf"));
        QCOMPARE(product->imageUrls,
                 QStringList({QStringLiteral("https://image.lceda.cn/a.png"),
                              QStringLiteral("https://image.lceda.cn/b.png"),
                              QStringLiteral("https://image.lceda.cn/c.png")}));
    }

    /**
     * @brief 验证没有精确匹配产品时不会错误回退到搜索结果第一项。
     */
    void testLcscProductParserRejectsNonMatchingProduct() {
        // response：模拟网络响应数据。
        const QByteArray response = R"({"result":{"productList":[{"component_code":"C99999","image":"wrong.png"}]}})";

        QVERIFY(!LcscProductParser::parse(QStringLiteral("C54338"), response).has_value());
    }

    /**
     * @brief 验证无效 JSON 响应会被产品解析器拒绝。
     */
    void testLcscProductParserRejectsInvalidJson() {
        QVERIFY(!LcscProductParser::parse(QStringLiteral("C54339"), QByteArray("not-json")).has_value());
    }

    /**
     * @brief 验证取消缓存加载后不会重新创建获取状态。
     */
    void testCancelledCacheLoadDoesNotRecreateFetchingEntry() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C34567");
        saveCorruptSymbolFootprintCache(componentId);

        // service：被测服务实例。
        ComponentService service;
        service.fetchComponentData(componentId, false);
        service.cancelRequestForComponent(componentId);

        QTest::qWait(200);

        QVERIFY(!service.testHasFetchingState(componentId));
    }

    /**
     * @brief 验证缓存 CAD 封装中的模型 UUID 会在 OBJ 预加载前恢复。
     */
    void testCachedFootprintModelRestoresBeforeObjPreload() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C23186");
        // modelUuid：测试使用的元器件标识或名称。
        const QString modelUuid = QStringLiteral("6bd5cd867e9542ebae21caaf5d2d4c4d");
        // error：测试期间收集的错误或诊断信息。
        QString error;
        const QByteArray cadData =
            Test::TestPaths::readBytes(Test::TestPaths::fixturePath(QStringLiteral("easyeda/cad_basic.json")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        // objData：测试使用的文本或数据内容。
        const QByteArray objData = Test::TestPaths::readBytes(
            Test::TestPaths::fixturePath(QStringLiteral("easyeda/model3d_r0603.obj")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        // metadata：测试使用的文本或数据内容。
        ComponentData metadata;
        metadata.setLcscId(componentId);
        metadata.setName(QStringLiteral("Cached R0603"));
        m_cache->saveComponentMetadata(componentId, metadata);
        m_cache->saveCadDataJson(componentId, cadData);
        m_cache->saveModel3D(modelUuid, objData, QStringLiteral("obj"));

        // receivedData：测试使用的文本或数据内容。
        ComponentData receivedData;
        // received：收到的测试响应。
        bool received = false;
        // service：被测服务实例。
        ComponentService service;
        connect(&service, &ComponentService::cadDataReady, this, [&](const QString& id, const ComponentData& data) {
            if (id == componentId) {
                receivedData = data;
                received = true;
            }
        });

        service.fetchComponentData(componentId, true);

        QTRY_VERIFY_WITH_TIMEOUT(received, 3000);
        QVERIFY(receivedData.model3DData() != nullptr);
        QCOMPARE(receivedData.model3DData()->uuid(), modelUuid);
        QCOMPARE(receivedData.model3DObjRaw(), objData);
    }

    /**
     * @brief 验证空批量请求会立即发出完成信号。
     */
    void testEmptyBatchFetchCompletesImmediately() {
        // service：被测服务实例。
        ComponentService service;
        QSignalSpy completedSpy(&service, &ComponentService::allComponentsDataCollectedWithErrors);

        service.fetchMultipleComponentsData({}, false);

        QCOMPARE(completedSpy.count(), 1);
        // arguments：测试用参数列表。
        const QList<QVariant> arguments = completedSpy.at(0);
        QCOMPARE(arguments.size(), 2);
        // collectedData：测试使用的文本或数据内容。
        const QList<ComponentData> collectedData = qvariant_cast<QList<ComponentData>>(arguments.at(0));
        // failedComponents：测试使用的缓存或元器件对象。
        const QMap<QString, QString> failedComponents = qvariant_cast<QMap<QString, QString>>(arguments.at(1));
        QCOMPARE(collectedData.size(), 0);
        QVERIFY(failedComponents.isEmpty());
    }

    /**
     * @brief 验证清空缓存后新请求可以恢复缓存写入。
     */
    void testNewRequestReleasesGlobalCacheTombstone() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54331");
        m_cache->clearAllCache();

        // service：被测服务实例。
        ComponentService service;
        service.fetchComponentData(componentId, false);

        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("New request component"));
        m_cache->saveComponentMetadata(componentId, data, m_cache->currentGeneration());

        QVERIFY(m_cache->hasCache(componentId));
        service.cancelAllPendingRequests();
    }

    /**
     * @brief 验证旧代次失败回调不会清理新请求状态。
     */
    void testStaleGenerationDoesNotClearNewRequest() {
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C54332");
        // service：被测服务实例。
        ComponentService service;
        service.testSetFetchingState(componentId, componentId, false, true);

        // currentGeneration：当前缓存代次。
        const uint64_t currentGeneration = m_cache->currentGeneration();
        QVERIFY(currentGeneration > 0);
        service.testEmitFetchErrorForGeneration(componentId, QStringLiteral("stale error"), currentGeneration - 1);

        QVERIFY(service.testHasFetchingState(componentId));
        service.cancelAllPendingRequests();
    }

private:
    /**
         * @brief 写入用于触发缓存加载失败回退的最小缓存。
         * @param componentId 元器件编号。
         */
    void saveCorruptSymbolFootprintCache(const QString& componentId) {
        // data：测试使用的文本或数据内容。
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Cached Component"));
        m_cache->saveComponentMetadata(componentId, data);
        m_cache->saveCadDataJson(componentId, QByteArrayLiteral("{}"));
        QVERIFY(m_cache->hasSymbolFootprintCache(componentId));
    }

    // m_tempDir：测试使用的文件系统路径。
    QTemporaryDir m_tempDir;
    // m_cache：测试使用的缓存或元器件对象。
    ComponentCacheService* m_cache = nullptr;
};

QTEST_GUILESS_MAIN(TestComponentService)
#include "test_component_service.moc"
