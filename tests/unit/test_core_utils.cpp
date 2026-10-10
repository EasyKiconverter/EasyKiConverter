#include "core/utils/GzipUtils.h"
#include "core/utils/KiCadTableUtils.h"
#include "core/utils/LayerMapper.h"
#include "core/utils/UrlUtils.h"
#include "utils/FileUtils.h"

#include <QByteArray>
#include <QDir>
#include <QTest>
#include <QUrl>

#include <cmath>

using namespace EasyKiConverter;

class TestCoreUtils : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 验证 normalizePreviewImageUrls 测试场景。
     */
    void normalizePreviewImageUrls() {
        QCOMPARE(UrlUtils::normalizePreviewImageUrl(QStringLiteral(" //image.lceda.cn/files/a.png ")),
                 QStringLiteral("https://image.lceda.cn/files/a.png"));
        QCOMPARE(UrlUtils::normalizePreviewImageUrl(QStringLiteral("/web1/M00/00/00/a.jpg")),
                 QStringLiteral("https://file.elecfans.com/web1/M00/00/00/a.jpg"));
        QCOMPARE(UrlUtils::normalizePreviewImageUrl(QStringLiteral("/M00/00/00/a.jpg")),
                 QStringLiteral("https://file.elecfans.com/M00/00/00/a.jpg"));
        QCOMPARE(UrlUtils::normalizePreviewImageUrl(QStringLiteral("/some/path/a.png")),
                 QStringLiteral("https://image.lceda.cn/some/path/a.png"));
        QCOMPARE(UrlUtils::normalizePreviewImageUrl(QStringLiteral("relative/a.png")),
                 QStringLiteral("https://image.lceda.cn/relative/a.png"));
        QCOMPARE(UrlUtils::normalizePreviewImageUrl(QStringLiteral("alimg.szlcsc.com/upload/a.png")),
                 QStringLiteral("https://alimg.szlcsc.com/upload/a.png"));
    }

    /**
     * @brief 验证 fileUtilsRoundTripsSpecialCharacters 测试场景。
     */
    void fileUtilsRoundTripsSpecialCharacters() {
        // fileUtils：测试使用的文件系统路径。
        const FileUtils fileUtils;
#ifdef Q_OS_WIN
        // path：测试使用的文件系统路径。
        const QString path = QStringLiteral("C:/tmp/work#1/folder?2");
        const QString expectedUrl = QStringLiteral("file:///C:/tmp/work%231/folder%3F2");
#else
        // path：测试使用的文件系统路径。
        const QString path = QStringLiteral("/tmp/work#1/folder?2");
        const QString expectedUrl = QStringLiteral("file:///tmp/work%231/folder%3F2");
#endif
        // url：测试用资源地址。
        const QUrl url = fileUtils.localPathToFileUrl(path);

        QCOMPARE(url.toString(QUrl::FullyEncoded), expectedUrl);
        QCOMPARE(fileUtils.urlToLocalPath(url), path);
    }

    /**
     * @brief 验证 fileUtilsParsesUncFileUrl 测试场景。
     */
    void fileUtilsParsesUncFileUrl() {
        // fileUtils：测试使用的文件系统路径。
        const FileUtils fileUtils;
        const QUrl url(QStringLiteral("file://server/share/folder%231"));
        // localPath：测试使用的文件系统路径。
        const QString localPath = fileUtils.urlToLocalPath(url);

        QCOMPARE(QDir::fromNativeSeparators(localPath), QStringLiteral("//server/share/folder#1"));
    }

    /**
     * @brief 验证 deduplicateAndNormalizeUrlsKeepsFirstOccurrenceOrder 测试场景。
     */
    void deduplicateAndNormalizeUrlsKeepsFirstOccurrenceOrder() {
        const QStringList urls = {
            QStringLiteral("//image.lceda.cn/a.png"),
            QStringLiteral("https://image.lceda.cn/a.png"),
            QStringLiteral("relative/b.png"),
            QStringLiteral(""),
            QStringLiteral(" relative/b.png "),
        };

        QCOMPARE(UrlUtils::deduplicateAndNormalizeUrls(urls),
                 QStringList({QStringLiteral("https://image.lceda.cn/a.png"),
                              QStringLiteral("https://image.lceda.cn/relative/b.png")}));
    }

    /**
     * @brief 验证 gzipDetectionAndDecompression 测试场景。
     */
    void gzipDetectionAndDecompression() {
        // plain：未压缩的测试数据。
        const QByteArray plain = QByteArrayLiteral("plain text payload long enough");
        QVERIFY(!GzipUtils::isGzipped(plain));

        // plainResult：测试得到的实际结果数据。
        const GzipUtils::DecompressResult plainResult = GzipUtils::decompress(plain);
        QVERIFY(plainResult.success);
        QCOMPARE(plainResult.data, plain);

        // gzipped：Gzip 压缩的测试数据。
        const QByteArray gzipped = QByteArray::fromHex("1f8b08001f3c036a02ffcb48cdc9c95748afca2c0000196ad2df0a000000");
        QVERIFY(GzipUtils::isGzipped(gzipped));

        // gzipResult：测试得到的实际结果数据。
        const GzipUtils::DecompressResult gzipResult = GzipUtils::decompress(gzipped);
        QVERIFY(gzipResult.success);
        QCOMPARE(gzipResult.data, QByteArrayLiteral("hello gzip"));

        // invalidResult：测试使用的元器件标识或名称。
        const GzipUtils::DecompressResult invalidResult = GzipUtils::decompress(QByteArray::fromHex("1f8b"));
        QVERIFY(!invalidResult.success);
        QVERIFY(invalidResult.data.isEmpty());
    }

    /**
     * @brief 验证 kicadTableValueEscaping 测试场景。
     */
    void kicadTableValueEscaping() {
        QCOMPARE(KiCadTableUtils::escapeTableValue(QStringLiteral("Plain")), QStringLiteral("Plain"));
        QCOMPARE(KiCadTableUtils::escapeTableValue(QStringLiteral("A \"quoted\" value")),
                 QStringLiteral("A \\\"quoted\\\" value"));
        QCOMPARE(KiCadTableUtils::escapeTableValue(QStringLiteral("C:\\KiCad\\Lib")),
                 QStringLiteral("C:\\\\KiCad\\\\Lib"));
    }

    /**
     * @brief 验证 layerMapperMapsIdsNamesAndFallbacks 测试场景。
     */
    void layerMapperMapsIdsNamesAndFallbacks() {
        // mapper：测试用映射器。
        const LayerMapper mapper;

        QCOMPARE(mapper.mapToKiCadLayer(1), static_cast<int>(LayerMapper::F_Cu));
        QCOMPARE(mapper.mapToKiCadLayer(2), static_cast<int>(LayerMapper::B_Cu));
        QCOMPARE(mapper.mapToKiCadLayer(3), static_cast<int>(LayerMapper::F_SilkS));
        QCOMPARE(mapper.mapToKiCadLayer(10), static_cast<int>(LayerMapper::Edge_Cuts));
        QCOMPARE(mapper.mapToKiCadLayer(9999), static_cast<int>(LayerMapper::Dwgs_User));

        QCOMPARE(mapper.mapToKiCadLayer(QStringLiteral("TopLayer")), static_cast<int>(LayerMapper::F_Cu));
        QCOMPARE(mapper.mapToKiCadLayer(QStringLiteral("BottomAssembly")), static_cast<int>(LayerMapper::B_Fab));
        QCOMPARE(mapper.mapToKiCadLayer(QStringLiteral("UnknownLayer")), static_cast<int>(LayerMapper::Dwgs_User));

        QCOMPARE(mapper.getKiCadLayerName(LayerMapper::F_Cu), QStringLiteral("F.Cu"));
        QCOMPARE(mapper.getKiCadLayerName(LayerMapper::B_Cu), QStringLiteral("B.Cu"));
        QCOMPARE(mapper.getKiCadLayerName(9999), QStringLiteral("Unknown(9999)"));
    }

    /**
     * @brief 验证 layerMapperClassifiesLayersAndConvertsUnits 测试场景。
     */
    void layerMapperClassifiesLayersAndConvertsUnits() {
        QVERIFY(LayerMapper::isSignalLayer(LayerMapper::F_Cu));
        QVERIFY(LayerMapper::isSignalLayer(LayerMapper::B_Cu));
        QVERIFY(!LayerMapper::isSignalLayer(LayerMapper::F_SilkS));

        QVERIFY(LayerMapper::isSilkLayer(LayerMapper::F_SilkS));
        QVERIFY(LayerMapper::isMaskLayer(LayerMapper::B_Mask));
        QVERIFY(LayerMapper::isPasteLayer(LayerMapper::F_Paste));
        QVERIFY(LayerMapper::isMechanicalLayer(LayerMapper::Edge_Cuts));

        QVERIFY(LayerMapper::isCourtYardLayer(48));
        QVERIFY(!LayerMapper::isCourtYardLayer(47));
        QCOMPARE(LayerMapper::getCourtYardLayerName(48), QStringLiteral("F.CrtYd"));
        QVERIFY(LayerMapper::getCourtYardLayerName(47).isEmpty());

        QVERIFY(LayerMapper::isInnerLayer(15));
        QVERIFY(LayerMapper::isInnerLayer(46));
        QVERIFY(!LayerMapper::isInnerLayer(47));

        QVERIFY(LayerMapper::isPadLayer(1));
        QVERIFY(LayerMapper::isPadLayer(11));
        QVERIFY(LayerMapper::isPadLayer(15));
        QVERIFY(!LayerMapper::isPadLayer(47));

        QVERIFY(std::abs(LayerMapper::milToMm(100.0) - 2.54) < 0.0001);
        QVERIFY(std::abs(LayerMapper::mmToMil(2.54) - 100.0) < 0.001);
    }
};

QTEST_GUILESS_MAIN(TestCoreUtils)
#include "test_core_utils.moc"
