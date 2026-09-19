#include "core/parser/GeometryTransforms.h"
#include "core/parser/ParseDiagnostics.h"
#include "core/parser/TextParsers.h"
#include "core/parser/XpeditionHkpReader.h"
#include "tests/common/TestPaths.hpp"

#include <QTest>

using namespace EasyKiConverter::Parser;

class TestTextParsers : public QObject {
    Q_OBJECT

private slots:

    // 验证点缩进 HKP 节点可以保留层级、值和行号。
    void parsesIndentedSections() {
        ParseDiagnostics diagnostics;
        const QList<SectionNode> roots = IndentedSectionParser::parse(
            QStringLiteral("!.HEADER\n.FILETYPE CELL_LIBRARY\n.PACKAGE_CELL \"U1\"\n..PIN \"1\"\n...XY (0, 0)\n"),
            &diagnostics);

        QCOMPARE(roots.size(), 2);
        QCOMPARE(roots.at(1).keyword, QStringLiteral("PACKAGE_CELL"));
        QCOMPARE(roots.at(1).value, QStringLiteral("\"U1\""));
        QCOMPARE(roots.at(1).children.first().keyword, QStringLiteral("PIN"));
        QCOMPARE(roots.at(1).children.first().children.first().keyword, QStringLiteral("XY"));
        QVERIFY(diagnostics.isEmpty());
    }

    // 验证非法数字不会静默转为零，并且会保留字段和行号诊断。
    void rejectsInvalidNumbers() {
        ParseDiagnostics diagnostics;
        QCOMPARE(
            StrictNumberParser::parseDouble(QStringLiteral("not-a-number"), &diagnostics, QStringLiteral("WIDTH"), 7),
            0.0);
        QVERIFY(diagnostics.hasErrors());
        QCOMPARE(diagnostics.items().first().entity, QStringLiteral("WIDTH"));
        QCOMPARE(diagnostics.items().first().line, 7);
    }

    // 验证嵌套 S-expression 和引号内容能够被分层保留。
    void parsesSExpressions() {
        ParseDiagnostics diagnostics;
        const QList<SExpressionNode> roots =
            SExpressionParser::parse(QStringLiteral("(symbol (name \"R1\") (at 1.0 2.0))"), &diagnostics);

        QCOMPARE(roots.size(), 1);
        QCOMPARE(roots.first().atom, QStringLiteral("symbol"));
        QCOMPARE(roots.first().children.first().atom, QStringLiteral("name"));
        QCOMPARE(roots.first().children.first().children.first().atom, QStringLiteral("R1"));
        QVERIFY(diagnostics.isEmpty());
    }

    // 验证括号损坏会返回诊断而不是让解析过程无提示结束。
    void reportsMalformedSExpression() {
        ParseDiagnostics diagnostics;
        const QList<SExpressionNode> roots =
            SExpressionParser::parse(QStringLiteral("(symbol (name \"R1\")"), &diagnostics);
        QVERIFY(!roots.isEmpty());
        QVERIFY(diagnostics.hasErrors());
    }

    // 验证常见输入扩展名和头部可以被识别，未知内容保持 Unknown。
    void detectsFormats() {
        QCOMPARE(FormatDetector::detect(QStringLiteral("pads.psk.hkp"), QByteArray()), DetectedFormat::XpeditionHkp);
        QCOMPARE(FormatDetector::detect(QStringLiteral("symbol.1"), QByteArray("V 50\n")),
                 DetectedFormat::XpeditionSymbol);
        QCOMPARE(FormatDetector::detect(QStringLiteral("design.dsn"), QByteArray()), DetectedFormat::TinyCadXml);
        QCOMPARE(FormatDetector::detect(QStringLiteral("unknown.bin"), QByteArray("binary")), DetectedFormat::Unknown);
    }

    // 验证词法单元保留引号、行号和列号，供多种文本格式共享。
    void tokenizesWithSourceLocations() {
        ParseDiagnostics diagnostics;
        const QList<TextToken> tokens = TextTokenizer::tokenize(QStringLiteral("PAD \"A 1\"\nWIDTH 0.5"), &diagnostics);
        QCOMPARE(tokens.size(), 4);
        QCOMPARE(tokens.at(1).text, QStringLiteral("A 1"));
        QVERIFY(tokens.at(1).quoted);
        QCOMPARE(tokens.at(1).line, 1);
        QCOMPARE(tokens.at(3).line, 2);
        QVERIFY(diagnostics.isEmpty());
    }

    // 验证毫米、mil、英寸换算以及旋转镜像保持可重复的几何结果。
    void convertsUnitsAndCoordinates() {
        QCOMPARE(UnitConverter::toMillimeters(10.0, LengthUnit::Mil), 0.254);
        QCOMPARE(UnitConverter::toMillimeters(1.0, LengthUnit::Inch), 25.4);
        const QPointF transformed = CoordinateTransform::apply(QPointF(1.0, 0.0), QPointF(0.0, 0.0), 90.0);
        QVERIFY(qAbs(transformed.x()) < 1e-9);
        QVERIFY(qAbs(transformed.y() - 1.0) < 1e-9);
    }

    // 验证 Xpedition HKP 文件头和分层节点能够进入格式专用读取模型。
    void readsXpeditionHkpHeader() {
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(".FILETYPE CELL_LIBRARY\n.UNITS mm\n.PACKAGE_CELL \"QFN\"\n..PIN \"1\"\n"),
            QStringLiteral("sample.cel.hkp"));
        QVERIFY(document.isRecognized());
        QCOMPARE(document.type, XpeditionHkpType::CellLibrary);
        QCOMPARE(document.unit, LengthUnit::Millimeter);
        QCOMPARE(document.sections.size(), 3);
        QVERIFY(!document.diagnostics.hasErrors());
    }

    // 验证空 HKP 文件以错误诊断结束，而不是被当作空库成功导入。
    void rejectsEmptyXpeditionHkp() {
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(QString(), QStringLiteral("empty.hkp"));
        QVERIFY(!document.isRecognized());
        QVERIFY(document.diagnostics.hasErrors());
    }

    // 验证 Pad、Padstack、Cell 和引脚关联可以从真实 HKP 语法进入格式模型。
    void parsesXpeditionCellLibrary() {
        const QString content = QStringLiteral(
            ".UNITS mm\n"
            ".PAD \"SMD\"\n"
            "..RECTANGLE\n"
            "...WIDTH 1.5\n"
            "...HEIGHT 1.3\n"
            ".PADSTACK \"P1\"\n"
            "..PADSTACK_TYPE PIN_SMD\n"
            "..TECHNOLOGY\n"
            "...TOP_PAD \"SMD\"\n"
            ".PACKAGE_CELL \"C1\"\n"
            "..PIN \"1\"\n"
            "...XY (1, -2)\n"
            "...PADSTACK \"P1\"\n"
            "..SILKSCREEN_OUTLINE\n"
            "...RECT_SHAPE\n"
            "....XY (-1, -1)\n"
            "....XY (1, 1)\n");
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(content, QStringLiteral("sample.cel.hkp"));
        QCOMPARE(document.model.pads.size(), 1);
        QCOMPARE(document.model.padstacks.size(), 1);
        QCOMPARE(document.model.cells.size(), 1);
        QCOMPARE(document.model.cells.first().pins.size(), 1);
        QCOMPARE(document.model.cells.first().pins.first().position, QPointF(1.0, -2.0));
        QCOMPARE(document.model.cells.first().outlines.first().points.size(), 2);
        QVERIFY(!document.diagnostics.hasErrors());
    }

    // 验证仓库内的真实格式样本可以通过统一测试路径读取并解析。
    void parsesXpeditionFixture() {
        QString error;
        const QString content = EasyKiConverter::Test::TestPaths::readText(
            EasyKiConverter::Test::TestPaths::fixturePath(QStringLiteral("xpedition/Sample.CEL.HKP")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(content, QStringLiteral("Sample.CEL.HKP"));
        QCOMPARE(document.type, XpeditionHkpType::CellLibrary);
        QCOMPARE(document.model.cells.size(), 1);
        QCOMPARE(document.model.cells.first().pins.size(), 2);
    }

    // 验证 PDB 器件的名称、封装、符号和属性关联可以被严格保留。
    void parsesXpeditionPartsDatabaseFixture() {
        QString error;
        const QString content = EasyKiConverter::Test::TestPaths::readText(
            EasyKiConverter::Test::TestPaths::fixturePath(QStringLiteral("xpedition/xpedition_device.pdb.hkp")),
            &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(content, QStringLiteral("device.pdb.hkp"));
        QCOMPARE(document.type, XpeditionHkpType::PartsDatabase);
        QCOMPARE(document.model.parts.size(), 1);
        QCOMPARE(document.model.parts.first().topCell, QStringLiteral("CC1206"));
        QCOMPARE(document.model.parts.first().bottomCell, QStringLiteral("CC1206_BOTTOM"));
        QCOMPARE(document.model.parts.first().symbol, QStringLiteral("Sample:resistor"));
        QCOMPARE(document.model.parts.first().properties.value(QStringLiteral("Type")), QStringLiteral("Resistor"));
    }

    // 验证非法坐标和缺失关联会产生可观察诊断，而不是静默生成零坐标。
    void reportsXpeditionAssociationErrors() {
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(".UNITS mm\n.PADSTACK \"P1\"\n..TECHNOLOGY\n...TOP_PAD \"MISSING\"\n"
                           ".PACKAGE_CELL \"C1\"\n..PIN \"1\"\n...XY (bad, 2)\n...PADSTACK \"P1\"\n"),
            QStringLiteral("broken.cel.hkp"));
        QVERIFY(document.diagnostics.hasErrors());
        QVERIFY(document.diagnostics.items().size() >= 2);
    }

    // 验证旋转和镜像字段会进入引脚模型并保留几何语义。
    void parsesXpeditionPinTransform() {
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(
                ".UNITS mm\n.PACKAGE_CELL \"C1\"\n..PIN \"1\"\n...XY (1, 2)\n...ROTATION 90\n...MIRROR YES\n"),
            QStringLiteral("transform.cel.hkp"));
        QCOMPARE(document.model.cells.first().pins.first().rotation, 90.0);
        QVERIFY(document.model.cells.first().pins.first().mirror);
    }

    // 验证未知几何关键字被跳过并记录原因，不会静默伪造形状。
    void reportsUnknownXpeditionPrimitive() {
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(".UNITS mm\n.PAD \"P\"\n..TRIANGLE\n...WIDTH 1\n"), QStringLiteral("unknown.psk.hkp"));
        QVERIFY(!document.diagnostics.isEmpty());
        QCOMPARE(document.model.pads.first().shape, XpeditionPadShape::Unknown);
    }

    // 验证重复名称会保留数据并生成稳定的重名诊断。
    void disambiguatesXpeditionDuplicateNames() {
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(".UNITS mm\n.PAD \"P\"\n..ROUND\n...DIAMETER 1\n.PAD \"P\"\n..ROUND\n...DIAMETER 2\n"),
            QStringLiteral("duplicate.psk.hkp"));
        QCOMPARE(document.model.pads.size(), 2);
        QCOMPARE(document.model.pads.at(1).name, QStringLiteral("P_2"));
        QVERIFY(!document.diagnostics.isEmpty());
    }

    // 验证 Cadstar 的 END* 分段和单行叶节点可以建立可遍历的树。
    void parsesDelimitedSections() {
        ParseDiagnostics diagnostics;
        const QList<DelimitedSectionNode> roots = DelimitedSectionParser::parse(
            QStringLiteral(
                "PAD \"P1\"\nSHAPE ROUND\nDIAMETER 1.0\nENDPAD\nPACKAGE \"C1\"\nPIN 1 (0 0) P1\nENDPACKAGE\n"),
            {QStringLiteral("SHAPE"), QStringLiteral("DIAMETER"), QStringLiteral("PIN")},
            &diagnostics);
        QCOMPARE(roots.size(), 2);
        QCOMPARE(roots.first().keyword, QStringLiteral("PAD"));
        QCOMPARE(roots.first().children.size(), 2);
        QCOMPARE(roots.first().children.first().arguments.first(), QStringLiteral("ROUND"));
        QVERIFY(roots.first().closed);
        QVERIFY(!diagnostics.hasErrors());
    }

    // 验证孤立和未闭合的 Cadstar 分段会产生可定位诊断。
    void reportsDelimitedSectionErrors() {
        ParseDiagnostics diagnostics;
        const QList<DelimitedSectionNode> roots = DelimitedSectionParser::parse(
            QStringLiteral("PAD \"P1\"\nSHAPE ROUND\nENDUNKNOWN\nENDPAD\n"), {QStringLiteral("SHAPE")}, &diagnostics);
        QVERIFY(!roots.isEmpty());
        QVERIFY(!diagnostics.isEmpty());
    }
};

QTEST_MAIN(TestTextParsers)
#include "test_text_parsers.moc"
