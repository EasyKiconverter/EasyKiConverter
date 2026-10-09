#include "models/ComponentData.h"
#include "services/export/ExportProgress.h"
#include "services/export/ExportProgressAggregator.h"
#include "services/export/ExportRunPlan.h"

#include <QtTest/QtTest>

namespace EasyKiConverter {

class TestExportProgress : public QObject {
    Q_OBJECT

private slots:

    // === ExportOptions 测试 ===

    // 验证状态合并会保留先前到达且不重复的诊断信息。
    /**
     * @brief 验证 exportProgressAggregatorMergesDiagnostics 测试场景。
     */
    void exportProgressAggregatorMergesDiagnostics() {
        // progress：测试用导出进度状态。
        ExportTypeProgress progress;
        // firstStatus：测试过程中的状态数据。
        ExportItemStatus firstStatus;
        firstStatus.status = ExportItemStatus::Status::InProgress;
        firstStatus.diagnostics = {QStringLiteral("first"), QStringLiteral("shared")};
        ExportProgressAggregator::mergeItemStatus(progress, QStringLiteral("C1"), firstStatus);

        // finalStatus：测试完成后的最终状态。
        ExportItemStatus finalStatus;
        finalStatus.status = ExportItemStatus::Status::Success;
        finalStatus.diagnostics = {QStringLiteral("shared"), QStringLiteral("second")};
        ExportProgressAggregator::mergeItemStatus(progress, QStringLiteral("C1"), finalStatus);

        QCOMPARE(progress.itemStatus.value(QStringLiteral("C1")).status, ExportItemStatus::Status::Success);
        QCOMPARE(progress.itemStatus.value(QStringLiteral("C1")).diagnostics,
                 QStringList({QStringLiteral("shared"), QStringLiteral("second"), QStringLiteral("first")}));
        QCOMPARE(progress.successCount, 1);
        QCOMPARE(progress.completedCount, 1);
    }

    // 验证最终统计只计算已经完成的元件，并区分成功和失败结果。
    /**
     * @brief 验证 exportProgressAggregatorCountsCompletedComponents 测试场景。
     */
    void exportProgressAggregatorCountsCompletedComponents() {
        // symbolProgress：符号导出进度状态。
        ExportTypeProgress symbolProgress;
        symbolProgress.itemStatus[QStringLiteral("C1")].status = ExportItemStatus::Status::Success;
        symbolProgress.itemStatus[QStringLiteral("C2")].status = ExportItemStatus::Status::Failed;
        symbolProgress.itemStatus[QStringLiteral("C3")].status = ExportItemStatus::Status::Pending;

        // footprintProgress：封装导出进度状态。
        ExportTypeProgress footprintProgress;
        footprintProgress.itemStatus[QStringLiteral("C1")].status = ExportItemStatus::Status::Skipped;
        footprintProgress.itemStatus[QStringLiteral("C2")].status = ExportItemStatus::Status::Success;
        footprintProgress.itemStatus[QStringLiteral("C3")].status = ExportItemStatus::Status::Success;

        // progress：测试用导出进度状态。
        QMap<QString, ExportTypeProgress> progress;
        progress.insert(QStringLiteral("Symbol"), symbolProgress);
        progress.insert(QStringLiteral("Footprint"), footprintProgress);

        // totals：各导出类型的总任务数量。
        const ExportCompletionTotals totals = ExportProgressAggregator::countCompletedComponents(
            {QStringLiteral("C1"), QStringLiteral("C2"), QStringLiteral("C3")}, progress);

        QCOMPARE(totals.successCount, 1);
        QCOMPARE(totals.failedCount, 1);
    }

    // 验证导出计划会区分完整缓存数据和缺失数据。
    /**
     * @brief 验证 exportRunPlanSeparatesCachedData 测试场景。
     */
    void exportRunPlanSeparatesCachedData() {
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.exportModel3D = true;
        options.exportPreviewImages = true;
        options.exportDatasheet = true;

        // cachedComponent：测试使用的缓存或元器件对象。
        auto cachedComponent = QSharedPointer<ComponentData>::create();
        cachedComponent->setLcscId(QStringLiteral("C100"));
        cachedComponent->setSymbolData(QSharedPointer<SymbolData>::create());
        cachedComponent->setFootprintData(QSharedPointer<FootprintData>::create());

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData.insert(QStringLiteral("C100"), cachedComponent);
        const ExportRunPlan plan =
            buildExportRunPlan(options, {QStringLiteral("C100"), QStringLiteral("C200")}, cachedData);

        QCOMPARE(plan.exportableComponentIds, QStringList{QStringLiteral("C100")});
        QCOMPARE(plan.missingDataComponentIds, QStringList{QStringLiteral("C200")});
        QCOMPARE(plan.progressTypeNames(),
                 QStringList({QStringLiteral("Symbol"),
                              QStringLiteral("Footprint"),
                              QStringLiteral("Model3D"),
                              QStringLiteral("PreviewImages"),
                              QStringLiteral("Datasheet")}));
        QCOMPARE(plan.runningStageCount(), 5);
    }

    // 验证 Xpedition 会输出独立三维模型，但不请求原生内嵌关联。
    /**
     * @brief 验证 exportRunPlanUsesStandaloneXpeditionModelStage 测试场景。
     */
    void exportRunPlanUsesStandaloneXpeditionModelStage() {
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.targetFormat = TargetEdaFormat::Xpedition;
        options.exportSymbol = false;
        options.exportFootprint = true;
        options.exportModel3D = true;

        // plan：测试用执行计划。
        const ExportRunPlan plan = buildExportRunPlan(options, {}, {});

        QCOMPARE(plan.enableModel3D, true);
        QCOMPARE(plan.runExternalModel3DStage, true);
        QCOMPARE(plan.progressTypeNames(), QStringList({QStringLiteral("Footprint"), QStringLiteral("Model3D")}));
        QCOMPARE(plan.runningStageCount(), 2);
    }

    // 验证仅导出独立三维模型时不会额外启动封装库阶段。
    /**
     * @brief 验证 exportRunPlanUsesOnlyStandaloneModelStage 测试场景。
     */
    void exportRunPlanUsesOnlyStandaloneModelStage() {
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.targetFormat = TargetEdaFormat::Xpedition;
        options.exportSymbol = false;
        options.exportFootprint = false;
        options.exportModel3D = true;

        // plan：测试用执行计划。
        const ExportRunPlan plan = buildExportRunPlan(options, {}, {});

        QVERIFY(!plan.enableSymbol);
        QVERIFY(!plan.enableFootprint);
        QVERIFY(plan.enableModel3D);
        QVERIFY(plan.runExternalModel3DStage);
        QCOMPARE(plan.progressTypeNames(), QStringList({QStringLiteral("Model3D")}));
        QCOMPARE(plan.runningStageCount(), 1);
    }

    // 验证 OrCAD 的独立三维导出只启动模型阶段，不错误创建不存在的封装导出阶段。
    /**
     * @brief 验证 exportRunPlanKeepsOrcadModelIndependent 测试场景。
     */
    void exportRunPlanKeepsOrcadModelIndependent() {
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.targetFormat = TargetEdaFormat::Orcad;
        options.exportSymbol = false;
        options.exportFootprint = false;
        options.exportModel3D = true;

        // component：测试使用的缓存或元器件对象。
        auto component = QSharedPointer<ComponentData>::create();
        component->setLcscId(QStringLiteral("C12399"));
        component->setFootprintData(QSharedPointer<FootprintData>::create());

        const ExportRunPlan plan =
            buildExportRunPlan(options, {QStringLiteral("C12399")}, {{QStringLiteral("C12399"), component}});

        QVERIFY(!plan.enableSymbol);
        QVERIFY(!plan.enableFootprint);
        QVERIFY(plan.enableModel3D);
        QVERIFY(plan.runExternalModel3DStage);
        QCOMPARE(plan.exportableComponentIds, QStringList{QStringLiteral("C12399")});
        QCOMPARE(plan.progressTypeNames(), QStringList{QStringLiteral("Model3D")});
        QCOMPARE(plan.runningStageCount(), 1);
    }

    // 验证独立三维模型可以在没有封装数据时单独导出，不被封装阶段的输入条件阻断。
    /**
     * @brief 验证 exportRunPlanAcceptsIndependentModelWithoutFootprint 测试场景。
     */
    void exportRunPlanAcceptsIndependentModelWithoutFootprint() {
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.targetFormat = TargetEdaFormat::Xpedition;
        options.exportSymbol = false;
        options.exportFootprint = false;
        options.exportModel3D = true;

        // component：测试使用的缓存或元器件对象。
        auto component = QSharedPointer<ComponentData>::create();
        component->setLcscId(QStringLiteral("C12400"));
        component->setSymbolData(QSharedPointer<SymbolData>::create());
        // model：测试使用的配置值或状态。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(QStringLiteral("independent-model"));
        model->setName(QStringLiteral("IndependentModel"));
        component->setModel3DData(model);

        const ExportRunPlan plan =
            buildExportRunPlan(options, {QStringLiteral("C12400")}, {{QStringLiteral("C12400"), component}});

        QVERIFY(!plan.enableSymbol);
        QVERIFY(!plan.enableFootprint);
        QVERIFY(plan.runExternalModel3DStage);
        QCOMPARE(plan.exportableComponentIds, QStringList{QStringLiteral("C12400")});
        QVERIFY(plan.missingDataComponentIds.isEmpty());
    }

    // 验证组合库目标会同时规划符号、封装和独立三维模型输出。
    /**
     * @brief 验证 exportRunPlanIncludesAllLibraryArtifacts 测试场景。
     */
    void exportRunPlanIncludesAllLibraryArtifacts() {
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.exportModel3D = true;

        options.targetFormat = TargetEdaFormat::Pcad;
        // pcadPlan：目标格式的导出计划。
        ExportRunPlan pcadPlan = buildExportRunPlan(options, {}, {});
        QVERIFY(pcadPlan.enableSymbol);
        QVERIFY(pcadPlan.enableFootprint);
        QVERIFY(pcadPlan.enableModel3D);
        QVERIFY(pcadPlan.runExternalModel3DStage);

        options.targetFormat = TargetEdaFormat::Eagle;
        // eaglePlan：目标格式的导出计划。
        const ExportRunPlan eaglePlan = buildExportRunPlan(options, {}, {});
        QVERIFY(!eaglePlan.enableSymbol);
        QVERIFY(eaglePlan.enableFootprint);
        QVERIFY(eaglePlan.enableModel3D);
        QVERIFY(eaglePlan.runExternalModel3DStage);

        options.targetFormat = TargetEdaFormat::Altium;
        // altiumPlan：目标格式的导出计划。
        const ExportRunPlan altiumPlan = buildExportRunPlan(options, {}, {});
        QVERIFY(altiumPlan.enableSymbol);
        QVERIFY(altiumPlan.enableFootprint);
        QVERIFY(altiumPlan.enableModel3D);
        QVERIFY(!altiumPlan.runExternalModel3DStage);

        options.targetFormat = TargetEdaFormat::Orcad;
        // orcadPlan：目标格式的导出计划。
        const ExportRunPlan orcadPlan = buildExportRunPlan(options, {}, {});
        QVERIFY(orcadPlan.enableSymbol);
        QVERIFY(!orcadPlan.enableFootprint);
        QVERIFY(orcadPlan.enableModel3D);
        QVERIFY(orcadPlan.runExternalModel3DStage);

        options.targetFormat = TargetEdaFormat::Xpedition;
        // xpeditionPlan：目标格式的导出计划。
        const ExportRunPlan xpeditionPlan = buildExportRunPlan(options, {}, {});
        QVERIFY(xpeditionPlan.enableSymbol);
        QVERIFY(xpeditionPlan.enableFootprint);
        QVERIFY(xpeditionPlan.enableModel3D);
        QVERIFY(xpeditionPlan.runExternalModel3DStage);

        options.targetFormat = TargetEdaFormat::Pads;
        // padsPlan：测试用焊盘数据。
        const ExportRunPlan padsPlan = buildExportRunPlan(options, {}, {});
        QVERIFY(padsPlan.enableSymbol);
        QVERIFY(padsPlan.enableFootprint);
        QVERIFY(padsPlan.enableModel3D);
        QVERIFY(padsPlan.runExternalModel3DStage);

        options.targetFormat = TargetEdaFormat::Cadstar;
        // cadstarPlan：目标格式的导出计划。
        const ExportRunPlan cadstarPlan = buildExportRunPlan(options, {}, {});
        QVERIFY(!cadstarPlan.enableSymbol);
        QVERIFY(cadstarPlan.enableFootprint);
        QVERIFY(cadstarPlan.enableModel3D);
        QVERIFY(cadstarPlan.runExternalModel3DStage);

        options.targetFormat = TargetEdaFormat::Allegro;
        // allegroPlan：目标格式的导出计划。
        const ExportRunPlan allegroPlan = buildExportRunPlan(options, {}, {});
        QVERIFY(!allegroPlan.enableSymbol);
        QVERIFY(allegroPlan.enableFootprint);
        QVERIFY(allegroPlan.enableModel3D);
        QVERIFY(!allegroPlan.runExternalModel3DStage);
    }

    // 验证仅符号或仅封装的目标不会被另一类未启用数据错误阻断。
    /**
     * @brief 验证 exportRunPlanUsesOnlyRequiredLibraryData 测试场景。
     */
    void exportRunPlanUsesOnlyRequiredLibraryData() {
        // symbolOnly：测试用符号数据。
        auto symbolOnly = QSharedPointer<ComponentData>::create();
        symbolOnly->setLcscId(QStringLiteral("C100"));
        symbolOnly->setSymbolData(QSharedPointer<SymbolData>::create());

        // orcadOptions：当前测试使用的导出选项。
        ExportOptions orcadOptions;
        orcadOptions.targetFormat = TargetEdaFormat::Orcad;
        orcadOptions.exportSymbol = true;
        orcadOptions.exportFootprint = false;
        orcadOptions.exportModel3D = false;
        const ExportRunPlan orcadPlan =
            buildExportRunPlan(orcadOptions, {QStringLiteral("C100")}, {{QStringLiteral("C100"), symbolOnly}});
        QCOMPARE(orcadPlan.exportableComponentIds, QStringList{QStringLiteral("C100")});

        // footprintOnly：测试用封装数据。
        auto footprintOnly = QSharedPointer<ComponentData>::create();
        footprintOnly->setLcscId(QStringLiteral("C200"));
        footprintOnly->setFootprintData(QSharedPointer<FootprintData>::create());

        // allegroOptions：当前测试使用的导出选项。
        ExportOptions allegroOptions;
        allegroOptions.targetFormat = TargetEdaFormat::Allegro;
        allegroOptions.exportSymbol = false;
        allegroOptions.exportFootprint = true;
        allegroOptions.exportModel3D = false;
        const ExportRunPlan allegroPlan =
            buildExportRunPlan(allegroOptions, {QStringLiteral("C200")}, {{QStringLiteral("C200"), footprintOnly}});
        QCOMPARE(allegroPlan.exportableComponentIds, QStringList{QStringLiteral("C200")});

        // eagleOptions：当前测试使用的导出选项。
        ExportOptions eagleOptions;
        eagleOptions.targetFormat = TargetEdaFormat::Eagle;
        eagleOptions.exportSymbol = false;
        eagleOptions.exportFootprint = true;
        const ExportRunPlan eaglePlan =
            buildExportRunPlan(eagleOptions, {QStringLiteral("C200")}, {{QStringLiteral("C200"), footprintOnly}});
        QCOMPARE(eaglePlan.exportableComponentIds, QStringList{QStringLiteral("C200")});

        // eagleSymbolOptions：当前测试使用的导出选项。
        ExportOptions eagleSymbolOptions;
        eagleSymbolOptions.targetFormat = TargetEdaFormat::Eagle;
        eagleSymbolOptions.exportSymbol = true;
        eagleSymbolOptions.exportFootprint = false;
        // eagleSymbolData：测试使用的文本或数据内容。
        auto eagleSymbolData = QSharedPointer<ComponentData>::create();
        eagleSymbolData->setLcscId(QStringLiteral("C300"));
        eagleSymbolData->setSymbolData(QSharedPointer<SymbolData>::create());
        // eagleSymbolPlan：测试用符号数据。
        const ExportRunPlan eagleSymbolPlan = buildExportRunPlan(
            eagleSymbolOptions, {QStringLiteral("C300")}, {{QStringLiteral("C300"), eagleSymbolData}});
        QVERIFY(eagleSymbolPlan.enableFootprint);
        QVERIFY(eagleSymbolPlan.symbolOnlyCombinedLibrary);
        QCOMPARE(eagleSymbolPlan.progressTypeNames(), QStringList{QStringLiteral("Symbol")});
        QCOMPARE(eagleSymbolPlan.exportableComponentIds, QStringList{QStringLiteral("C300")});
        QVERIFY(eagleSymbolPlan.missingDataComponentIds.isEmpty());

        // cadstarOptions：当前测试使用的导出选项。
        ExportOptions cadstarOptions;
        cadstarOptions.targetFormat = TargetEdaFormat::Cadstar;
        cadstarOptions.exportSymbol = false;
        cadstarOptions.exportFootprint = true;
        const ExportRunPlan cadstarPlan =
            buildExportRunPlan(cadstarOptions, {QStringLiteral("C200")}, {{QStringLiteral("C200"), footprintOnly}});
        QCOMPARE(cadstarPlan.exportableComponentIds, QStringList{QStringLiteral("C200")});

        // embeddedModelOptions：测试使用的配置值或状态。
        ExportOptions embeddedModelOptions;
        embeddedModelOptions.targetFormat = TargetEdaFormat::Allegro;
        embeddedModelOptions.exportSymbol = false;
        embeddedModelOptions.exportFootprint = false;
        embeddedModelOptions.exportModel3D = true;
        // embeddedModelPlan：测试使用的配置值或状态。
        const ExportRunPlan embeddedModelPlan = buildExportRunPlan(
            embeddedModelOptions, {QStringLiteral("C200")}, {{QStringLiteral("C200"), footprintOnly}});
        QVERIFY(embeddedModelPlan.enableFootprint);
        QCOMPARE(embeddedModelPlan.exportableComponentIds, QStringList{QStringLiteral("C200")});

        // allegroSymbolOptions：当前测试使用的导出选项。
        ExportOptions allegroSymbolOptions;
        allegroSymbolOptions.targetFormat = TargetEdaFormat::Allegro;
        allegroSymbolOptions.exportSymbol = true;
        allegroSymbolOptions.exportFootprint = false;
        // allegroSymbolData：测试使用的文本或数据内容。
        auto allegroSymbolData = QSharedPointer<ComponentData>::create();
        allegroSymbolData->setLcscId(QStringLiteral("C400"));
        allegroSymbolData->setSymbolData(QSharedPointer<SymbolData>::create());
        // allegroSymbolPlan：测试用符号数据。
        const ExportRunPlan allegroSymbolPlan = buildExportRunPlan(
            allegroSymbolOptions, {QStringLiteral("C400")}, {{QStringLiteral("C400"), allegroSymbolData}});
        QVERIFY(!allegroSymbolPlan.enableSymbol);
        QVERIFY(allegroSymbolPlan.enableFootprint);
        QVERIFY(allegroSymbolPlan.symbolOnlyCombinedLibrary);
        QCOMPARE(allegroSymbolPlan.progressTypeNames(), QStringList{QStringLiteral("Symbol")});
        QCOMPARE(allegroSymbolPlan.exportableComponentIds, QStringList{QStringLiteral("C400")});
        QVERIFY(allegroSymbolPlan.missingDataComponentIds.isEmpty());
    }

    // 提供三维模型格式位掩码测试所需的参数组合。
    /**
     * @brief 验证 exportOptionsModel3DFormatBitmask_data 测试场景。
     */
    void exportOptionsModel3DFormatBitmask_data() {
        QTest::addColumn<int>("format");
        QTest::addColumn<bool>("wrl");
        QTest::addColumn<bool>("step");

        QTest::newRow("none") << static_cast<int>(ExportOptions::MODEL_3D_FORMAT_NONE) << false << false;
        QTest::newRow("wrl") << static_cast<int>(ExportOptions::MODEL_3D_FORMAT_WRL) << true << false;
        QTest::newRow("step") << static_cast<int>(ExportOptions::MODEL_3D_FORMAT_STEP) << false << true;
        QTest::newRow("both") << static_cast<int>(ExportOptions::MODEL_3D_FORMAT_BOTH) << true << true;
    }

    // 验证三维模型格式位掩码对应的 WRL 和 STEP 需求。
    /**
     * @brief 验证 exportOptionsModel3DFormatBitmask 测试场景。
     */
    void exportOptionsModel3DFormatBitmask() {
        QFETCH(int, format);
        QFETCH(bool, wrl);
        QFETCH(bool, step);

        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.exportModel3DFormat = format;
        QCOMPARE(options.needsModel3DWrl(), wrl);
        QCOMPARE(options.needsModel3DStep(), step);
    }

    // 验证 Altium 目标启用三维模型时始终需要内嵌 STEP。
    /**
     * @brief 验证 altiumAlwaysRequiresEmbeddedStep 测试场景。
     */
    void altiumAlwaysRequiresEmbeddedStep() {
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.targetFormat = TargetEdaFormat::Altium;
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;

        QVERIFY(options.needsEmbeddedModel3DStep());

        options.exportModel3D = false;
        QVERIFY(!options.needsEmbeddedModel3DStep());
    }

    // 验证禁用三维模型时不会请求内嵌数据。
    /**
     * @brief 验证 model3DIsNotRequestedWhenDisabled 测试场景。
     */
    void model3DIsNotRequestedWhenDisabled() {
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.targetFormat = TargetEdaFormat::KiCad;
        options.exportModel3D = false;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_STEP;

        QVERIFY(!options.needsEmbeddedModel3DStep());
    }

    // 验证 Xpedition 目标不会请求内嵌三维模型。
    /**
     * @brief 验证 xpeditionDoesNotRequestEmbeddedModel3D 测试场景。
     */
    void xpeditionDoesNotRequestEmbeddedModel3D() {
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.targetFormat = TargetEdaFormat::Xpedition;
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_BOTH;

        QVERIFY(!options.needsEmbeddedModel3DStep());
    }

    // 提供三维模型路径模式的规范化测试数据。
    /**
     * @brief 验证 exportOptionsNormalizePathMode_data 测试场景。
     */
    void exportOptionsNormalizePathMode_data() {
        QTest::addColumn<int>("input");
        QTest::addColumn<int>("expected");

        QTest::newRow("relative") << 0 << 0;
        QTest::newRow("absolute") << 1 << 1;
        QTest::newRow("out-of-range") << 2 << 0;
        QTest::newRow("negative") << -1 << 0;
    }

    // 验证三维模型路径模式会收敛到有效取值。
    /**
     * @brief 验证 exportOptionsNormalizePathMode 测试场景。
     */
    void exportOptionsNormalizePathMode() {
        QFETCH(int, input);
        QFETCH(int, expected);
        QCOMPARE(ExportOptions::normalizePathMode(input), expected);
    }

    // === ExportItemStatus 测试 ===

    /**
     * @brief 验证 itemStatusIsComplete_data 测试场景。
     */
    void itemStatusIsComplete_data() {
        QTest::addColumn<ExportItemStatus::Status>("status");
        QTest::addColumn<bool>("expected");

        QTest::newRow("Pending") << ExportItemStatus::Status::Pending << false;
        QTest::newRow("InProgress") << ExportItemStatus::Status::InProgress << false;
        QTest::newRow("Success") << ExportItemStatus::Status::Success << true;
        QTest::newRow("Failed") << ExportItemStatus::Status::Failed << true;
        QTest::newRow("Skipped") << ExportItemStatus::Status::Skipped << true;
    }

    // 验证导出条目状态的完成判断。
    /**
     * @brief 验证 itemStatusIsComplete 测试场景。
     */
    void itemStatusIsComplete() {
        QFETCH(ExportItemStatus::Status, status);
        QFETCH(bool, expected);

        // item：测试使用的缓存或元器件对象。
        ExportItemStatus item;
        item.status = status;
        QCOMPARE(item.isComplete(), expected);
    }

    // 验证导出条目成功状态的判断。
    /**
     * @brief 验证 itemStatusIsSuccess 测试场景。
     */
    void itemStatusIsSuccess() {
        // item：测试使用的缓存或元器件对象。
        ExportItemStatus item;
        item.status = ExportItemStatus::Status::Success;
        QVERIFY(item.isSuccess());

        item.status = ExportItemStatus::Status::Failed;
        QVERIFY(!item.isSuccess());

        item.status = ExportItemStatus::Status::Pending;
        QVERIFY(!item.isSuccess());
    }

    // 验证导出条目持续时间的计算和无效时间处理。
    /**
     * @brief 验证 itemStatusDurationMs 测试场景。
     */
    void itemStatusDurationMs() {
        // item：测试使用的缓存或元器件对象。
        ExportItemStatus item;

        // 无效时间返回 0
        QCOMPARE(item.durationMs(), 0);

        // 有效时间返回正确差值
        item.startTime = QDateTime::fromString("2026-01-01T00:00:00", Qt::ISODate);
        item.endTime = QDateTime::fromString("2026-01-01T00:00:02", Qt::ISODate);
        QCOMPARE(item.durationMs(), 2000);

        // 只有 startTime 无效
        ExportItemStatus item2;
        item2.endTime = QDateTime::currentDateTime();
        QCOMPARE(item2.durationMs(), 0);
    }

    // 提供导出条目字节进度百分比的测试数据。
    /**
     * @brief 验证 itemStatusPercentage_data 测试场景。
     */
    void itemStatusPercentage_data() {
        QTest::addColumn<qint64>("processed");
        QTest::addColumn<qint64>("total");
        QTest::addColumn<int>("expected");

        QTest::newRow("zero-total") << qint64(0) << qint64(0) << 0;
        QTest::newRow("half") << qint64(50) << qint64(100) << 50;
        QTest::newRow("complete") << qint64(100) << qint64(100) << 100;
        QTest::newRow("not-started") << qint64(0) << qint64(100) << 0;
    }

    // 验证导出条目字节进度百分比计算。
    /**
     * @brief 验证 itemStatusPercentage 测试场景。
     */
    void itemStatusPercentage() {
        QFETCH(qint64, processed);
        QFETCH(qint64, total);
        QFETCH(int, expected);

        // item：测试使用的缓存或元器件对象。
        ExportItemStatus item;
        item.bytesProcessed = processed;
        item.totalBytes = total;
        QCOMPARE(item.percentage(), expected);
    }

    // === ExportTypeProgress 测试 ===

    // 验证导出类型进度百分比计算。
    /**
     * @brief 验证 typeProgressPercentage 测试场景。
     */
    void typeProgressPercentage() {
        // progress：测试用导出进度状态。
        ExportTypeProgress progress;
        QCOMPARE(progress.percentage(), 0);

        progress.totalCount = 10;
        progress.completedCount = 3;
        QCOMPARE(progress.percentage(), 30);

        progress.completedCount = 10;
        QCOMPARE(progress.percentage(), 100);
    }

    // 验证导出类型在完成数量达到总数后结束。
    /**
     * @brief 验证 typeProgressIsComplete 测试场景。
     */
    void typeProgressIsComplete() {
        // progress：测试用导出进度状态。
        ExportTypeProgress progress;
        QVERIFY(progress.isComplete());  // 0 >= 0

        progress.totalCount = 5;
        QVERIFY(!progress.isComplete());

        progress.completedCount = 5;
        QVERIFY(progress.isComplete());

        progress.completedCount = 6;  // 超过也是完成
        QVERIFY(progress.isComplete());
    }

    // === PreloadProgress 测试 ===

    /**
     * @brief 验证 preloadProgressPercentage 测试场景。
     */
    void preloadProgressPercentage() {
        // progress：测试用导出进度状态。
        PreloadProgress progress;
        QCOMPARE(progress.percentage(), 0);

        progress.totalCount = 4;
        progress.completedCount = 1;
        QCOMPARE(progress.percentage(), 25);
    }

    // 验证预加载进度在完成数量达到总数后结束。
    /**
     * @brief 验证 preloadProgressIsComplete 测试场景。
     */
    void preloadProgressIsComplete() {
        // progress：测试用导出进度状态。
        PreloadProgress progress;
        QVERIFY(progress.isComplete());  // 0 >= 0

        progress.totalCount = 3;
        QVERIFY(!progress.isComplete());

        progress.completedCount = 3;
        QVERIFY(progress.isComplete());
    }

    // === ExportOverallProgress 测试 ===

    // 验证空闲阶段的整体进度为零。
    /**
     * @brief 验证 overallProgressIdleReturnsZero 测试场景。
     */
    void overallProgressIdleReturnsZero() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Idle;
        QCOMPARE(progress.overallPercentage(), 0);
    }

    // 验证完成阶段的整体进度为百分之百。
    /**
     * @brief 验证 overallProgressCompletedReturns100 测试场景。
     */
    void overallProgressCompletedReturns100() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Completed;
        QCOMPARE(progress.overallPercentage(), 100);
    }

    // 验证取消和失败阶段的整体进度为零。
    /**
     * @brief 验证 overallProgressCancelledFailedReturnsZero 测试场景。
     */
    void overallProgressCancelledFailedReturnsZero() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;

        progress.currentStage = ExportOverallProgress::Stage::Cancelled;
        QCOMPARE(progress.overallPercentage(), 0);

        progress.currentStage = ExportOverallProgress::Stage::Failed;
        QCOMPARE(progress.overallPercentage(), 0);
    }

    // 验证预加载阶段的整体进度取自预加载进度。
    /**
     * @brief 验证 overallProgressPreloadingReturnsPreloadPercentage 测试场景。
     */
    void overallProgressPreloadingReturnsPreloadPercentage() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Preloading;
        progress.preloadProgress.totalCount = 10;
        progress.preloadProgress.completedCount = 4;
        QCOMPARE(progress.overallPercentage(), 40);
    }

    // 验证导出阶段按全部导出类型聚合整体进度。
    /**
     * @brief 验证 overallProgressExportingAggregatesTypes 测试场景。
     */
    void overallProgressExportingAggregatesTypes() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Exporting;

        // symbol：测试用符号数据。
        ExportTypeProgress symbol;
        symbol.totalCount = 10;
        symbol.completedCount = 5;
        progress.exportTypeProgress[QStringLiteral("Symbol")] = symbol;

        // footprint：测试用封装数据。
        ExportTypeProgress footprint;
        footprint.totalCount = 10;
        footprint.completedCount = 10;
        progress.exportTypeProgress[QStringLiteral("Footprint")] = footprint;

        // total: 20, completed: 15 => 75%
        QCOMPARE(progress.overallPercentage(), 75);
    }

    // 验证没有导出类型时整体进度为零。
    /**
     * @brief 验证 overallProgressExportingEmptyReturnsZero 测试场景。
     */
    void overallProgressExportingEmptyReturnsZero() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Exporting;
        QCOMPARE(progress.overallPercentage(), 0);
    }

    // 提供整体阶段完成判断的测试数据。
    /**
     * @brief 验证 overallProgressIsComplete_data 测试场景。
     */
    void overallProgressIsComplete_data() {
        QTest::addColumn<ExportOverallProgress::Stage>("stage");
        QTest::addColumn<bool>("expected");

        QTest::newRow("Idle") << ExportOverallProgress::Stage::Idle << false;
        QTest::newRow("Completed") << ExportOverallProgress::Stage::Completed << true;
        QTest::newRow("Cancelled") << ExportOverallProgress::Stage::Cancelled << true;
        QTest::newRow("Failed") << ExportOverallProgress::Stage::Failed << true;
    }

    // 验证非导出阶段的整体完成判断。
    /**
     * @brief 验证 overallProgressIsComplete 测试场景。
     */
    void overallProgressIsComplete() {
        QFETCH(ExportOverallProgress::Stage, stage);
        QFETCH(bool, expected);

        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        progress.currentStage = stage;
        QCOMPARE(progress.isComplete(), expected);
    }

    // 验证空预加载任务立即视为完成。
    /**
     * @brief 验证 overallProgressPreloadingCompleteWhenEmpty 测试场景。
     */
    void overallProgressPreloadingCompleteWhenEmpty() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Preloading;
        // 空的 preloadProgress（totalCount=0）视为完成
        QVERIFY(progress.isComplete());
    }

    // 验证仍有预加载项目时整体任务未完成。
    /**
     * @brief 验证 overallProgressPreloadingNotCompleteWhenPending 测试场景。
     */
    void overallProgressPreloadingNotCompleteWhenPending() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Preloading;
        progress.preloadProgress.totalCount = 5;
        progress.preloadProgress.completedCount = 3;
        QVERIFY(!progress.isComplete());
    }

    // 验证空导出类型集合视为完成。
    /**
     * @brief 验证 overallProgressExportingCompleteWhenEmpty 测试场景。
     */
    void overallProgressExportingCompleteWhenEmpty() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Exporting;
        // 空的 exportTypeProgress 视为完成
        QVERIFY(progress.isComplete());
    }

    // 验证所有导出类型完成后整体任务完成。
    /**
     * @brief 验证 overallProgressIsCompleteWhenAllTypesDone 测试场景。
     */
    void overallProgressIsCompleteWhenAllTypesDone() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Exporting;

        // type1：测试使用的配置值或状态。
        ExportTypeProgress type1;
        type1.totalCount = 5;
        type1.completedCount = 5;
        progress.exportTypeProgress[QStringLiteral("A")] = type1;

        // type2：测试使用的配置值或状态。
        ExportTypeProgress type2;
        type2.totalCount = 3;
        type2.completedCount = 3;
        progress.exportTypeProgress[QStringLiteral("B")] = type2;

        QVERIFY(progress.isComplete());
    }

    // 验证任一导出类型未完成时整体任务仍未完成。
    /**
     * @brief 验证 overallProgressIsNotCompleteWhenAnyTypePending 测试场景。
     */
    void overallProgressIsNotCompleteWhenAnyTypePending() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Exporting;

        // type1：测试使用的配置值或状态。
        ExportTypeProgress type1;
        type1.totalCount = 5;
        type1.completedCount = 5;
        progress.exportTypeProgress[QStringLiteral("A")] = type1;

        // type2：测试使用的配置值或状态。
        ExportTypeProgress type2;
        type2.totalCount = 3;
        type2.completedCount = 2;
        progress.exportTypeProgress[QStringLiteral("B")] = type2;

        QVERIFY(!progress.isComplete());
    }

    // 验证整体成功数和失败数汇总逻辑。
    /**
     * @brief 验证 overallProgressTotalSuccessAndFailedCounts 测试场景。
     */
    void overallProgressTotalSuccessAndFailedCounts() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;

        // type1：测试使用的配置值或状态。
        ExportTypeProgress type1;
        type1.successCount = 8;
        type1.failedCount = 2;
        progress.exportTypeProgress[QStringLiteral("Symbol")] = type1;

        // type2：测试使用的配置值或状态。
        ExportTypeProgress type2;
        type2.successCount = 5;
        type2.failedCount = 0;
        progress.exportTypeProgress[QStringLiteral("Footprint")] = type2;

        QCOMPARE(progress.totalSuccessCount(), 13);
        QCOMPARE(progress.totalFailedCount(), 2);
    }

    // 验证没有导出类型时成功数和失败数均为零。
    /**
     * @brief 验证 overallProgressEmptyTypesReturnsZeroCounts 测试场景。
     */
    void overallProgressEmptyTypesReturnsZeroCounts() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        QCOMPARE(progress.totalSuccessCount(), 0);
        QCOMPARE(progress.totalFailedCount(), 0);
    }
};

}  // namespace EasyKiConverter

QTEST_GUILESS_MAIN(EasyKiConverter::TestExportProgress)
#include "test_export_progress.moc"
