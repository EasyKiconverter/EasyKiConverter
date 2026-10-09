#include "tests/common/TestPaths.hpp"
#include "workers/ProcessWorker.h"

#include <QSignalSpy>
#include <QTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestProcessWorker : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 验证 runParsesRealCadFixture 测试场景。
     */
    void runParsesRealCadFixture() {
        // status：测试用状态对象。
        auto status = QSharedPointer<ComponentExportStatus>::create();
        status->componentId = QStringLiteral("C23186");
        status->fetchSuccess = true;
        status->componentInfoRaw = QByteArrayLiteral(R"({"name":"0603 resistor"})");

        // error：测试期间收集的错误或诊断信息。
        QString error;
        status->cadDataRaw =
            TestPaths::readBytes(TestPaths::fixturePath(QStringLiteral("easyeda/cad_basic.json")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        ProcessWorker worker(status);
        QSignalSpy spy(&worker, &ProcessWorker::processCompleted);
        worker.run();

        QCOMPARE(spy.count(), 1);
        QVERIFY(status->processSuccess);
        QCOMPARE(status->processMessage, QStringLiteral("Process completed successfully"));
        QVERIFY(status->componentData);
        QCOMPARE(status->componentData->lcscId(), QStringLiteral("C23186"));
        QCOMPARE(status->componentData->name(), QStringLiteral("0603 resistor"));
        QVERIFY(status->symbolData);
        QCOMPARE(status->symbolData->info().name, QStringLiteral("0603WAF5101T5E"));
        QVERIFY(status->footprintData);
        QCOMPARE(status->footprintData->info().name, QStringLiteral("R0603"));
        QVERIFY(status->processDurationMs >= 0);
        QVERIFY(status->cadDataRaw.isEmpty());
        QVERIFY(status->componentInfoRaw.isEmpty());
    }

    /**
     * @brief 验证 runFailsOnInvalidComponentInfoJson 测试场景。
     */
    void runFailsOnInvalidComponentInfoJson() {
        // status：测试用状态对象。
        auto status = QSharedPointer<ComponentExportStatus>::create();
        status->componentId = QStringLiteral("C_BAD");
        status->fetchSuccess = true;
        status->componentInfoRaw = QByteArrayLiteral("{");

        ProcessWorker worker(status);
        QSignalSpy spy(&worker, &ProcessWorker::processCompleted);
        worker.run();

        QCOMPARE(spy.count(), 1);
        QVERIFY(!status->processSuccess);
        QCOMPARE(status->processMessage, QStringLiteral("Failed to parse component info"));
        QVERIFY(!status->isCancelled);
    }

    /**
     * @brief 验证 runFailsOnInvalidCadJson 测试场景。
     */
    void runFailsOnInvalidCadJson() {
        // status：测试用状态对象。
        auto status = QSharedPointer<ComponentExportStatus>::create();
        status->componentId = QStringLiteral("C_BAD_CAD");
        status->fetchSuccess = true;
        status->cadDataRaw = QByteArrayLiteral("{");

        ProcessWorker worker(status);
        QSignalSpy spy(&worker, &ProcessWorker::processCompleted);
        worker.run();

        QCOMPARE(spy.count(), 1);
        QVERIFY(!status->processSuccess);
        QCOMPARE(status->processMessage, QStringLiteral("Failed to parse CAD data"));
    }

    /**
     * @brief 验证 abortBeforeRunEmitsCancelledStatus 测试场景。
     */
    void abortBeforeRunEmitsCancelledStatus() {
        // status：测试用状态对象。
        auto status = QSharedPointer<ComponentExportStatus>::create();
        status->componentId = QStringLiteral("C_CANCEL");

        ProcessWorker worker(status);
        QSignalSpy spy(&worker, &ProcessWorker::processCompleted);
        worker.abort();
        worker.run();

        QCOMPARE(spy.count(), 1);
        QVERIFY(!status->processSuccess);
        QVERIFY(status->isCancelled);
        QCOMPARE(status->processMessage, QStringLiteral("Export cancelled"));
    }
};

QTEST_GUILESS_MAIN(TestProcessWorker)
#include "test_process_worker.moc"
