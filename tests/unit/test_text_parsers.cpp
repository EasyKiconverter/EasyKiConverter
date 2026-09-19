#include "core/parser/GeometryTransforms.h"
#include "core/parser/ParseDiagnostics.h"
#include "core/parser/TextParsers.h"

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
};

QTEST_MAIN(TestTextParsers)
#include "test_text_parsers.moc"
