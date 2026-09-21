#include "core/eagle/ExporterEagleFootprint.h"

#include <QFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QtTest>

using namespace EasyKiConverter;

class TestEagleExporter final : public QObject {
    Q_OBJECT

private slots:
    /** @brief 验证 Eagle XML library 包含 package、Pad 和独立机械孔。 */
    void writesXmlLibrary();
    /** @brief 验证清洗名称冲突会阻止生成歧义 package。 */
    void rejectsNameCollision();
};

static IR::FootprintComponentIR makeFixture(const QString& name) {
    IR::FootprintComponentIR footprint;
    footprint.name = name;
    footprint.description = QStringLiteral("Eagle XML fixture");

    IR::FootprintPadIR smd;
    smd.number = QStringLiteral("1");
    smd.position = QPointF(-1.0, 0.0);
    smd.shape = IR::PadShape::RoundRect;
    smd.size = QSizeF(1.2, 0.8);
    footprint.pads.append(smd);

    IR::FootprintPadIR through;
    through.number = QStringLiteral("2");
    through.position = QPointF(1.0, 0.0);
    through.padType = IR::PadType::ThroughHole;
    through.shape = IR::PadShape::Ellipse;
    through.size = QSizeF(1.8, 1.8);
    through.holeSize = 0.9;
    footprint.pads.append(through);
    footprint.holes.append({QPointF(0.0, 2.0), 0.4, false});
    footprint.circles.append({QPointF(0.0, 0.0), 2.0, 0.15, IR::LayerType::TopSilk, false});
    return footprint;
}

/** 验证 Eagle XML library 可解析且包含基础封装元素。 */
void TestEagleExporter::writesXmlLibrary() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterEagleFootprint exporter;
    const QString path = temporary.path() + QStringLiteral("/library.lbr");
    QVERIFY(exporter.exportFootprintLibrary({makeFixture(QStringLiteral("QFN 4"))}, QStringLiteral("library"), path));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader reader(&file);
    bool packageSeen = false;
    bool smdSeen = false;
    bool padSeen = false;
    bool holeSeen = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement())
            continue;
        if (reader.name() == QStringLiteral("package"))
            packageSeen = true;
        if (reader.name() == QStringLiteral("smd"))
            smdSeen = true;
        if (reader.name() == QStringLiteral("pad"))
            padSeen = true;
        if (reader.name() == QStringLiteral("hole"))
            holeSeen = true;
    }
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    QVERIFY(packageSeen);
    QVERIFY(smdSeen);
    QVERIFY(padSeen);
    QVERIFY(holeSeen);
}

/** 验证不同原名清洗为同一 package 名称时会失败。 */
void TestEagleExporter::rejectsNameCollision() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterEagleFootprint exporter;
    QVERIFY(!exporter.exportFootprintLibrary({makeFixture(QStringLiteral("A B")), makeFixture(QStringLiteral("A/B"))},
                                             QStringLiteral("library"),
                                             temporary.path() + QStringLiteral("/library.lbr")));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("冲突")));
}

QTEST_MAIN(TestEagleExporter)
#include "test_eagle_exporter.moc"
