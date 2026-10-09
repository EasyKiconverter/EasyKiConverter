#include "core/parser/GeometryTransforms.h"
#include "core/parser/ParseDiagnostics.h"
#include "core/parser/TextParsers.h"
#include "core/parser/XpeditionHkpReader.h"
#include "core/parser/XpeditionSymbolModel.h"
#include "tests/common/TestPaths.hpp"

#include <QTest>

using namespace EasyKiConverter::Parser;

class TestTextParsers : public QObject {
    Q_OBJECT

private slots:

    // 验证点缩进 HKP 节点可以保留层级、值和行号。
    /**
     * @brief 验证 parsesIndentedSections 测试场景。
     */
    void parsesIndentedSections() {
        // diagnostics：导出过程中收集的诊断信息。
        ParseDiagnostics diagnostics;
        // roots：测试使用的文件系统路径。
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
    /**
     * @brief 验证 rejectsInvalidNumbers 测试场景。
     */
    void rejectsInvalidNumbers() {
        // diagnostics：导出过程中收集的诊断信息。
        ParseDiagnostics diagnostics;
        QCOMPARE(
            StrictNumberParser::parseDouble(QStringLiteral("not-a-number"), &diagnostics, QStringLiteral("WIDTH"), 7),
            0.0);
        QVERIFY(diagnostics.hasErrors());
        QCOMPARE(diagnostics.items().first().entity, QStringLiteral("WIDTH"));
        QCOMPARE(diagnostics.items().first().line, 7);
    }

    // 验证嵌套 S-expression 和引号内容能够被分层保留。
    /**
     * @brief 验证 parsesSExpressions 测试场景。
     */
    void parsesSExpressions() {
        // diagnostics：导出过程中收集的诊断信息。
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
    /**
     * @brief 验证 reportsMalformedSExpression 测试场景。
     */
    void reportsMalformedSExpression() {
        // diagnostics：导出过程中收集的诊断信息。
        ParseDiagnostics diagnostics;
        const QList<SExpressionNode> roots =
            SExpressionParser::parse(QStringLiteral("(symbol (name \"R1\")"), &diagnostics);
        QVERIFY(!roots.isEmpty());
        QVERIFY(diagnostics.hasErrors());
    }

    // 验证无参数列表不会被误判为普通原子节点。
    /**
     * @brief 验证 preservesEmptySExpressionLists 测试场景。
     */
    void preservesEmptySExpressionLists() {
        // diagnostics：导出过程中收集的诊断信息。
        ParseDiagnostics diagnostics;
        // roots：测试使用的文件系统路径。
        const QList<SExpressionNode> roots = SExpressionParser::parse(QStringLiteral("(FLAG) (VALUE 1)"), &diagnostics);
        QCOMPARE(roots.size(), 2);
        QVERIFY(roots.at(0).isList());
        QCOMPARE(roots.at(0).atom, QStringLiteral("FLAG"));
        QVERIFY(roots.at(1).isList());
        QVERIFY(!roots.at(1).children.first().isList());
        QVERIFY(diagnostics.isEmpty());
    }

    // 验证常见输入扩展名和头部可以被识别，未知内容保持 Unknown。
    /**
     * @brief 验证 detectsFormats 测试场景。
     */
    void detectsFormats() {
        QCOMPARE(FormatDetector::detect(QStringLiteral("pads.psk.hkp"), QByteArray()), DetectedFormat::XpeditionHkp);
        QCOMPARE(FormatDetector::detect(QStringLiteral("symbol.1"), QByteArray("V 50\n")),
                 DetectedFormat::XpeditionSymbol);
        QCOMPARE(FormatDetector::detect(QStringLiteral("design.dsn"), QByteArray()), DetectedFormat::TinyCadXml);
        QCOMPARE(FormatDetector::detect(QStringLiteral("design.pcb"), QByteArray("ACCEL_ASCII\n")),
                 DetectedFormat::PcadSExpression);
        QCOMPARE(FormatDetector::detect(QStringLiteral("board.txt"), QByteArray("# comment\nPCB[0 0]\n")),
                 DetectedFormat::GedaLegacy);
        QCOMPARE(FormatDetector::detect(QStringLiteral("board.pcb"), QByteArray("pcb(\"board\")\n")),
                 DetectedFormat::GedaLegacy);
        QCOMPARE(FormatDetector::detect(QStringLiteral("library.cpa"), QByteArray("cadstar\n")),
                 DetectedFormat::CadstarAscii);
        QCOMPARE(FormatDetector::detect(QStringLiteral("legacy.lib"), QByteArray(".LIB\n")), DetectedFormat::Unknown);
        QCOMPARE(FormatDetector::detect(QStringLiteral("unknown.bin"), QByteArray("binary")), DetectedFormat::Unknown);
    }

    // 验证词法单元保留引号、行号和列号，供多种文本格式共享。
    /**
     * @brief 验证 tokenizesWithSourceLocations 测试场景。
     */
    void tokenizesWithSourceLocations() {
        // diagnostics：导出过程中收集的诊断信息。
        ParseDiagnostics diagnostics;
        // tokens：测试解析得到的条目集合。
        const QList<TextToken> tokens = TextTokenizer::tokenize(QStringLiteral("PAD \"A 1\"\nWIDTH 0.5"), &diagnostics);
        QCOMPARE(tokens.size(), 4);
        QCOMPARE(tokens.at(1).text, QStringLiteral("A 1"));
        QVERIFY(tokens.at(1).quoted);
        QCOMPARE(tokens.at(1).line, 1);
        QCOMPARE(tokens.at(3).line, 2);
        QVERIFY(diagnostics.isEmpty());
    }

    // 验证毫米、mil、英寸换算以及旋转镜像保持可重复的几何结果。
    /**
     * @brief 验证 convertsUnitsAndCoordinates 测试场景。
     */
    void convertsUnitsAndCoordinates() {
        QCOMPARE(UnitConverter::toMillimeters(10.0, LengthUnit::Mil), 0.254);
        QCOMPARE(UnitConverter::toMillimeters(1.0, LengthUnit::Inch), 25.4);
        // transformed：变换后的几何数据。
        const QPointF transformed = CoordinateTransform::apply(QPointF(1.0, 0.0), QPointF(0.0, 0.0), 90.0);
        QVERIFY(qAbs(transformed.x()) < 1e-9);
        QVERIFY(qAbs(transformed.y() - 1.0) < 1e-9);
    }

    // 验证 Xpedition HKP 文件头和分层节点能够进入格式专用读取模型。
    /**
     * @brief 验证 readsXpeditionHkpHeader 测试场景。
     */
    void readsXpeditionHkpHeader() {
        // document：测试用文档对象。
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
    /**
     * @brief 验证 rejectsEmptyXpeditionHkp 测试场景。
     */
    void rejectsEmptyXpeditionHkp() {
        // document：测试用文档对象。
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(QString(), QStringLiteral("empty.hkp"));
        QVERIFY(!document.isRecognized());
        QVERIFY(document.diagnostics.hasErrors());
    }

    // 验证 Pad、Padstack、Cell 和引脚关联可以从真实 HKP 语法进入格式模型。
    /**
     * @brief 验证 parsesXpeditionCellLibrary 测试场景。
     */
    void parsesXpeditionCellLibrary() {
        // content：测试使用的文本或数据内容。
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
        // document：测试用文档对象。
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
    /**
     * @brief 验证 parsesXpeditionFixture 测试场景。
     */
    void parsesXpeditionFixture() {
        // error：测试期间收集的错误或诊断信息。
        QString error;
        // content：测试使用的文本或数据内容。
        const QString content = EasyKiConverter::Test::TestPaths::readText(
            EasyKiConverter::Test::TestPaths::fixturePath(QStringLiteral("xpedition/Sample.CEL.HKP")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        // document：测试用文档对象。
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(content, QStringLiteral("Sample.CEL.HKP"));
        QCOMPARE(document.type, XpeditionHkpType::CellLibrary);
        QCOMPARE(document.model.cells.size(), 1);
        QCOMPARE(document.model.cells.first().pins.size(), 2);
    }

    // 验证 PDB 器件的名称、封装、符号和属性关联可以被严格保留。
    /**
     * @brief 验证 parsesXpeditionPartsDatabaseFixture 测试场景。
     */
    void parsesXpeditionPartsDatabaseFixture() {
        // error：测试期间收集的错误或诊断信息。
        QString error;
        // content：测试使用的文本或数据内容。
        const QString content = EasyKiConverter::Test::TestPaths::readText(
            EasyKiConverter::Test::TestPaths::fixturePath(QStringLiteral("xpedition/xpedition_device.pdb.hkp")),
            &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        // document：测试用文档对象。
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(content, QStringLiteral("device.pdb.hkp"));
        QCOMPARE(document.type, XpeditionHkpType::PartsDatabase);
        QCOMPARE(document.model.parts.size(), 1);
        QCOMPARE(document.model.parts.first().topCell, QStringLiteral("CC1206"));
        QCOMPARE(document.model.parts.first().bottomCell, QStringLiteral("CC1206_BOTTOM"));
        QCOMPARE(document.model.parts.first().symbol, QStringLiteral("Sample:resistor"));
        QCOMPARE(document.model.parts.first().properties.value(QStringLiteral("Type")), QStringLiteral("Resistor"));
    }

    // 验证 PDB 的标签、符号引脚名称和 Slot 引脚编号按层级保留。
    /**
     * @brief 验证 parsesXpeditionPartDetails 测试场景。
     */
    void parsesXpeditionPartDetails() {
        // document：测试用文档对象。
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(".FILETYPE ASCII_PDB\n.Number \"U1\"\n..Label \"DUAL\"\n"
                           "..Symbol \"Logic:dual\"\n...PinName \"A\"\n...PinName \"B\"\n"
                           "..Slots\n...SlotID 1\n....PinNumber \"1\"\n....PinNumber \"2\"\n"),
            QStringLiteral("details.pdb.hkp"));
        QCOMPARE(document.model.parts.size(), 1);
        // part：测试使用的元器件标识或名称。
        const XpeditionPartDefinition& part = document.model.parts.first();
        QCOMPARE(part.label, QStringLiteral("DUAL"));
        QCOMPARE(part.symbolPinNames, QStringList({QStringLiteral("A"), QStringLiteral("B")}));
        QCOMPARE(part.slotPinNumbers.size(), 1);
        QCOMPARE(part.slotPinNumbers.first(), QStringList({QStringLiteral("1"), QStringLiteral("2")}));
    }

    // 验证非法坐标和缺失关联会产生可观察诊断，而不是静默生成零坐标。
    /**
     * @brief 验证 reportsXpeditionAssociationErrors 测试场景。
     */
    void reportsXpeditionAssociationErrors() {
        // document：测试用文档对象。
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(".UNITS mm\n.PADSTACK \"P1\"\n..TECHNOLOGY\n...TOP_PAD \"MISSING\"\n"
                           ".PACKAGE_CELL \"C1\"\n..PIN \"1\"\n...XY (bad, 2)\n...PADSTACK \"P1\"\n"),
            QStringLiteral("broken.cel.hkp"));
        QVERIFY(document.diagnostics.hasErrors());
        QVERIFY(document.diagnostics.items().size() >= 2);
    }

    // 验证旋转和镜像字段会进入引脚模型并保留几何语义。
    /**
     * @brief 验证 parsesXpeditionPinTransform 测试场景。
     */
    void parsesXpeditionPinTransform() {
        // document：测试用文档对象。
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(
                ".UNITS mm\n.PACKAGE_CELL \"C1\"\n..PIN \"1\"\n...XY (1, 2)\n...ROTATION 90\n...MIRROR YES\n"),
            QStringLiteral("transform.cel.hkp"));
        QCOMPARE(document.model.cells.first().pins.first().rotation, 90.0);
        QVERIFY(document.model.cells.first().pins.first().mirror);
    }

    // 验证未知几何关键字被跳过并记录原因，不会静默伪造形状。
    /**
     * @brief 验证 reportsUnknownXpeditionPrimitive 测试场景。
     */
    void reportsUnknownXpeditionPrimitive() {
        // document：测试用文档对象。
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(".UNITS mm\n.PAD \"P\"\n..TRIANGLE\n...WIDTH 1\n"), QStringLiteral("unknown.psk.hkp"));
        QVERIFY(!document.diagnostics.isEmpty());
        QCOMPARE(document.model.pads.first().shape, XpeditionPadShape::Unknown);
    }

    // 验证 HKP 的无层级坐标续行会合并到同一个 XY 节点并保留全部多边形点。
    /**
     * @brief 验证 parsesXpeditionContinuationPoints 测试场景。
     */
    void parsesXpeditionContinuationPoints() {
        // document：测试用文档对象。
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(".UNITS mm\n.PAD \"POLYGON\"\n..POLYGON\n...XY (0, 0)\n    (1, 0)\n    (1, 1)\n"),
            QStringLiteral("continuation.psk.hkp"));
        QCOMPARE(document.model.pads.size(), 1);
        QCOMPARE(document.model.pads.first().polygon.size(), 3);
        QCOMPARE(document.model.pads.first().polygon.at(2), QPointF(1.0, 1.0));
        QVERIFY(!document.diagnostics.hasErrors());
    }

    // 验证单点坐标同时支持括号、逗号和空格分隔形式。
    /**
     * @brief 验证 parsesXpeditionCoordinateSeparators 测试场景。
     */
    void parsesXpeditionCoordinateSeparators() {
        const XpeditionHkpDocument document =
            XpeditionHkpReader::parse(QStringLiteral(".UNITS mm\n.PAD \"P\"\n..POLYGON\n...XY 1, 2\n...XY (3 4)\n"
                                                     ".PACKAGE_CELL \"C\"\n..PIN \"1\"\n...XY (5 6)\n"),
                                      QStringLiteral("coordinate-separators.cel.hkp"));
        QCOMPARE(document.model.pads.first().polygon.size(), 2);
        QCOMPARE(document.model.pads.first().polygon.at(0), QPointF(1.0, 2.0));
        QCOMPARE(document.model.pads.first().polygon.at(1), QPointF(3.0, 4.0));
        QCOMPARE(document.model.cells.first().pins.first().position, QPointF(5.0, 6.0));
        QVERIFY(!document.diagnostics.hasErrors());
    }

    // 验证多点坐标中混入非法点时保留合法点但必然生成错误诊断。
    /**
     * @brief 验证 reportsInvalidXpeditionContinuationPoint 测试场景。
     */
    void reportsInvalidXpeditionContinuationPoint() {
        // document：测试用文档对象。
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(".UNITS mm\n.PAD \"POLYGON\"\n..POLYGON\n...XY (0, 0) (bad, 1) (1, 1)\n"),
            QStringLiteral("invalid-point.psk.hkp"));
        QCOMPARE(document.model.pads.first().polygon.size(), 2);
        QVERIFY(document.diagnostics.hasErrors());
    }

    // 验证 Xpedition V54 符号的引脚、图形、文本和多部件元数据可以被保留。
    /**
     * @brief 验证 parsesXpeditionSymbolV54 测试场景。
     */
    void parsesXpeditionSymbolV54() {
        // error：测试期间收集的错误或诊断信息。
        QString error;
        // content：测试使用的文本或数据内容。
        const QString content = EasyKiConverter::Test::TestPaths::readText(
            EasyKiConverter::Test::TestPaths::fixturePath(QStringLiteral("xpedition/Sample.SYM.1")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        // document：测试用文档对象。
        const XpeditionSymbolDocument document = XpeditionSymbolParser::parse(content, QStringLiteral("Sample.SYM.1"));
        QVERIFY(document.isRecognized());
        QVERIFY(!document.diagnostics.hasErrors());
        QCOMPARE(document.model.name, QStringLiteral("TEST_SYMBOL"));
        QCOMPARE(document.model.partCount, 2);
        QCOMPARE(document.model.pins.size(), 1);
        QCOMPARE(document.model.pins.first().name, QStringLiteral("INPUT"));
        QCOMPARE(document.model.pins.first().numbers, QStringList{QStringLiteral("A1")});
        QCOMPARE(document.model.polylines.size(), 1);
        QVERIFY(document.model.polylines.first().closed);
        QCOMPARE(document.model.rectangles.size(), 1);
        QCOMPARE(document.model.circles.size(), 1);
        QCOMPARE(document.model.arcs.size(), 1);
        QCOMPARE(document.model.texts.size(), 1);
    }

    // 验证 Xpedition 符号非法数字和未知命令会产生可观察诊断。
    /**
     * @brief 验证 reportsInvalidXpeditionSymbolFields 测试场景。
     */
    void reportsInvalidXpeditionSymbolFields() {
        // document：测试用文档对象。
        const XpeditionSymbolDocument document = XpeditionSymbolParser::parse(
            QStringLiteral("V invalid\nP 1 bad 0 0 0 0 0 0\nQ unsupported\n"), QStringLiteral("broken.1"));
        QVERIFY(document.diagnostics.hasErrors());
        QVERIFY(!document.diagnostics.isEmpty());
    }

    // 验证 Pad 和孔的几何节点顺序变化不会阻止解析器找到受支持的图元。
    /**
     * @brief 验证 parsesXpeditionGeometryAfterUnknownNode 测试场景。
     */
    void parsesXpeditionGeometryAfterUnknownNode() {
        // document：测试用文档对象。
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(".UNITS mm\n.PAD \"P\"\n..UNSUPPORTED\n..RECTANGLE\n...WIDTH 1\n...HEIGHT 2\n"
                           ".HOLE \"H\"\n..UNSUPPORTED\n..ROUND\n...DIAMETER 0.5\n"),
            QStringLiteral("geometry-order.psk.hkp"));
        QCOMPARE(document.model.pads.first().shape, XpeditionPadShape::Rectangle);
        QCOMPARE(document.model.holes.first().shape, XpeditionPadShape::Round);
        QCOMPARE(document.model.pads.first().size, QSizeF(1.0, 2.0));
        QVERIFY(!document.diagnostics.hasErrors());
    }

    // 验证重复名称会保留数据并生成稳定的重名诊断。
    /**
     * @brief 验证 disambiguatesXpeditionDuplicateNames 测试场景。
     */
    void disambiguatesXpeditionDuplicateNames() {
        // document：测试用文档对象。
        const XpeditionHkpDocument document = XpeditionHkpReader::parse(
            QStringLiteral(".UNITS mm\n.PAD \"P\"\n..ROUND\n...DIAMETER 1\n.PAD \"P\"\n..ROUND\n...DIAMETER 2\n"),
            QStringLiteral("duplicate.psk.hkp"));
        QCOMPARE(document.model.pads.size(), 2);
        QCOMPARE(document.model.pads.at(1).name, QStringLiteral("P_2"));
        QVERIFY(!document.diagnostics.isEmpty());
    }

    // 验证重复 Pad 和 Cell 的原始引用会报告歧义，而不会静默绑定到首个定义。
    /**
     * @brief 验证 reportsAmbiguousXpeditionReferences 测试场景。
     */
    void reportsAmbiguousXpeditionReferences() {
        const XpeditionHkpDocument document =
            XpeditionHkpReader::parse(QStringLiteral(".UNITS mm\n"
                                                     ".PAD \"P\"\n..ROUND\n...DIAMETER 1\n"
                                                     ".PAD \"P\"\n..ROUND\n...DIAMETER 2\n"
                                                     ".PADSTACK \"S\"\n..TECHNOLOGY\n...TOP_PAD \"P\"\n"
                                                     ".PACKAGE_CELL \"C\"\n..PIN \"1\"\n...PADSTACK \"S\"\n"
                                                     ".PACKAGE_CELL \"C\"\n..PIN \"2\"\n...PADSTACK \"S\"\n"
                                                     ".NUMBER \"U1\"\n..TOPCELL \"C\"\n"),
                                      QStringLiteral("ambiguous.cel.hkp"));
        QVERIFY(document.model.isPadAmbiguous(QStringLiteral("P")));
        QVERIFY(document.model.isCellAmbiguous(QStringLiteral("C")));
        QVERIFY(document.model.findCell(QStringLiteral("C")) == nullptr);
        QVERIFY(document.diagnostics.hasErrors());
    }

    // 验证多个 HKP 文件可以合并，并且重复的原始名称会保留为歧义关联。
    /**
     * @brief 验证 mergesXpeditionDocuments 测试场景。
     */
    void mergesXpeditionDocuments() {
        // first：第一个测试结果。
        const XpeditionHkpDocument first = XpeditionHkpReader::parse(
            QStringLiteral(".FILETYPE PADSTACK_LIBRARY\n.UNITS mm\n.PAD \"P\"\n..ROUND\n...DIAMETER 1\n"
                           ".PADSTACK \"S\"\n...TOP_PAD \"P\"\n"),
            QStringLiteral("first.psk.hkp"));
        // second：第二个测试结果。
        const XpeditionHkpDocument second = XpeditionHkpReader::parse(
            QStringLiteral(".FILETYPE PADSTACK_LIBRARY\n.UNITS mm\n.PAD \"P\"\n..ROUND\n...DIAMETER 2\n"
                           ".PADSTACK \"S\"\n...TOP_PAD \"P\"\n"),
            QStringLiteral("second.psk.hkp"));
        // merged：合并后的导出结果。
        const XpeditionHkpDocument merged = XpeditionHkpMerger::merge({first, second}, QStringLiteral("merged.hkp"));
        QCOMPARE(merged.model.pads.size(), 2);
        QCOMPARE(merged.model.pads.at(1).name, QStringLiteral("P_2"));
        QVERIFY(merged.model.isPadAmbiguous(QStringLiteral("P")));
        QVERIFY(merged.model.isPadstackAmbiguous(QStringLiteral("S")));
        QVERIFY(merged.diagnostics.hasErrors());
    }

    // 验证 HKP 字节入口能够识别 UTF-8 BOM 并复用统一解析链。
    /**
     * @brief 验证 parsesXpeditionUtf8Bytes 测试场景。
     */
    void parsesXpeditionUtf8Bytes() {
        QByteArray bytes("\xEF\xBB\xBF.FILETYPE CELL_LIBRARY\n.UNITS mm\n.PACKAGE_CELL \"C1\"\n");
        // document：测试用文档对象。
        const XpeditionHkpDocument document = XpeditionHkpReader::parseBytes(bytes, QStringLiteral("bom.cel.hkp"));
        QVERIFY(document.isRecognized());
        QCOMPARE(document.type, XpeditionHkpType::CellLibrary);
        QVERIFY(!document.diagnostics.hasErrors());
    }

    // 验证 Cadstar 的 END* 分段和单行叶节点可以建立可遍历的树。
    /**
     * @brief 验证 parsesDelimitedSections 测试场景。
     */
    void parsesDelimitedSections() {
        // diagnostics：导出过程中收集的诊断信息。
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
    /**
     * @brief 验证 reportsDelimitedSectionErrors 测试场景。
     */
    void reportsDelimitedSectionErrors() {
        // diagnostics：导出过程中收集的诊断信息。
        ParseDiagnostics diagnostics;
        // roots：测试使用的文件系统路径。
        const QList<DelimitedSectionNode> roots = DelimitedSectionParser::parse(
            QStringLiteral("PAD \"P1\"\nSHAPE ROUND\nENDUNKNOWN\nENDPAD\n"), {QStringLiteral("SHAPE")}, &diagnostics);
        QVERIFY(!roots.isEmpty());
        QVERIFY(!diagnostics.isEmpty());
    }
};

QTEST_MAIN(TestTextParsers)
#include "test_text_parsers.moc"
