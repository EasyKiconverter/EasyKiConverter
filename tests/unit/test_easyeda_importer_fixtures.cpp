#include "core/altium/ExporterAltiumFootprint.h"
#include "core/altium/ExporterAltiumSymbol.h"
#include "core/altium/readers/AltiumPcbLibReader.h"
#include "core/altium/readers/AltiumSchLibReader.h"
#include "core/altium/utils/AltiumConstants.h"
#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "core/ir/FootprintDataConverter.h"
#include "core/ir/SymbolDataConverter.h"
#include "tests/common/TestPaths.hpp"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

namespace {

// 为压缩数据恢复 Qt 解压所需的原始长度头。
QByteArray restoreQtCompressionHeader(const QByteArray& compressed, int uncompressedSize) {
    QByteArray withHeader;
    withHeader.reserve(compressed.size() + 4);
    withHeader.append(static_cast<char>((uncompressedSize >> 24) & 0xFF));
    withHeader.append(static_cast<char>((uncompressedSize >> 16) & 0xFF));
    withHeader.append(static_cast<char>((uncompressedSize >> 8) & 0xFF));
    withHeader.append(static_cast<char>(uncompressedSize & 0xFF));
    withHeader.append(compressed);
    return withHeader;
}

}  // namespace

class TestEasyedaImporterFixtures : public QObject {
    Q_OBJECT

private slots:

    // 验证符号夹具能够导入元数据和几何图元。
    void testSymbolFixtureImportsMetadataAndGeometry() {
        QString error;
        const QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/symbol_basic.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        EasyedaSymbolImporter importer;
        const QSharedPointer<SymbolData> symbol = importer.importSymbolData(fixture);

        QVERIFY(symbol);
        QCOMPARE(symbol->info().name, QStringLiteral("FIXTURE_SYMBOL"));
        QCOMPARE(symbol->info().prefix, QStringLiteral("U"));
        QCOMPARE(symbol->info().package, QStringLiteral("FIXTURE_FOOTPRINT"));
        QCOMPARE(symbol->info().manufacturer, QStringLiteral("Fixture Inc"));
        QCOMPARE(symbol->info().lcscId, QStringLiteral("C12345"));
        QCOMPARE(symbol->info().datasheet, QStringLiteral("https://example.test/datasheet.pdf"));
        QCOMPARE(symbol->bbox().width, 120.0);
        QCOMPARE(symbol->bbox().height, 80.0);
        QCOMPARE(symbol->graphicOrder().size(), 6);
        QCOMPARE(symbol->graphicOrder().at(0).type, QStringLiteral("R"));
        QCOMPARE(symbol->graphicOrder().at(0).index, 0);
        QCOMPARE(symbol->graphicOrder().at(1).type, QStringLiteral("PT"));
        QCOMPARE(symbol->graphicOrder().at(1).index, 0);
        QCOMPARE(symbol->graphicOrder().at(2).type, QStringLiteral("A"));
        QCOMPARE(symbol->graphicOrder().at(2).index, 0);
        QCOMPARE(symbol->graphicOrder().at(3).type, QStringLiteral("I"));
        QCOMPARE(symbol->graphicOrder().at(3).index, 0);
        QCOMPARE(symbol->graphicOrder().at(4).type, QStringLiteral("T"));
        QCOMPARE(symbol->graphicOrder().at(4).index, 0);
        QCOMPARE(symbol->graphicOrder().at(5).type, QStringLiteral("P"));
        QCOMPARE(symbol->graphicOrder().at(5).index, 0);

        QCOMPARE(symbol->texts().size(), 1);
        QCOMPARE(symbol->texts().first().text, QStringLiteral("LABEL"));
        QCOMPARE(symbol->texts().first().anchor, QStringLiteral("start"));
        QCOMPARE(symbol->texts().first().font, QStringLiteral("Arial"));
        QCOMPARE(symbol->texts().first().textSize, 7.0);

        QCOMPARE(symbol->rectangles().size(), 1);
        QCOMPARE(symbol->rectangles().first().width, 60.0);
        QCOMPARE(symbol->rectangles().first().height, 40.0);
        QCOMPARE(symbol->rectangles().first().rx, 4.0);
        QCOMPARE(symbol->rectangles().first().ry, 6.0);
        const IR::SymbolComponentIR symbolIr = IR::toSymbolIR(*symbol);
        QCOMPARE(symbolIr.rectangles.size(), 1);
        QVERIFY(symbolIr.rectangles.first().cornerRadiusX > 0.0);
        QVERIFY(symbolIr.rectangles.first().cornerRadiusY > 0.0);
        QCOMPARE(symbolIr.rectangles.first().strokeStyle, IR::StrokeStyle::Dashed);
        QCOMPARE(symbol->paths().size(), 1);
        QCOMPARE(
            symbol->paths().first().paths,
            QStringLiteral(
                "M 0 0 C 0 10 10 10 10 0 L 20 0 Q 25 5 30 0 A 5 5 0 0 0 40 0 A 10 5 0 0 0 60 0 A 10 5 30 0 1 70 10"));
        QCOMPARE(symbolIr.paths.size(), 1);
        QCOMPARE(symbolIr.paths.first().segments.size(), 7);
        QCOMPARE(symbolIr.paths.first().segments.first().type, IR::SymbolPathSegmentIR::Type::CubicBezier);
        QCOMPARE(symbolIr.paths.first().segments.at(1).type, IR::SymbolPathSegmentIR::Type::Line);
        QCOMPARE(symbolIr.paths.first().segments.at(2).type, IR::SymbolPathSegmentIR::Type::QuadraticBezier);
        QCOMPARE(symbolIr.paths.first().segments.at(3).type, IR::SymbolPathSegmentIR::Type::CircularArc);
        QCOMPARE(symbolIr.paths.first().segments.at(4).type, IR::SymbolPathSegmentIR::Type::EllipticalArc);
        QCOMPARE(symbolIr.paths.first().segments.first().end, QPointF(2.54, 0.0));
        QCOMPARE(symbolIr.paths.first().segments.at(2).control1, QPointF(6.35, -1.27));
        QCOMPARE(symbolIr.paths.first().segments.at(2).end, QPointF(7.62, 0.0));
        QCOMPARE(symbolIr.paths.first().segments.at(3).end, QPointF(10.16, 0.0));
        QCOMPARE(symbolIr.paths.first().segments.at(4).end, QPointF(15.24, 0.0));
        QCOMPARE(symbolIr.paths.first().segments.at(4).arcCenter, QPointF(12.7, 0.0));
        QCOMPARE(symbolIr.paths.first().segments.at(4).radiusX, 2.54);
        QCOMPARE(symbolIr.paths.first().segments.at(4).radiusY, 1.27);
        QCOMPARE(symbolIr.paths.first().segments.at(5).type, IR::SymbolPathSegmentIR::Type::CubicBezier);
        QCOMPARE(symbolIr.paths.first().segments.at(6).type, IR::SymbolPathSegmentIR::Type::CubicBezier);
        QCOMPARE(symbolIr.paths.first().segments.at(5).start, QPointF(15.24, 0.0));
        QVERIFY(qAbs(symbolIr.paths.first().segments.last().end.x() - 17.78) < 1e-9);
        QVERIFY(qAbs(symbolIr.paths.first().segments.last().end.y() + 2.54) < 1e-9);
        QCOMPARE(symbolIr.arcs.size(), 1);
        QCOMPARE(symbolIr.arcs.first().startPoint, QPointF(0.0, 0.0));
        QCOMPARE(symbolIr.arcs.first().endPoint, QPointF(5.08, 0.0));
        QCOMPARE(symbolIr.images.size(), 1);
        QCOMPARE(symbolIr.images.first().fileName, QStringLiteral("image.png"));
        QVERIFY(!symbolIr.images.first().data.isEmpty());
        SymbolData restored;
        QVERIFY(restored.fromJson(symbol->toJson()));
        QCOMPARE(restored.images().size(), 1);
        QCOMPARE(restored.images().first().fileName, QStringLiteral("image.png"));
        QCOMPARE(restored.images().first().data, symbol->images().first().data);
        QCOMPARE(symbolIr.graphicOrder.size(), 6);
        QCOMPARE(symbolIr.graphicOrder.at(0).type, QStringLiteral("R"));
        QCOMPARE(symbolIr.graphicOrder.at(1).type, QStringLiteral("PT"));
        QCOMPARE(symbolIr.graphicOrder.at(2).type, QStringLiteral("A"));
        QCOMPARE(symbolIr.graphicOrder.at(3).type, QStringLiteral("I"));
        QCOMPARE(symbolIr.graphicOrder.at(4).type, QStringLiteral("T"));
        QCOMPARE(symbolIr.graphicOrder.at(5).type, QStringLiteral("P"));
        QCOMPARE(symbolIr.texts.size(), 1);
        QCOMPARE(symbolIr.texts.first().text, QStringLiteral("LABEL"));
        QCOMPARE(symbolIr.texts.first().anchor, QStringLiteral("start"));
        QCOMPARE(symbolIr.texts.first().fontFamily, QStringLiteral("Arial"));
        QVERIFY(qAbs(symbolIr.texts.first().fontSizeMm - 7.0 * 25.4 / 72.0) < 1e-5);
        QVERIFY(symbolIr.texts.first().bold);
        QVERIFY(!symbolIr.texts.first().italic);
        QCOMPARE(symbolIr.texts.first().color, QColor(QStringLiteral("#112233")));

        QCOMPARE(symbol->pins().size(), 1);
        const SymbolPin pin = symbol->pins().first();
        QCOMPARE(pin.settings.spicePinNumber, QStringLiteral("1"));
        QCOMPARE(pin.settings.posX, 10.0);
        QCOMPARE(pin.settings.posY, 20.0);
        QCOMPARE(pin.settings.rotation, 0);
        QCOMPARE(pin.settings.type, PinType::Input);
        QCOMPARE(pin.name.text, QStringLiteral("VCC"));
        QCOMPARE(pin.pinPath.path, QStringLiteral("M 10 20 h 20"));
    }

    /**
     * @brief 验证未支持的 EasyEDA 图元不会静默丢失诊断信息。
     * @details 未知 designator 以无效顺序引用保留，供模型校验和下游导出报告使用。
     */
    void testUnsupportedShapeIsReportedInGraphicOrder() {
        QString error;
        QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/symbol_basic.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        QJsonObject dataStr = fixture.value(QStringLiteral("dataStr")).toObject();
        QJsonArray shapes = dataStr.value(QStringLiteral("shape")).toArray();
        shapes.append(QStringLiteral("UNKNOWN~payload"));
        dataStr[QStringLiteral("shape")] = shapes;
        fixture[QStringLiteral("dataStr")] = dataStr;

        EasyedaSymbolImporter importer;
        const QSharedPointer<SymbolData> symbol = importer.importSymbolData(fixture);
        QVERIFY(symbol);
        QCOMPARE(symbol->graphicOrder().size(), 7);
        QCOMPARE(symbol->graphicOrder().last().type, QStringLiteral("UNKNOWN"));
        QCOMPARE(symbol->graphicOrder().last().index, -1);

        const QStringList validationErrors = symbol->validationErrors();
        QVERIFY(validationErrors.join('\n').contains(QStringLiteral("unknown type UNKNOWN")));

        SymbolData restored;
        QVERIFY(restored.fromJson(symbol->toJson()));
        QCOMPARE(restored.graphicOrder().last().type, QStringLiteral("UNKNOWN"));
        QCOMPARE(restored.graphicOrder().last().index, -1);
        QVERIFY(restored.validationErrors().join('\n').contains(QStringLiteral("unknown type UNKNOWN")));

        const IR::SymbolComponentIR symbolIr = IR::toSymbolIR(restored);
        QTemporaryDir outputDir;
        QVERIFY(outputDir.isValid());
        const QString outputPath = outputDir.filePath(QStringLiteral("unsupported-shape.SchLib"));
        ExporterAltiumSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary({symbolIr}, QStringLiteral("unsupported-shape"), outputPath, false));
        QVERIFY(exporter.diagnostics().contains(
            QStringLiteral("符号 FIXTURE_SYMBOL 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(outputPath), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY2(reader.readComponentRecords(0, &records), qPrintable(reader.errorString()));
        QVERIFY(!records.isEmpty());
    }

    /**
     * @brief 验证多部件符号中的未支持图元同样保留诊断引用。
     * @details 覆盖公共 Part 的导入分支，避免单部分修复与多部分逻辑出现行为分歧。
     */
    void testUnsupportedShapeIsReportedInMultipartGraphicOrder() {
        QString error;
        QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/symbol_multipart.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        QJsonArray subparts = fixture.value(QStringLiteral("subparts")).toArray();
        QVERIFY(!subparts.isEmpty());
        QJsonObject commonPart = subparts.first().toObject();
        QJsonObject dataStr = commonPart.value(QStringLiteral("dataStr")).toObject();
        QJsonArray shapes = dataStr.value(QStringLiteral("shape")).toArray();
        shapes.append(QStringLiteral("UNKNOWN~payload"));
        dataStr[QStringLiteral("shape")] = shapes;
        commonPart[QStringLiteral("dataStr")] = dataStr;
        subparts[0] = commonPart;
        fixture[QStringLiteral("subparts")] = subparts;

        EasyedaSymbolImporter importer;
        const QSharedPointer<SymbolData> symbol = importer.importSymbolData(fixture);
        QVERIFY(symbol);
        QVERIFY(symbol->isMultiPart());
        QCOMPARE(symbol->parts().first().graphicOrder.last().type, QStringLiteral("UNKNOWN"));
        QCOMPARE(symbol->parts().first().graphicOrder.last().index, -1);
        QVERIFY(symbol->validationErrors().join('\n').contains(QStringLiteral("unknown type UNKNOWN")));
    }

    /**
     * @brief 验证真实 EasyEDA 源数据能够完整写出为 Altium SchLib
     * @details 覆盖源 JSON、SymbolData、通用 IR 和 SchLib 导出之间的完整链路。
     */
    void testSymbolFixtureExportsThroughCompleteAltiumChain() {
        QString error;
        const QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/symbol_basic.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        EasyedaSymbolImporter importer;
        const QSharedPointer<SymbolData> symbol = importer.importSymbolData(fixture);
        QVERIFY(symbol);

        const IR::SymbolComponentIR symbolIr = IR::toSymbolIR(*symbol);
        QVERIFY(!symbolIr.name.isEmpty());
        QVERIFY(!symbolIr.rectangles.isEmpty());
        QVERIFY(!symbolIr.paths.isEmpty());
        QVERIFY(!symbolIr.arcs.isEmpty());
        QVERIFY(!symbolIr.images.isEmpty());
        QVERIFY(!symbolIr.pins.isEmpty());
        QCOMPARE(symbolIr.sourceMetadata.value(QStringLiteral("manufacturer")), QStringLiteral("Fixture Inc"));
        QCOMPARE(symbolIr.sourceMetadata.value(QStringLiteral("lcscId")), QStringLiteral("C12345"));
        QCOMPARE(symbolIr.sourceMetadata.value(QStringLiteral("supplier")), QStringLiteral("LCSC"));
        QCOMPARE(symbolIr.sourceMetadata.value(QStringLiteral("manufacturerPart")), QStringLiteral("FIX-123"));
        QCOMPARE(symbolIr.sourceMetadata.value(QStringLiteral("jlcpcbPartClass")), QStringLiteral("Basic"));
        QCOMPARE(symbolIr.texts.size(), 1);
        QCOMPARE(symbolIr.texts.first().text, QStringLiteral("LABEL"));
        QCOMPARE(symbolIr.texts.first().anchor, QStringLiteral("start"));
        QCOMPARE(symbolIr.texts.first().fontFamily, QStringLiteral("Arial"));
        QVERIFY(symbolIr.texts.first().bold);
        QCOMPARE(symbolIr.texts.first().color, QColor(QStringLiteral("#112233")));

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("fixture.SchLib"));
        ExporterAltiumSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary({symbolIr}, QStringLiteral("fixture"), outputPath, false, false));
        QVERIFY(QFileInfo::exists(outputPath));
        QVERIFY(QFileInfo(outputPath).size() > 0);
        QVERIFY(exporter.diagnostics().isEmpty());

        QFile outputFile(outputPath);
        QVERIFY(outputFile.open(QIODevice::ReadOnly));
        const QByteArray schLibData = outputFile.readAll();
        QVERIFY(schLibData.contains("RECORD=10"));  // 圆角矩形
        QVERIFY(schLibData.contains("RECORD=5"));  // 路径中的三次 Bézier
        QVERIFY(schLibData.contains("RECORD=6"));  // 路径中的线段
        QVERIFY(schLibData.contains("RECORD=12"));  // 原生圆弧
        QVERIFY(schLibData.contains("RECORD=11"));  // 路径中的椭圆弧
        QVERIFY(schLibData.contains("RECORD=30"));  // 嵌入图片
        QVERIFY(schLibData.contains("RECORD=4"));  // 普通文本
        QVERIFY(schLibData.contains("Text=LABEL"));
        QVERIFY(schLibData.contains("TextAnchor=start"));
        QVERIFY(schLibData.contains("Arial"));
        QVERIFY(schLibData.contains(QByteArray::fromHex("02000000")));  // 二进制引脚记录类型
        QVERIFY(schLibData.contains("IndexInSheet=1"));
        QVERIFY(schLibData.contains("IndexInSheet=2"));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(outputPath), qPrintable(reader.errorString()));
        QCOMPARE(reader.components().size(), 1);
        QCOMPARE(reader.components().first().name, QStringLiteral("FIXTURE_SYMBOL"));
        QVector<AltiumSchLibReader::ImageStorageEntry> imageEntries;
        QVERIFY2(reader.readImageStorage(&imageEntries), qPrintable(reader.errorString()));
        QCOMPARE(imageEntries.size(), 1);
        QCOMPARE(imageEntries.first().name, QStringLiteral("image.png"));
        QCOMPARE(qUncompress(restoreQtCompressionHeader(imageEntries.first().compressedData,
                                                        symbolIr.images.first().data.size())),
                 symbolIr.images.first().data);
        const QVector<AltiumSchLibReader::FontInfo> fonts = reader.fonts();
        bool foundArialFont = false;
        for (const auto& font : fonts) {
            if (font.name == QStringLiteral("Arial") && font.size == 7 && font.bold) {
                foundArialFont = true;
                break;
            }
        }
        QVERIFY(foundArialFont);
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(reader.readComponentRecords(QStringLiteral("FIXTURE_SYMBOL"), &records));
        QVERIFY(records.size() >= 7);
        QVERIFY(records.first().hasParameters);
        QCOMPARE(records.first().parameters.value(QStringLiteral("RECORD")), QStringLiteral("1"));
        QCOMPARE(records.first().parameters.value(QStringLiteral("LibReference")), QStringLiteral("FIXTURE_SYMBOL"));
        bool foundRoundedRectangle = false;
        bool foundBezier = false;
        bool foundLine = false;
        bool foundArc = false;
        bool foundText = false;
        bool foundBinaryPin = false;
        bool foundManufacturer = false;
        bool foundLCSCPart = false;
        bool foundSupplier = false;
        bool foundManufacturerPart = false;
        bool foundJlcpcbPartClass = false;
        for (const auto& record : records) {
            if (!record.hasParameters) {
                if (record.payload.size() >= 4 &&
                    static_cast<unsigned char>(record.payload.at(0)) == static_cast<unsigned char>(2))
                    foundBinaryPin = true;
                continue;
            }
            const QString recordType = record.parameters.value(QStringLiteral("RECORD"));
            foundRoundedRectangle |= recordType == QStringLiteral("10");
            foundBezier |= recordType == QStringLiteral("5");
            foundLine |= recordType == QStringLiteral("6");
            foundArc |= recordType == QStringLiteral("12");
            foundText |= recordType == QStringLiteral("4") &&
                         record.parameters.value(QStringLiteral("Text")) == QStringLiteral("LABEL");
            if (recordType == QStringLiteral("4") &&
                record.parameters.value(QStringLiteral("Text")) == QStringLiteral("LABEL")) {
                QVERIFY(record.fontId > 0);
                QCOMPARE(fonts.at(record.fontId - 1).name, QStringLiteral("Arial"));
            }
            if (recordType == QStringLiteral("41")) {
                const QString name = record.parameters.value(QStringLiteral("NAME"));
                const QString text = record.parameters.value(QStringLiteral("TEXT"));
                foundManufacturer |= name == QStringLiteral("Manufacturer") && text == QStringLiteral("Fixture Inc");
                foundLCSCPart |= name == QStringLiteral("LCSC Part") && text == QStringLiteral("C12345");
                foundSupplier |= name == QStringLiteral("Supplier") && text == QStringLiteral("LCSC");
                foundManufacturerPart |=
                    name == QStringLiteral("Manufacturer Part Number") && text == QStringLiteral("FIX-123");
                foundJlcpcbPartClass |= name == QStringLiteral("JLCPCB Part Class") && text == QStringLiteral("Basic");
            }
        }
        QVERIFY(foundRoundedRectangle);
        QVERIFY(foundBezier);
        QVERIFY(foundLine);
        QVERIFY(foundArc);
        QVERIFY(foundText);
        QVERIFY(foundBinaryPin);
        QVERIFY(foundManufacturer);
        QVERIFY(foundLCSCPart);
        QVERIFY(foundSupplier);
        QVERIFY(foundManufacturerPart);
        QVERIFY(foundJlcpcbPartClass);
    }

    /**
     * @brief 验证基础符号的 SchLib 核心记录顺序与归属快照
     * @details Golden 只保留稳定语义字段，不冻结随机 UniqueID 和 OLE 物理布局。
     */
    void testSymbolFixtureMatchesAltiumRecordGolden() {
        QString error;
        const QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/symbol_basic.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        EasyedaSymbolImporter importer;
        const QSharedPointer<SymbolData> symbol = importer.importSymbolData(fixture);
        QVERIFY(symbol);

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("fixture.SchLib"));
        ExporterAltiumSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary(
            {IR::toSymbolIR(*symbol)}, QStringLiteral("fixture"), outputPath, false, false));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(outputPath), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(reader.readComponentRecords(QStringLiteral("FIXTURE_SYMBOL"), &records));
        bool weightOk = false;
        const int declaredWeight = reader.headerParameters().value(QStringLiteral("WEIGHT")).toInt(&weightOk);
        QVERIFY(weightOk);
        QCOMPARE(declaredWeight, records.size());

        QString actual;
        for (const auto& record : records) {
            if (record.recordType != 1 && record.recordType != 2 && record.recordType != 4 && record.recordType != 5 &&
                record.recordType != 6 && record.recordType != 10 && record.recordType != 11 &&
                record.recordType != 12 && record.recordType != 30) {
                continue;
            }
            actual += QStringLiteral("record=%1|owner=%2|display=%3|index=%4")
                          .arg(record.recordType)
                          .arg(record.ownerPartId)
                          .arg(record.ownerPartDisplayMode)
                          .arg(record.indexInSheet);
            if (record.recordType == 4) {
                actual += QStringLiteral("|text=%1|anchor=%2")
                              .arg(record.parameters.value(QStringLiteral("Text")),
                                   record.parameters.value(QStringLiteral("TextAnchor")));
            } else if (record.recordType == 5 || record.recordType == 6) {
                actual += QStringLiteral("|locations=%1").arg(record.parameters.value(QStringLiteral("LocationCount")));
            }
            actual += QLatin1Char('\n');
        }

        QVERIFY2(TestPaths::compareTextToGolden(actual, QStringLiteral("altium/symbol_basic_records.txt"), &error),
                 qPrintable(error));

        QList<int> contentIndexes;
        bool hasBinaryPin = false;
        for (const auto& record : records) {
            if (!record.hasParameters && record.recordType == 2) {
                hasBinaryPin = true;
                continue;
            }
            if (record.indexInSheet >= 0)
                contentIndexes.append(record.indexInSheet);
        }
        QVERIFY(hasBinaryPin);
        QCOMPARE(contentIndexes, QList<int>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13}));
    }

    /**
     * @brief 验证真实 EasyEDA 多部件符号的公共部件归属能够完整写入 SchLib
     * @details 覆盖公共 Part 0、两个普通部件、图形和引脚的 OWNERPARTID 映射。
     */
    void testMultipartSymbolFixturePreservesPartOwnership() {
        QString error;
        const QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/symbol_multipart.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        EasyedaSymbolImporter importer;
        const QSharedPointer<SymbolData> symbol = importer.importSymbolData(fixture);
        QVERIFY(symbol);
        QCOMPARE(symbol->parts().size(), 3);
        QVERIFY(symbol->parts().at(0).commonToAllParts);
        QVERIFY(!symbol->parts().at(1).commonToAllParts);
        QVERIFY(!symbol->parts().at(2).commonToAllParts);

        SymbolData restored;
        QVERIFY(restored.fromJson(symbol->toJson()));
        QCOMPARE(restored.parts().size(), 3);
        QVERIFY(restored.parts().at(0).commonToAllParts);
        QCOMPARE(restored.parts().at(1).graphicOrder.size(), 3);
        QCOMPARE(restored.parts().at(2).graphicOrder.size(), 2);
        QCOMPARE(restored.parts().at(1).images.size(), 1);
        QCOMPARE(restored.parts().at(1).images.first().fileName, QStringLiteral("image.png"));
        QVERIFY(!restored.parts().at(1).images.first().data.isEmpty());

        IR::SymbolComponentIR symbolIr = IR::toSymbolIR(restored);
        QCOMPARE(symbolIr.partCount, 2);
        QCOMPARE(symbolIr.pins.size(), 3);
        QCOMPARE(symbolIr.rectangles.size(), 3);
        QCOMPARE(symbolIr.images.size(), 1);
        QCOMPARE(symbolIr.images.first().partIndex, 0);
        QCOMPARE(symbolIr.images.first().fileName, QStringLiteral("image.png"));

        IR::SymbolParameterIR partParameter;
        partParameter.name = QStringLiteral("PART_PARAMETER");
        partParameter.value = QStringLiteral("PART_B_VALUE");
        partParameter.visible = true;
        partParameter.position = QPointF(205.0, 205.0);
        partParameter.fontSizeMm = 2.0;
        partParameter.partIndex = 1;
        symbolIr.parameters.append(partParameter);

        int commonPinCount = 0;
        int partOnePinCount = 0;
        int partTwoPinCount = 0;
        for (const auto& pin : symbolIr.pins) {
            if (pin.partIndex == -1) {
                ++commonPinCount;
            } else if (pin.partIndex == 0) {
                ++partOnePinCount;
            } else if (pin.partIndex == 1) {
                ++partTwoPinCount;
            }
        }
        QCOMPARE(commonPinCount, 1);
        QCOMPARE(partOnePinCount, 1);
        QCOMPARE(partTwoPinCount, 1);

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("multipart.SchLib"));
        ExporterAltiumSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary({symbolIr}, QStringLiteral("multipart"), outputPath, false, false));
        QVERIFY2(exporter.diagnostics().isEmpty(), qPrintable(exporter.diagnostics().join('\n')));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(outputPath), qPrintable(reader.errorString()));
        QCOMPARE(reader.components().size(), 1);
        QCOMPARE(reader.components().first().partCount, 2);
        QVector<AltiumSchLibReader::ImageStorageEntry> imageEntries;
        QVERIFY2(reader.readImageStorage(&imageEntries), qPrintable(reader.errorString()));
        QCOMPARE(imageEntries.size(), 1);
        QCOMPARE(imageEntries.first().name, QStringLiteral("image.png"));
        QCOMPARE(qUncompress(restoreQtCompressionHeader(imageEntries.first().compressedData,
                                                        symbolIr.images.first().data.size())),
                 symbolIr.images.first().data);

        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(reader.readComponentRecords(QStringLiteral("MULTIPART_SYMBOL"), &records));
        int commonRecordCount = 0;
        int partOneRecordCount = 0;
        int partTwoRecordCount = 0;
        int binaryPinCount = 0;
        int commonParameterCount = 0;
        int partOneRectangleCount = 0;
        int partTwoRectangleCount = 0;
        int partOneImageCount = 0;
        int partTwoParameterCount = 0;
        QList<int> contentIndexes;
        for (const auto& record : records) {
            if (record.indexInSheet >= 0)
                contentIndexes.append(record.indexInSheet);
            if (record.ownerPartId == -1)
                ++commonRecordCount;
            else if (record.ownerPartId == 1)
                ++partOneRecordCount;
            else if (record.ownerPartId == 2)
                ++partTwoRecordCount;
            if (!record.hasParameters && record.payload.size() >= 4 &&
                static_cast<unsigned char>(record.payload.at(0)) == static_cast<unsigned char>(2)) {
                QCOMPARE(record.ownerPartDisplayMode, 1);
                ++binaryPinCount;
            }
            if ((record.recordType == 10 || record.recordType == 14) && record.ownerPartId == 1)
                ++partOneRectangleCount;
            if ((record.recordType == 10 || record.recordType == 14) && record.ownerPartId == 2)
                ++partTwoRectangleCount;
            if (record.recordType == 30 && record.ownerPartId == 1)
                ++partOneImageCount;
            if (record.recordType == 34) {
                QCOMPARE(record.ownerPartId, -1);
                ++commonParameterCount;
            }
            if (record.recordType == 41 &&
                record.parameters.value(QStringLiteral("NAME")) == QStringLiteral("PART_PARAMETER")) {
                QCOMPARE(record.ownerPartId, 2);
                QCOMPARE(record.ownerPartDisplayMode, 1);
                ++partTwoParameterCount;
            } else if (record.recordType == 41) {
                QCOMPARE(record.ownerPartId, -1);
                ++commonParameterCount;
            }
        }
        QVERIFY(commonRecordCount >= 2);
        QVERIFY(partOneRecordCount >= 3);
        QVERIFY(partTwoRecordCount >= 2);
        QCOMPARE(binaryPinCount, 3);
        QCOMPARE(partOneRectangleCount, 1);
        QCOMPARE(partTwoRectangleCount, 1);
        QCOMPARE(partOneImageCount, 1);
        QVERIFY(commonParameterCount >= 2);
        QCOMPARE(partTwoParameterCount, 1);
        for (int i = 1; i < contentIndexes.size(); ++i)
            QVERIFY(contentIndexes.at(i) > contentIndexes.at(i - 1));
    }

    // 验证封装夹具能够导入元数据和几何图元。
    void testFootprintFixtureImportsMetadataAndGeometry() {
        QString error;
        const QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/footprint_basic.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        EasyedaFootprintImporter importer;
        const QSharedPointer<FootprintData> footprint = importer.importFootprintData(fixture);

        QVERIFY(footprint);
        QCOMPARE(footprint->info().name, QStringLiteral("FIXTURE_FOOTPRINT"));
        QCOMPARE(footprint->info().type, QStringLiteral("smd"));
        QCOMPARE(footprint->info().model3DName, QStringLiteral("FIXTURE_MODEL"));
        QCOMPARE(footprint->bbox().width, 100.0);
        QCOMPARE(footprint->bbox().height, 50.0);

        QCOMPARE(footprint->pads().size(), 1);
        const FootprintPad pad = footprint->pads().first();
        QCOMPARE(pad.shape, QStringLiteral("rect"));
        QCOMPARE(pad.centerX, 10.0);
        QCOMPARE(pad.centerY, 20.0);
        QCOMPARE(pad.width, 30.0);
        QCOMPARE(pad.height, 40.0);
        QCOMPARE(pad.layerId, 1);
        QCOMPARE(pad.number, QStringLiteral("1"));
        QCOMPARE(pad.rotation, 90.0);

        QCOMPARE(footprint->tracks().size(), 1);
        QCOMPARE(footprint->tracks().first().points, QStringLiteral("0 0 100 50"));
        QCOMPARE(footprint->rectangles().size(), 1);
        QCOMPARE(footprint->rectangles().first().strokeWidth, 1.0);
        QCOMPARE(footprint->layers().size(), 2);
        QCOMPARE(footprint->objectVisibilities().size(), 2);
    }

    // 验证不支持的封装图形会产生诊断并保留序列化信息。
    void testUnsupportedFootprintShapeIsReportedAndSerialized() {
        QString error;
        QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/footprint_basic.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        QJsonObject packageDetail = fixture.value(QStringLiteral("packageDetail")).toObject();
        QJsonObject dataStr = packageDetail.value(QStringLiteral("dataStr")).toObject();
        QJsonArray shapes = dataStr.value(QStringLiteral("shape")).toArray();
        shapes.append(QStringLiteral("UNSUPPORTED_SHAPE~payload"));
        dataStr.insert(QStringLiteral("shape"), shapes);
        packageDetail.insert(QStringLiteral("dataStr"), dataStr);
        fixture.insert(QStringLiteral("packageDetail"), packageDetail);

        EasyedaFootprintImporter importer;
        const QSharedPointer<FootprintData> footprint = importer.importFootprintData(fixture);
        QVERIFY(footprint);
        QVERIFY(footprint->validationErrors().join('\n').contains(QStringLiteral("UNSUPPORTED_SHAPE")));

        FootprintData restored;
        QVERIFY(restored.fromJson(footprint->toJson()));
        QVERIFY(restored.validationErrors().join('\n').contains(QStringLiteral("UNSUPPORTED_SHAPE")));
    }

    // 验证封装夹具能够通过完整 Altium 导出链路生成结果。
    void testFootprintFixtureExportsThroughCompleteAltiumChain() {
        QString error;
        const QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/footprint_basic.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        EasyedaFootprintImporter importer;
        const QSharedPointer<FootprintData> footprint = importer.importFootprintData(fixture);
        QVERIFY(footprint);

        const IR::FootprintComponentIR footprintIr = IR::toFootprintIR(*footprint);
        QCOMPARE(footprintIr.name, QStringLiteral("FIXTURE_FOOTPRINT"));
        QCOMPARE(footprintIr.pads.size(), 1);
        QCOMPARE(footprintIr.tracks.size(), 1);
        QCOMPARE(footprintIr.rectangles.size(), 1);

        QTemporaryDir outputDir;
        QVERIFY(outputDir.isValid());
        const QString outputPath = outputDir.filePath(QStringLiteral("fixture.PcbLib"));
        ExporterAltiumFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprintIr}, QStringLiteral("fixture"), outputPath));
        QVERIFY2(exporter.diagnostics().isEmpty(), qPrintable(exporter.diagnostics().join('\n')));

        AltiumPcbLibReader reader;
        QVERIFY2(reader.open(outputPath), qPrintable(reader.errorString()));
        QVector<AltiumPcbLibReader::PrimitiveRecord> objects;
        QVERIFY2(reader.readFootprintObjects(QStringLiteral("FIXTURE_FOOTPRINT"), &objects),
                 qPrintable(reader.errorString()));

        bool foundPad = false;
        bool foundTrack = false;
        bool foundFill = false;
        for (const auto& object : objects) {
            if (object.objectId == AltiumConstants::PCB_OBJECT_PAD) {
                QVERIFY(object.hasPadFields);
                foundPad = true;
            } else if (object.objectId == AltiumConstants::PCB_OBJECT_TRACK) {
                QVERIFY(object.hasTrackFields);
                foundTrack = true;
            } else if (object.objectId == AltiumConstants::PCB_OBJECT_FILL) {
                QVERIFY(object.hasFillFields);
                foundFill = true;
            }
        }
        QVERIFY(foundPad);
        QVERIFY(foundTrack);
        QVERIFY(foundFill);
    }

    // 验证几何规范化器接受不同类型的空白分隔符。
    void testGeometryNormalizerAcceptsAllWhitespaceSeparators() {
        const QList<QPointF> flatPoints =
            IR::GeometryNormalizer::parseFlatPointString(QStringLiteral(" 0\t0\n10 20\r\n30\t40 "), 1.0);
        QCOMPARE(flatPoints, QList<QPointF>({QPointF(0.0, 0.0), QPointF(10.0, 20.0), QPointF(30.0, 40.0)}));

        const QList<QPointF> commaPoints =
            IR::GeometryNormalizer::parseCommaSeparatedPoints(QStringLiteral("0,0\t10,20\n30,40"), 1.0);
        QCOMPARE(commaPoints, QList<QPointF>({QPointF(0.0, 0.0), QPointF(10.0, 20.0), QPointF(30.0, 40.0)}));
    }

    // 验证包含三维数据的 CAD 夹具能够导入符号和封装。
    void testRealCadFixtureWith3DImportsSymbolAndFootprint() {
        QString error;
        const QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/cad_basic.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        EasyedaSymbolImporter symbolImporter;
        const QSharedPointer<SymbolData> symbol = symbolImporter.importSymbolData(fixture);

        QVERIFY(symbol);
        QCOMPARE(symbol->info().name, QStringLiteral("0603WAF5101T5E"));
        QCOMPARE(symbol->info().package, QStringLiteral("R0603"));
        QCOMPARE(symbol->info().manufacturer, QStringLiteral("UNI-ROYAL(厚声)"));
        QCOMPARE(symbol->info().lcscId, QStringLiteral("C23186"));
        QVERIFY(symbol->pins().size() >= 2);

        EasyedaFootprintImporter footprintImporter;
        const QSharedPointer<FootprintData> footprint = footprintImporter.importFootprintData(fixture);

        QVERIFY(footprint);
        QCOMPARE(footprint->info().name, QStringLiteral("R0603"));
        QCOMPARE(footprint->info().model3DName, QStringLiteral("R0603"));
        QCOMPARE(footprint->info().uuid3d, QStringLiteral("e06b23482b894f1ebafac2e1696a59f5"));
        QCOMPARE(footprint->model3D().uuid(), QStringLiteral("6bd5cd867e9542ebae21caaf5d2d4c4d"));
        QCOMPARE(footprint->model3D().name(), QStringLiteral("R0603"));
        QVERIFY(footprint->pads().size() >= 2);
        QVERIFY(footprint->outlines().size() >= 1);

        const IR::FootprintComponentIR footprintIr = IR::toFootprintIR(*footprint);
        QVERIFY(!footprintIr.pads.isEmpty());
        const auto smdPad = std::find_if(
            footprintIr.pads.cbegin(), footprintIr.pads.cend(), [](const auto& pad) { return pad.isSmd(); });
        QVERIFY(smdPad != footprintIr.pads.cend());
        QCOMPARE(smdPad->solderMaskExpansionMm, 0.3);
        QCOMPARE(smdPad->pasteMaskContractionMm, 0.0);
    }

    // 验证缺少三维 UUID 时仍能从轮廓数据导入模型信息。
    void testRealCadFixtureWithoutUuid3DStillImportsOutlineModel() {
        QString error;
        const QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/cad_no_3d.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        const QJsonObject packageHead = fixture.value(QStringLiteral("packageDetail"))
                                            .toObject()
                                            .value(QStringLiteral("dataStr"))
                                            .toObject()
                                            .value(QStringLiteral("head"))
                                            .toObject();
        QVERIFY(!packageHead.contains(QStringLiteral("uuid_3d")));

        EasyedaFootprintImporter importer;
        const QSharedPointer<FootprintData> footprint = importer.importFootprintData(fixture);

        QVERIFY(footprint);
        QCOMPARE(footprint->info().name, QStringLiteral("C0603"));
        QVERIFY(footprint->info().uuid3d.isEmpty());
        QCOMPARE(footprint->model3D().uuid(), QStringLiteral("ac9b32e974bc448eab36b1293f859dcb"));
        QCOMPARE(footprint->model3D().name(), QStringLiteral("C0603_L1.6-W0.8-H0.8"));
        QVERIFY(footprint->pads().size() >= 2);
        QVERIFY(footprint->outlines().size() >= 1);
    }

    // 验证真实三维模型夹具被识别为 OBJ 数据。
    void testRealModel3DFixtureIsObjData() {
        QString error;
        const QByteArray modelData =
            TestPaths::readBytes(TestPaths::fixturePath(QStringLiteral("easyeda/model3d_r0603.obj")), &error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(modelData.size() > 1024);
        QVERIFY(modelData.contains("v "));
        QVERIFY(modelData.contains("newmtl"));
    }

private:
    // 读取并解析测试夹具中的 JSON 对象。
    QJsonObject loadFixtureObject(const QString& relativePath, QString* errorMessage) const {
        const QByteArray bytes = TestPaths::readBytes(TestPaths::fixturePath(relativePath), errorMessage);
        if (errorMessage != nullptr && !errorMessage->isEmpty()) {
            return {};
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            if (errorMessage != nullptr) {
                *errorMessage = parseError.errorString();
            }
            return {};
        }
        if (!document.isObject()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Fixture root must be a JSON object");
            }
            return {};
        }
        return document.object();
    }
};

QTEST_GUILESS_MAIN(TestEasyedaImporterFixtures)
#include "test_easyeda_importer_fixtures.moc"
