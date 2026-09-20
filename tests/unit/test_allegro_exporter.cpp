#include "../common/TestPaths.hpp"
#include "core/allegro/AllegroLayerMapper.h"
#include "core/allegro/ExporterAllegroFootprint.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestAllegroExporter final : public QObject {
    Q_OBJECT

private slots:
    /** @brief 验证基本 Padstack 去重、Pin 关联、Place Bound 回退和 Import Package 文件。 */
    void packageContainsNormalizedFootprint();
    /** @brief 验证重复 Pin 编号会阻断导出，而不是生成歧义文件。 */
    void duplicatePinNumberFails();
    /** @brief 验证空编号焊盘不会被错误地写成无引用 Pin。 */
    void emptyPinNumberFails();
    /** @brief 验证未知层不会被静默映射到铜层。 */
    void unknownLayerProducesFailure();
    /** @brief 验证目标格式输出后缀和 Allegro 层语义映射。 */
    void targetContractIsExplicit();
};

static IR::FootprintComponentIR makeQfnFixture() {
    IR::FootprintComponentIR footprint;
    footprint.name = QStringLiteral("QFN-4_Unicode_测试");
    footprint.description = QStringLiteral("QFN fixture with exposed pad");
    footprint.shouldGenerateCourtyard = true;
    for (int i = 0; i < 4; ++i) {
        IR::FootprintPadIR pad;
        pad.number = QString::number(i + 1);
        pad.position = QPointF(i * 0.5, 0.0);
        pad.shape = IR::PadShape::RoundRect;
        pad.size = QSizeF(1.0, 0.3);
        pad.layer = IR::LayerType::TopCopper;
        footprint.pads.append(pad);
    }
    IR::FootprintPadIR exposed;
    exposed.number = QStringLiteral("EP");
    exposed.position = QPointF(0.75, 0.8);
    exposed.shape = IR::PadShape::Polygon;
    exposed.size = QSizeF(2.0, 2.0);
    exposed.customShapePoints = {QPointF(-1, -1), QPointF(1, -1), QPointF(1, 1), QPointF(-1, 1)};
    exposed.layer = IR::LayerType::TopCopper;
    footprint.pads.append(exposed);
    IR::FootprintPadIR mountingPad;
    mountingPad.number = QStringLiteral("MH1");
    mountingPad.position = QPointF(3.0, 0.0);
    mountingPad.shape = IR::PadShape::Ellipse;
    mountingPad.padType = IR::PadType::ThroughHole;
    mountingPad.size = QSizeF(2.0, 2.0);
    mountingPad.holeSize = 1.0;
    mountingPad.holeLength = 1.5;
    mountingPad.isPlated = false;
    mountingPad.layer = IR::LayerType::MultiLayer;
    footprint.pads.append(mountingPad);
    footprint.holes.append({QPointF(3.0, 3.0), 0.5, false});
    footprint.circles.append({QPointF(0, 0), 2.0, 0.1, IR::LayerType::TopSilk, false});
    footprint.texts.append({QStringLiteral("REF**"),
                            QPointF(0, -2.0),
                            0.0,
                            false,
                            0.1,
                            1.0,
                            IR::LayerType::TopSilk,
                            true,
                            false,
                            false,
                            {}});
    IR::Model3DIR model;
    model.setName(QStringLiteral("qfn.step"));
    model.setTranslation({0.1, 0.2, 0.3});
    model.setRotation({0.0, 0.0, 90.0});
    model.setStepOffsetMm({0.4, 0.5, 0.6});
    model.setStepData(QByteArray("ISO-10303-21 fixture"));
    footprint.models3d.append(model);
    return footprint;
}

/** 验证 QFN fixture 能生成完整的 Allegro Import Package。 */
void TestAllegroExporter::packageContainsNormalizedFootprint() {
    const QString fixturePath = Test::TestPaths::fixturePath(QStringLiteral("allegro/qfn56_fixture.json"));
    QFile fixture(fixturePath);
    QVERIFY2(fixture.open(QIODevice::ReadOnly), qPrintable(fixturePath));
    QVERIFY(!QJsonDocument::fromJson(fixture.readAll()).isNull());
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterAllegroFootprint exporter;
    const QString output = temporary.path() + QStringLiteral("/unsafe/../QFN package");
    QVERIFY(exporter.exportFootprintLibrary({makeQfnFixture()}, QStringLiteral("QFN/56"), output, false, true));
    QVERIFY(QFileInfo::exists(output + QStringLiteral("/manifest.json")));
    QVERIFY(QFileInfo::exists(output + QStringLiteral("/generator.il")));
    QVERIFY(QFileInfo::exists(output + QStringLiteral("/README_ALLEGRO.md")));
    QVERIFY(QDir(output + QStringLiteral("/normalized-data")).entryList(QDir::Files).first().contains("测试") == false);
    QVERIFY(QDir(output + QStringLiteral("/normalized-data")).entryList(QDir::Files).size() == 1);
    QVERIFY(QDir(output + QStringLiteral("/padstacks")).entryList(QDir::Files).size() == 3);
    QVERIFY(!exporter.diagnostics().isEmpty());
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("Place Bound")));
    QFile manifestFile(output + QStringLiteral("/manifest.json"));
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    QCOMPARE(manifest.value(QStringLiteral("native_database_generated")).toBool(), false);
    QCOMPARE(manifest.value(QStringLiteral("packages")).toArray().size(), 1);
}

/** 验证重复 Pin 编号会阻止产生歧义的目标包。 */
void TestAllegroExporter::duplicatePinNumberFails() {
    IR::FootprintComponentIR footprint;
    footprint.name = QStringLiteral("duplicate");
    footprint.pads = {IR::FootprintPadIR{}, IR::FootprintPadIR{}};
    footprint.pads[0].number = QStringLiteral("1");
    footprint.pads[1].number = QStringLiteral("1");
    QTemporaryDir temporary;
    ExporterAllegroFootprint exporter;
    QVERIFY(!exporter.exportFootprintLibrary({footprint}, QStringLiteral("duplicate"), temporary.path()));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("重复 Pin")));
}

/** 验证空 Pin 编号会被报告为不可恢复的关联错误。 */
void TestAllegroExporter::emptyPinNumberFails() {
    IR::FootprintComponentIR footprint;
    footprint.name = QStringLiteral("empty-pin");
    footprint.pads.append(IR::FootprintPadIR{});
    QTemporaryDir temporary;
    ExporterAllegroFootprint exporter;
    QVERIFY(!exporter.exportFootprintLibrary({footprint}, QStringLiteral("empty"), temporary.path()));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("空编号")));
}

/** 验证未知 IR 图层不会被静默映射到错误的 Allegro 层。 */
void TestAllegroExporter::unknownLayerProducesFailure() {
    IR::FootprintComponentIR footprint;
    footprint.name = QStringLiteral("unknown-layer");
    IR::FootprintPadIR pad;
    pad.number = QStringLiteral("1");
    footprint.pads.append(pad);
    footprint.regions.append({{QPointF(0, 0), QPointF(1, 0), QPointF(1, 1)}, IR::LayerType::UserDefined, false, false});
    QTemporaryDir temporary;
    ExporterAllegroFootprint exporter;
    QVERIFY(!exporter.exportFootprintLibrary({footprint}, QStringLiteral("unknown"), temporary.path()));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("无法映射")));
}

/** 验证 Allegro 导出器的目录契约和核心层映射语义。 */
void TestAllegroExporter::targetContractIsExplicit() {
    ExporterAllegroFootprint exporter;
    QCOMPARE(exporter.libraryFileExtension(), QStringLiteral("_Allegro"));
    QVERIFY(exporter.isDirectoryOutput());
    QVERIFY(AllegroLayerMapper::map(IR::LayerType::TopSilk).has_value());
    QCOMPARE(AllegroLayerMapper::map(IR::LayerType::TopSilk)->subclassName, QStringLiteral("SILKSCREEN_TOP"));
    QVERIFY(!AllegroLayerMapper::map(IR::LayerType::Unknown).has_value());
}

QTEST_MAIN(TestAllegroExporter)
#include "test_allegro_exporter.moc"
