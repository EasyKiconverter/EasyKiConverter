#include "core/pads/ExporterPadsFootprint.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestPadsExporter final : public QObject {
    Q_OBJECT

private slots:
    /** @brief 验证基本 PADS Decal 文件包含图元、焊盘栈和通孔层记录。 */
    void writesAsciiDecal();
    /** @brief 验证清洗后的封装名称冲突会阻止覆盖错误文件。 */
    void rejectsSanitizedNameCollision();
    /** @brief 验证无法无损表达的形状和机械孔不会被静默丢失。 */
    void rejectsUnsupportedGeometry();
};

static IR::FootprintComponentIR makeFixture(const QString& name) {
    IR::FootprintComponentIR footprint;
    footprint.name = name;

    IR::FootprintCircleIR circle;
    circle.center = QPointF(0.0, 0.0);
    circle.radius = 2.0;
    circle.strokeWidth = 0.15;
    footprint.circles.append(circle);

    IR::FootprintPadIR smd;
    smd.number = QStringLiteral("1");
    smd.position = QPointF(-1.0, 0.0);
    smd.shape = IR::PadShape::Rect;
    smd.size = QSizeF(1.0, 0.6);
    footprint.pads.append(smd);

    IR::FootprintPadIR through;
    through.number = QStringLiteral("2");
    through.position = QPointF(1.0, 0.0);
    through.padType = IR::PadType::ThroughHole;
    through.shape = IR::PadShape::Ellipse;
    through.size = QSizeF(1.6, 1.6);
    through.holeSize = 0.8;
    through.isPlated = false;
    footprint.pads.append(through);
    return footprint;
}

/** 验证 PADS ASCII Decal 输出的基本结构和单位标识。 */
void TestPadsExporter::writesAsciiDecal() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    ExporterPadsFootprint exporter;
    QVERIFY(exporter.exportFootprintLibrary(
        {makeFixture(QStringLiteral("QFN 4"))}, QStringLiteral("qfn"), temporary.path()));

    QFile file(QDir(temporary.path()).filePath(QStringLiteral("QFN_4.d")));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.startsWith(QStringLiteral("QFN_4 I 0 0")));
    QVERIFY(content.contains(QStringLiteral("TIMESTAMP ")));
    QVERIFY(content.contains(QStringLiteral("CIRCLE ")));
    QVERIFY(content.contains(QStringLiteral("PAD 1 1")));
    QVERIFY(content.contains(QStringLiteral("PAD 2 2")));
    QVERIFY(content.contains(QStringLiteral("-0 ")));
}

/** 验证两个不同原名清洗为同一 Decal 名称时会失败并返回诊断。 */
void TestPadsExporter::rejectsSanitizedNameCollision() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    ExporterPadsFootprint exporter;
    const QList<IR::FootprintComponentIR> footprints = {makeFixture(QStringLiteral("A B")),
                                                        makeFixture(QStringLiteral("A/B"))};
    QVERIFY(!exporter.exportFootprintLibrary(footprints, QStringLiteral("collision"), temporary.path()));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("冲突")));
}

/** 验证当前 PADS ASCII 后端拒绝无法无损表达的几何数据。 */
void TestPadsExporter::rejectsUnsupportedGeometry() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    IR::FootprintComponentIR footprint = makeFixture(QStringLiteral("unsupported"));
    footprint.pads[0].shape = IR::PadShape::RoundRect;
    footprint.holes.append({QPointF(0.0, 0.0), 0.5, false});

    ExporterPadsFootprint exporter;
    QVERIFY(!exporter.exportFootprintLibrary({footprint}, QStringLiteral("unsupported"), temporary.path()));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("机械孔")));
}

QTEST_MAIN(TestPadsExporter)
#include "test_pads_exporter.moc"
