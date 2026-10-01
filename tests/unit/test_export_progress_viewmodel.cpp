#include "services/export/ExportProgress.h"
#include "services/export/ParallelExportService.h"
#include "ui/viewmodels/ExportProgressViewModel.h"

#include <QMetaType>
#include <QtTest/QtTest>

namespace EasyKiConverter {

class TestExportProgressViewModel : public QObject {
    Q_OBJECT

private slots:

    void initTestCase() {
        qRegisterMetaType<ExportOverallProgress>();
        qRegisterMetaType<ExportItemStatus>();
    }

    void fullReplacementTargetsNormalizeUpdateMode() {
        ParallelExportService service;
        ExportProgressViewModel viewModel(&service, nullptr, nullptr);

        viewModel.startExport({"C2040"},
                              "/tmp/easykiconverter-test",
                              "testlib",
                              false,
                              true,
                              false,
                              0,
                              0,
                              false,
                              false,
                              false,
                              true,
                              false,
                              QString(),
                              QString(),
                              QString(),
                              static_cast<int>(TargetEdaFormat::LibrePcb));

        const ExportOptions options = service.options();
        QVERIFY(options.overwriteExistingFiles);
        QVERIFY(!options.updateMode);
    }

    void stageProgressRespectsPipelineOrdering() {
        ParallelExportService service;
        ExportProgressViewModel viewModel(&service, nullptr, nullptr);

        viewModel.startExport({"C1", "C2", "C3", "C4"},
                              "/tmp/easykiconverter-test",
                              "testlib",
                              true,
                              true,
                              true,
                              0,
                              0,
                              true,
                              false,
                              false,
                              false,
                              false);

        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Exporting;
        progress.totalComponents = 4;

        ExportTypeProgress preview;
        preview.typeName = QStringLiteral("PreviewImages");
        preview.totalCount = 4;
        preview.completedCount = 2;
        progress.exportTypeProgress.insert(preview.typeName, preview);

        ExportTypeProgress symbol;
        symbol.typeName = QStringLiteral("Symbol");
        symbol.totalCount = 4;
        symbol.completedCount = 4;
        progress.exportTypeProgress.insert(symbol.typeName, symbol);

        ExportTypeProgress footprint;
        footprint.typeName = QStringLiteral("Footprint");
        footprint.totalCount = 4;
        footprint.completedCount = 4;
        progress.exportTypeProgress.insert(footprint.typeName, footprint);

        ExportTypeProgress model3D;
        model3D.typeName = QStringLiteral("Model3D");
        model3D.totalCount = 4;
        model3D.completedCount = 4;
        progress.exportTypeProgress.insert(model3D.typeName, model3D);

        const bool invoked = QMetaObject::invokeMethod(
            &viewModel, "handleProgressChanged", Qt::DirectConnection, Q_ARG(ExportOverallProgress, progress));
        QVERIFY(invoked);

        QCOMPARE(viewModel.fetchProgress(), 75);
        QCOMPARE(viewModel.processProgress(), 75);
        QCOMPARE(viewModel.writeProgress(), 75);
        QCOMPARE(viewModel.progress(), 75);
    }

    void itemStatusesAggregateToComponentAndTypeCounts() {
        ParallelExportService service;
        ExportProgressViewModel viewModel(&service, nullptr, nullptr);

        viewModel.startExport({"C1", "C2"},
                              "/tmp/easykiconverter-test",
                              "testlib",
                              true,
                              true,
                              true,
                              0,
                              0,
                              false,
                              false,
                              false,
                              false,
                              false);

        auto makeStatus = [](ExportItemStatus::Status state, const QString& error = QString()) {
            ExportItemStatus status;
            status.status = state;
            status.errorMessage = error;
            return status;
        };

        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C1")),
                                          Q_ARG(QString, QStringLiteral("Symbol")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));
        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C1")),
                                          Q_ARG(QString, QStringLiteral("Footprint")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));
        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C1")),
                                          Q_ARG(QString, QStringLiteral("Model3D")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));

        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C2")),
                                          Q_ARG(QString, QStringLiteral("Symbol")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));
        QVERIFY(QMetaObject::invokeMethod(
            &viewModel,
            "handleItemStatusChanged",
            Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("C2")),
            Q_ARG(QString, QStringLiteral("Footprint")),
            Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Failed, QStringLiteral("broken")))));
        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C2")),
                                          Q_ARG(QString, QStringLiteral("Model3D")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));

        QVERIFY(QMetaObject::invokeMethod(&viewModel, "flushPendingUpdates", Qt::DirectConnection));

        QCOMPARE(viewModel.successCount(), 1);
        QCOMPARE(viewModel.failureCount(), 1);
        QCOMPARE(viewModel.symbolSuccessCount(), 2);
        QCOMPARE(viewModel.footprintSuccessCount(), 1);
        QCOMPARE(viewModel.model3DSuccessCount(), 2);
        QCOMPARE(viewModel.filteredPendingCount(), 0);

        const QVariantList results = viewModel.resultsList();
        QCOMPARE(results.size(), 2);

        const QVariantMap first = results.at(0).toMap();
        QCOMPARE(first.value("status").toString(), QStringLiteral("success"));

        const QVariantMap second = results.at(1).toMap();
        QCOMPARE(second.value("status").toString(), QStringLiteral("failed"));
        QCOMPARE(second.value("footprintStatus").toString(), QStringLiteral("failed"));
        QCOMPARE(second.value("error").toString(), QStringLiteral("broken"));
    }

    void cancelExportMarksPendingItemsCancelledAndResetsState() {
        ParallelExportService service;
        ExportProgressViewModel viewModel(&service, nullptr, nullptr);
        QSignalSpy stoppingSpy(&viewModel, &ExportProgressViewModel::isStoppingChanged);
        QSignalSpy exportingSpy(&viewModel, &ExportProgressViewModel::isExportingChanged);

        viewModel.startExport({"C1", "C2"},
                              "/tmp/easykiconverter-test",
                              "testlib",
                              true,
                              true,
                              false,
                              0,
                              0,
                              false,
                              false,
                              false,
                              false,
                              false);

        QVERIFY(viewModel.isExporting());
        QCOMPARE(viewModel.resultsList().size(), 2);

        viewModel.cancelExport();

        QCOMPARE(viewModel.isStopping(), false);
        QCOMPARE(viewModel.isExporting(), false);
        QCOMPARE(viewModel.hasCompletedExport(), false);
        QCOMPARE(viewModel.progress(), 0);
        QCOMPARE(viewModel.status(), QStringLiteral("Export cancelled"));
        QVERIFY(stoppingSpy.count() >= 2);
        QVERIFY(exportingSpy.count() >= 2);

        const QVariantList results = viewModel.resultsList();
        QCOMPARE(results.size(), 2);
        for (const QVariant& resultVariant : results) {
            const QVariantMap result = resultVariant.toMap();
            QCOMPARE(result.value("status").toString(), QStringLiteral("failed"));
            QCOMPARE(result.value("symbolStatus").toString(), QStringLiteral("failed"));
            QCOMPARE(result.value("footprintStatus").toString(), QStringLiteral("failed"));
            QCOMPARE(result.value("model3DStatus").toString(), QStringLiteral("disabled"));
            QCOMPARE(result.value("error").toString(), QStringLiteral("Export cancelled"));
        }
    }

    void retryFailedComponentsResetsOnlyFailedItems() {
        ParallelExportService service;
        ExportProgressViewModel viewModel(&service, nullptr, nullptr);

        viewModel.startExport({"C1", "C2", "C3"},
                              "/tmp/easykiconverter-test",
                              "testlib",
                              true,
                              true,
                              false,
                              0,
                              0,
                              false,
                              false,
                              false,
                              false,
                              false);

        auto makeStatus = [](ExportItemStatus::Status state, const QString& error = QString()) {
            ExportItemStatus status;
            status.status = state;
            status.errorMessage = error;
            return status;
        };

        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C1")),
                                          Q_ARG(QString, QStringLiteral("Symbol")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));
        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C1")),
                                          Q_ARG(QString, QStringLiteral("Footprint")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));
        QVERIFY(QMetaObject::invokeMethod(
            &viewModel,
            "handleItemStatusChanged",
            Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("C2")),
            Q_ARG(QString, QStringLiteral("Symbol")),
            Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Failed, QStringLiteral("symbol failed")))));
        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C2")),
                                          Q_ARG(QString, QStringLiteral("Footprint")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));
        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C3")),
                                          Q_ARG(QString, QStringLiteral("Symbol")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));
        QVERIFY(QMetaObject::invokeMethod(
            &viewModel,
            "handleItemStatusChanged",
            Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("C3")),
            Q_ARG(QString, QStringLiteral("Footprint")),
            Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Failed, QStringLiteral("footprint failed")))));
        QVERIFY(QMetaObject::invokeMethod(&viewModel, "flushPendingUpdates", Qt::DirectConnection));

        QCOMPARE(viewModel.successCount(), 1);
        QCOMPARE(viewModel.failureCount(), 2);

        viewModel.retryFailedComponents();

        QCOMPARE(viewModel.successCount(), 1);
        QCOMPARE(viewModel.failureCount(), 0);
        QCOMPARE(viewModel.filteredPendingCount(), 2);

        const QVariantList results = viewModel.resultsList();
        QCOMPARE(results.size(), 3);

        const QVariantMap success = results.at(0).toMap();
        QCOMPARE(success.value("componentId").toString(), QStringLiteral("C1"));
        QCOMPARE(success.value("status").toString(), QStringLiteral("success"));
        QCOMPARE(success.value("symbolStatus").toString(), QStringLiteral("success"));
        QCOMPARE(success.value("footprintStatus").toString(), QStringLiteral("success"));

        const QVariantMap retrySymbol = results.at(1).toMap();
        QCOMPARE(retrySymbol.value("componentId").toString(), QStringLiteral("C2"));
        QCOMPARE(retrySymbol.value("status").toString(), QStringLiteral("pending"));
        QCOMPARE(retrySymbol.value("symbolStatus").toString(), QStringLiteral("pending"));
        QCOMPARE(retrySymbol.value("footprintStatus").toString(), QStringLiteral("pending"));
        QCOMPARE(retrySymbol.value("error").toString(), QString());

        const QVariantMap retryFootprint = results.at(2).toMap();
        QCOMPARE(retryFootprint.value("componentId").toString(), QStringLiteral("C3"));
        QCOMPARE(retryFootprint.value("status").toString(), QStringLiteral("pending"));
        QCOMPARE(retryFootprint.value("symbolStatus").toString(), QStringLiteral("pending"));
        QCOMPARE(retryFootprint.value("footprintStatus").toString(), QStringLiteral("pending"));
        QCOMPARE(retryFootprint.value("error").toString(), QString());
    }
};

}  // namespace EasyKiConverter

QTEST_GUILESS_MAIN(EasyKiConverter::TestExportProgressViewModel)
#include "test_export_progress_viewmodel.moc"
