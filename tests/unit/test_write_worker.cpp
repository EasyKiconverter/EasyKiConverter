#include "workers/WriteWorker.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestWriteWorker : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 验证 runWritesSymbolFootprintPreviewAndDatasheet 测试场景。
     */
    void runWritesSymbolFootprintPreviewAndDatasheet() {
        // outputDir：测试使用的文件系统路径。
        QTemporaryDir outputDir;
        QVERIFY(outputDir.isValid());
        // tempDir：测试使用的文件系统路径。
        const QString tempDir = QDir(outputDir.path()).filePath(QStringLiteral("tmp"));
        QVERIFY(QDir().mkpath(tempDir));

        // status：测试用状态对象。
        auto status = makeReadyStatus();
        status->symbolData = makeSymbol();
        status->footprintData = makeFootprint();
        status->previewImageDataList = {QByteArrayLiteral("jpg-bytes")};
        status->datasheetData = QByteArrayLiteral("%PDF-1.4\n");

        WriteWorker worker(
            status, outputDir.path(), QStringLiteral("WorkerLib"), true, true, false, true, true, false, tempDir);
        QSignalSpy spy(&worker, &WriteWorker::writeCompleted);
        worker.run();

        QCOMPARE(spy.count(), 1);
        QVERIFY(status->writeSuccess);
        QVERIFY(status->symbolWritten);
        QVERIFY(status->footprintWritten);
        QVERIFY(status->previewImageWritten);
        QVERIFY(status->datasheetWritten);
        QVERIFY(QFileInfo::exists(QDir(tempDir).filePath(QStringLiteral("C777.kicad_sym"))));
        QVERIFY(
            QFileInfo::exists(QDir(outputDir.path()).filePath(QStringLiteral("WorkerLib.pretty/WORKER_FP.kicad_mod"))));
        QVERIFY(QFileInfo::exists(QDir(outputDir.path()).filePath(QStringLiteral("images/C777_0.jpg"))));
        QVERIFY(QFileInfo::exists(QDir(outputDir.path()).filePath(QStringLiteral("datasheets/C777.pdf"))));
    }

    /**
     * @brief 验证 runSkipsWhenPreviousStageFailed 测试场景。
     */
    void runSkipsWhenPreviousStageFailed() {
        // outputDir：测试使用的文件系统路径。
        QTemporaryDir outputDir;
        QVERIFY(outputDir.isValid());

        // status：测试用状态对象。
        auto status = makeReadyStatus();
        status->fetchSuccess = false;

        WriteWorker worker(status,
                           outputDir.path(),
                           QStringLiteral("WorkerLib"),
                           true,
                           false,
                           false,
                           false,
                           false,
                           false,
                           outputDir.path());
        QSignalSpy spy(&worker, &WriteWorker::writeCompleted);
        worker.run();

        QCOMPARE(spy.count(), 1);
        QVERIFY(!status->writeSuccess);
        QCOMPARE(status->writeMessage, QStringLiteral("Skipped writing due to previous stage failure"));
    }

    /**
     * @brief 验证 abortBeforeRunEmitsCancelledStatus 测试场景。
     */
    void abortBeforeRunEmitsCancelledStatus() {
        // outputDir：测试使用的文件系统路径。
        QTemporaryDir outputDir;
        QVERIFY(outputDir.isValid());

        // status：测试用状态对象。
        auto status = makeReadyStatus();
        WriteWorker worker(status,
                           outputDir.path(),
                           QStringLiteral("WorkerLib"),
                           false,
                           false,
                           false,
                           false,
                           false,
                           false,
                           outputDir.path());
        QSignalSpy spy(&worker, &WriteWorker::writeCompleted);
        worker.abort();
        worker.run();

        QCOMPARE(spy.count(), 1);
        QVERIFY(!status->writeSuccess);
        QVERIFY(status->isCancelled);
        QCOMPARE(status->writeMessage, QStringLiteral("Export cancelled"));
    }

private:
    /**
     * @brief 验证 makeReadyStatus 测试场景。
     */
    QSharedPointer<ComponentExportStatus> makeReadyStatus() const {
        // status：测试用状态对象。
        auto status = QSharedPointer<ComponentExportStatus>::create();
        status->componentId = QStringLiteral("C777");
        status->fetchSuccess = true;
        status->processSuccess = true;
        return status;
    }

    /**
     * @brief 验证 makeSymbol 测试场景。
     */
    QSharedPointer<SymbolData> makeSymbol() const {
        // symbol：测试用符号数据。
        auto symbol = QSharedPointer<SymbolData>::create();
        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("WORKER_SYMBOL");
        info.prefix = QStringLiteral("U");
        info.package = QStringLiteral("WORKER_FP");
        symbol->setInfo(info);
        symbol->setBbox({0, 0, 0, 0});
        return symbol;
    }

    /**
     * @brief 验证 makeFootprint 测试场景。
     */
    QSharedPointer<FootprintData> makeFootprint() const {
        // footprint：测试用封装数据。
        auto footprint = QSharedPointer<FootprintData>::create();
        // info：测试用元数据对象。
        FootprintInfo info;
        info.name = QStringLiteral("WORKER_FP");
        footprint->setInfo(info);
        footprint->setBbox({0, 0, 0, 0});

        FootprintPad pad{};
        pad.shape = QStringLiteral("rect");
        pad.centerX = 0;
        pad.centerY = 0;
        pad.width = 10;
        pad.height = 5;
        pad.layerId = 1;
        pad.number = QStringLiteral("1");
        footprint->addPad(pad);
        return footprint;
    }
};

QTEST_GUILESS_MAIN(TestWriteWorker)
#include "test_write_worker.moc"
