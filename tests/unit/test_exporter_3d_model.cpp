#include "core/ir/Model3DDataConverter.h"
#include "core/kicad/Exporter3DModel.h"
#include "models/Model3DData.h"
#include "tests/common/TestPaths.hpp"

#include <QTemporaryDir>
#include <QTest>

#include <limits>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestExporter3DModel : public QObject {
    Q_OBJECT

private slots:

    // === calculateObjMinZ 测试 ===

    /**
     * @brief 验证空 OBJ 的最小 Z 返回无穷大哨兵值。
     */
    void calculateObjMinZEmptyReturnsMax() {
        QCOMPARE(Exporter3DModel::calculateObjMinZ(QByteArray()), std::numeric_limits<double>::max());
    }

    /**
     * @brief 验证没有顶点的 OBJ 返回无穷大哨兵值。
     */
    void calculateObjMinZNoVerticesReturnsMax() {
        const QByteArray obj("# comment\nmtllib test.mtl\nusemtl default\n");
        QCOMPARE(Exporter3DModel::calculateObjMinZ(obj), std::numeric_limits<double>::max());
    }

    /**
     * @brief 验证单个顶点的 Z 坐标换算结果。
     */
    void calculateObjMinZSingleVertex() {
        // OBJ 坐标单位是 0.01 inch，除以 2.54 转换为 mm
        // v 0 0 254 => Z = 254/2.54 = 100 mm
        const QByteArray obj("v 0 0 254\n");
        QCOMPARE(Exporter3DModel::calculateObjMinZ(obj), 100.0);
    }

    /**
     * @brief 验证多个顶点取最小 Z 坐标。
     */
    void calculateObjMinZMultipleVertices() {
        const QByteArray obj(
            "v 0 0 254\n"
            "v 10 20 127\n"
            "v -5 0 508\n");
        // min Z = min(100, 50, 200) = 50 mm
        QCOMPARE(Exporter3DModel::calculateObjMinZ(obj), 50.0);
    }

    /**
     * @brief 验证负 Z 坐标可以正确参与最小值计算。
     */
    void calculateObjMinZNegativeZ() {
        const QByteArray obj("v 0 0 -254\n");
        QCOMPARE(Exporter3DModel::calculateObjMinZ(obj), -100.0);
    }

    /**
     * @brief 验证最小 Z 计算忽略法线、纹理和面定义。
     */
    void calculateObjMinZIgnoresNonVertexLines() {
        const QByteArray obj(
            "# comment\n"
            "vn 0 0 1\n"
            "vt 0 0\n"
            "v 0 0 254\n"
            "f 1 2 3\n");
        QCOMPARE(Exporter3DModel::calculateObjMinZ(obj), 100.0);
    }

    /**
     * @brief 验证 OBJ 几何校验接受包含顶点和面的模型。
     */
    void hasUsableObjGeometryAcceptsValidModel() {
        const QByteArray obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1/1 2/2 3/3\n");
        QVERIFY(Exporter3DModel::hasUsableObjGeometry(obj));
    }

    /**
     * @brief 验证 OBJ 几何校验拒绝错误响应和不完整模型。
     */
    void hasUsableObjGeometryRejectsInvalidModel() {
        QVERIFY(!Exporter3DModel::hasUsableObjGeometry(QByteArrayLiteral("<html>access denied</html>")));
        QVERIFY(!Exporter3DModel::hasUsableObjGeometry(QByteArrayLiteral("v 0 0 0\nv 1 0 0\n")));
    }

    /**
     * @brief 验证 WRL 几何校验接受包含坐标和面索引的模型。
     */
    void hasUsableWrlGeometryAcceptsValidModel() {
        /**
         * @brief 验证 wrl 测试场景。
         */
        const QByteArray wrl(
            "#VRML V2.0 utf8\n"
            "Shape { geometry IndexedFaceSet { coord Coordinate { point [0 0 0, 1 0 0, 0 1 0] } "
            "coordIndex [0 1 2 -1] } }\n");
        QVERIFY(Exporter3DModel::hasUsableWrlGeometry(wrl));
    }

    /**
     * @brief 验证 WRL 几何校验拒绝错误响应和不完整模型。
     */
    void hasUsableWrlGeometryRejectsInvalidModel() {
        QVERIFY(!Exporter3DModel::hasUsableWrlGeometry(QByteArrayLiteral("<html>access denied</html>")));
        QVERIFY(!Exporter3DModel::hasUsableWrlGeometry(QByteArrayLiteral("#VRML V2.0 utf8\nShape {}\n")));
    }

    /**
     * @brief 验证 STEP 结构校验接受完整交换文件。
     */
    void hasUsableStepDataAcceptsValidModel() {
        QVERIFY(Exporter3DModel::hasUsableStepData(
            QByteArrayLiteral("ISO-10303-21;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n")));
    }

    /**
     * @brief 验证 STEP 结构校验拒绝错误响应和截断文件。
     */
    void hasUsableStepDataRejectsInvalidModel() {
        QVERIFY(!Exporter3DModel::hasUsableStepData(QByteArrayLiteral("<html>access denied</html>")));
        QVERIFY(!Exporter3DModel::hasUsableStepData(QByteArrayLiteral("ISO-10303-21;\nDATA;\n")));
    }

    // === calculateWrlDisplayMinZ 测试 ===

    /**
     * @brief 验证空 WRL 的最小 Z 返回无穷大哨兵值。
     */
    void calculateWrlDisplayMinZEmptyReturnsMax() {
        QCOMPARE(Exporter3DModel::calculateWrlDisplayMinZ(QByteArray()), std::numeric_limits<double>::max());
    }

    /**
     * @brief 验证没有坐标点的 WRL 返回无穷大哨兵值。
     */
    void calculateWrlDisplayMinZNoPointsReturnsMax() {
        const QByteArray wrl("#VRML V2.0 utf8\nShape {}\n");
        QCOMPARE(Exporter3DModel::calculateWrlDisplayMinZ(wrl), std::numeric_limits<double>::max());
    }

    /**
     * @brief 验证单个 WRL 坐标点的单位换算结果。
     */
    void calculateWrlDisplayMinZSinglePoint() {
        // WRL 坐标 * 2.54 = mm（KiCad 按 1 WRL unit = 2.54 mm 解释）
        // point [0 0 0.5] => Z = 0.5 * 2.54 = 1.27 mm
        /**
         * @brief 验证 wrl 测试场景。
         */
        const QByteArray wrl(
            "#VRML V2.0 utf8\n"
            "Shape {\n"
            "  geometry IndexedFaceSet {\n"
            "    coord Coordinate {\n"
            "      point [0 0 0.5,]\n"
            "    }\n"
            "  }\n"
            "}\n");
        QCOMPARE(Exporter3DModel::calculateWrlDisplayMinZ(wrl), 1.27);
    }

    /**
     * @brief 验证多个 WRL 坐标点取最小 Z 坐标。
     */
    void calculateWrlDisplayMinZMultiplePoints() {
        /**
         * @brief 验证 wrl 测试场景。
         */
        const QByteArray wrl(
            "#VRML V2.0 utf8\n"
            "Shape {\n"
            "  geometry IndexedFaceSet {\n"
            "    coord Coordinate {\n"
            "      point [\n"
            "        1.0 2.0 0.1,\n"
            "        3.0 4.0 0.5,\n"
            "        5.0 6.0 0.2,\n"
            "      ]\n"
            "    }\n"
            "  }\n"
            "}\n");
        // min Z = min(0.1, 0.5, 0.2) * 2.54 = 0.254
        QCOMPARE(Exporter3DModel::calculateWrlDisplayMinZ(wrl), 0.254);
    }

    /**
     * @brief 验证多个 WRL 图形的坐标点共同参与最小值计算。
     */
    void calculateWrlDisplayMinZMultipleShapes() {
        /**
         * @brief 验证 wrl 测试场景。
         */
        const QByteArray wrl(
            "#VRML V2.0 utf8\n"
            "Shape {\n"
            "  geometry IndexedFaceSet {\n"
            "    coord Coordinate {\n"
            "      point [0 0 1.0,]\n"
            "    }\n"
            "  }\n"
            "}\n"
            "Shape {\n"
            "  geometry IndexedFaceSet {\n"
            "    coord Coordinate {\n"
            "      point [0 0 0.3,]\n"
            "    }\n"
            "  }\n"
            "}\n");
        // min Z across both shapes = 0.3 * 2.54 = 0.762
        QCOMPARE(Exporter3DModel::calculateWrlDisplayMinZ(wrl), 0.762);
    }

    // === exportToStep 测试 ===

    /**
     * @brief 验证 exportToStepWritesRawBytes 测试场景。
     */
    void exportToStepWritesRawBytes() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // model：测试使用的配置值或状态。
        Model3DData model;
        const QByteArray stepData("ISO-10303-21;\nHEADER;\nENDSEC;\nENDISO-10303-21;\n");
        model.setStep(stepData);

        // savePath：测试使用的文件系统路径。
        const QString savePath = tempDir.filePath(QStringLiteral("test.step"));

        // exporter：负责生成测试输出的导出器实例。
        Exporter3DModel exporter;
        // irModel：测试使用的配置值或状态。
        auto irModel = IR::toModel3DIR(model);
        QVERIFY(exporter.exportToStep(irModel, savePath));

        QFile file(savePath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), stepData);
    }

    /**
     * @brief 验证空 STEP 数据仍能生成空输出文件。
     */
    void exportToStepEmptyDataCreatesEmptyFile() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // model：测试使用的配置值或状态。
        Model3DData model;
        // savePath：测试使用的文件系统路径。
        const QString savePath = tempDir.filePath(QStringLiteral("empty.step"));

        // exporter：负责生成测试输出的导出器实例。
        Exporter3DModel exporter;
        // irModel：测试使用的配置值或状态。
        auto irModel = IR::toModel3DIR(model);
        QVERIFY(exporter.exportToStep(irModel, savePath));

        QFile file(savePath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().isEmpty());
    }

    // === exportToWrl 黄金文件测试 ===

    /**
     * @brief 验证 exportToWrlSimpleTriangleMatchesGolden 测试场景。
     */
    void exportToWrlSimpleTriangleMatchesGolden() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // 构造简单三角形 OBJ 数据（3 顶点 + 1 面 + 1 材质）
        Model3DData model;
        model.setName(QStringLiteral("TRIANGLE"));
        model.setTranslation({0.0, 0.0, 0.0});
        model.setRotation({0.0, 0.0, 0.0});
        model.setRawObj(
            QStringLiteral("# Simple triangle\n"
                           "mtllib triangle.mtl\n"
                           "usemtl copper\n"
                           "v 0 0 0\n"
                           "v 254 0 0\n"
                           "v 127 254 0\n"
                           "f 1 2 3\n"));

        // savePath：测试使用的文件系统路径。
        const QString savePath = tempDir.filePath(QStringLiteral("TRIANGLE.wrl"));

        // exporter：负责生成测试输出的导出器实例。
        Exporter3DModel exporter;
        // irModel：测试使用的配置值或状态。
        auto irModel = IR::toModel3DIR(model);
        QVERIFY(exporter.exportToWrl(irModel, savePath));

        // error：测试期间收集的错误或诊断信息。
        QString error;
        // actual：测试得到的实际结果数据。
        const QString actual = TestPaths::readText(savePath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(TestPaths::compareTextToGolden(actual, QStringLiteral("kicad/golden_3d_triangle.wrl"), &error),
                 qPrintable(error));
    }

    /**
     * @brief 验证 WRL 输出包含标准文件头。
     */
    void exportToWrlContainsVrmlHeader() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // model：测试使用的配置值或状态。
        Model3DData model;
        model.setRawObj(QStringLiteral("v 0 0 0\nv 100 0 0\nv 50 100 0\nf 1 2 3\n"));

        // savePath：测试使用的文件系统路径。
        const QString savePath = tempDir.filePath(QStringLiteral("header.wrl"));

        // exporter：负责生成测试输出的导出器实例。
        Exporter3DModel exporter;
        // irModel：测试使用的配置值或状态。
        auto irModel = IR::toModel3DIR(model);
        QVERIFY(exporter.exportToWrl(irModel, savePath));

        QFile file(savePath);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        // content：测试使用的文本或数据内容。
        const QString content = QString::fromUtf8(file.readAll());
        QVERIFY(content.startsWith(QStringLiteral("#VRML V2.0 utf8")));
    }

    /**
     * @brief 验证 WRL 顶点按毫米规则归一化。
     */
    void exportToWrlVerticesNormalizedToMm() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // model：测试使用的配置值或状态。
        Model3DData model;
        model.setRawObj(QStringLiteral("v 0 0 254\nv 254 0 254\nv 127 254 254\nf 1 2 3\n"));

        // savePath：测试使用的文件系统路径。
        const QString savePath = tempDir.filePath(QStringLiteral("normalize.wrl"));

        // exporter：负责生成测试输出的导出器实例。
        Exporter3DModel exporter;
        // irModel：测试使用的配置值或状态。
        auto irModel = IR::toModel3DIR(model);
        QVERIFY(exporter.exportToWrl(irModel, savePath));

        QFile file(savePath);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        // content：测试使用的文本或数据内容。
        const QString content = QString::fromUtf8(file.readAll());

        // 所有 Z 坐标应为 0（归一化后）
        QVERIFY(content.contains(QStringLiteral("-50.0000 -50.0000 0.0000")));
        QVERIFY(content.contains(QStringLiteral("50.0000 -50.0000 0.0000")));
        QVERIFY(content.contains(QStringLiteral("0.0000 50.0000 0.0000")));
    }
};

QTEST_GUILESS_MAIN(TestExporter3DModel)
#include "test_exporter_3d_model.moc"
