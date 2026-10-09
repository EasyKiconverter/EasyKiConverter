#include "../common/MockNetworkClient.hpp"
#include "../common/TestPaths.hpp"
#include "services/CadDataLoader.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestCadDataLoader : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 验证 fetchAndParseCadDataSuccess 测试场景。
     */
    void fetchAndParseCadDataSuccess() {
        // mock：测试用模拟对象。
        MockNetworkClient mock;
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C23186");
        mock.addJsonResponse(componentUrl(componentId),
                             responseWithResult(readCadFixture(QStringLiteral("cad_basic"))));

        // result：测试得到的实际结果数据。
        const CadFetchTaskResult result = CadDataLoader::fetchAndParseCadData(componentId, &mock);

        QVERIFY(result.success);
        QVERIFY(result.parsed.success);
        QCOMPARE(result.componentId, componentId);
        QCOMPARE(result.parsed.componentId, componentId);
        QCOMPARE(result.parsed.componentData.lcscId(), componentId);
        QCOMPARE(result.parsed.componentData.name(), QStringLiteral("0603WAF5101T5E"));
        QVERIFY(result.parsed.componentData.symbolData());
        QCOMPARE(result.parsed.componentData.symbolData()->info().name, QStringLiteral("0603WAF5101T5E"));
        QVERIFY(result.parsed.componentData.footprintData());
        QCOMPARE(result.parsed.componentData.footprintData()->info().name, QStringLiteral("R0603"));
        QVERIFY(result.parsed.componentData.model3DData());
        QVERIFY(!result.parsed.componentData.model3DData()->uuid().isEmpty());
        QVERIFY(result.errorMessage.isEmpty());
    }

    /**
     * @brief 验证 fetchAndParseCadDataReportsSuccessFalse 测试场景。
     */
    void fetchAndParseCadDataReportsSuccessFalse() {
        // mock：测试用模拟对象。
        MockNetworkClient mock;
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C_BAD");

        // response：模拟网络响应数据。
        QJsonObject response;
        response.insert(QStringLiteral("success"), false);
        response.insert(QStringLiteral("code"), 500);
        response.insert(QStringLiteral("message"), QStringLiteral("temporary backend failure"));
        mock.addJsonResponse(componentUrl(componentId), response, 500);

        // result：测试得到的实际结果数据。
        const CadFetchTaskResult result = CadDataLoader::fetchAndParseCadData(componentId, &mock);

        QVERIFY(!result.success);
        QCOMPARE(result.componentId, componentId);
        QCOMPARE(result.errorMessage, QStringLiteral("temporary backend failure"));
    }

    /**
     * @brief 验证 fetchAndParseCadDataReports404AsMissingComponent 测试场景。
     */
    void fetchAndParseCadDataReports404AsMissingComponent() {
        // mock：测试用模拟对象。
        MockNetworkClient mock;
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C404");

        // response：模拟网络响应数据。
        QJsonObject response;
        response.insert(QStringLiteral("success"), false);
        response.insert(QStringLiteral("code"), 404);
        response.insert(QStringLiteral("message"), QStringLiteral("not found"));
        mock.addJsonResponse(componentUrl(componentId), response, 404);

        // result：测试得到的实际结果数据。
        const CadFetchTaskResult result = CadDataLoader::fetchAndParseCadData(componentId, &mock);

        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QStringLiteral("元器件不存在（404）"));
    }

    /**
     * @brief 验证 fetchAndParseCadDataReportsMissingResult 测试场景。
     */
    void fetchAndParseCadDataReportsMissingResult() {
        // mock：测试用模拟对象。
        MockNetworkClient mock;
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C_MISSING_RESULT");

        // response：模拟网络响应数据。
        QJsonObject response;
        response.insert(QStringLiteral("success"), true);
        mock.addJsonResponse(componentUrl(componentId), response);

        // result：测试得到的实际结果数据。
        const CadFetchTaskResult result = CadDataLoader::fetchAndParseCadData(componentId, &mock);

        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QStringLiteral("API response missing result field"));
    }

    /**
     * @brief 验证 fetchAndParseCadDataReportsEmptyResult 测试场景。
     */
    void fetchAndParseCadDataReportsEmptyResult() {
        // mock：测试用模拟对象。
        MockNetworkClient mock;
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C_EMPTY_RESULT");

        // response：模拟网络响应数据。
        QJsonObject response;
        response.insert(QStringLiteral("success"), true);
        response.insert(QStringLiteral("result"), QJsonObject{});
        mock.addJsonResponse(componentUrl(componentId), response);

        // result：测试得到的实际结果数据。
        const CadFetchTaskResult result = CadDataLoader::fetchAndParseCadData(componentId, &mock);

        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QStringLiteral("Empty CAD data"));
    }

    /**
     * @brief 验证 fetchAndParseCadDataReportsInvalidJson 测试场景。
     */
    void fetchAndParseCadDataReportsInvalidJson() {
        // mock：测试用模拟对象。
        MockNetworkClient mock;
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C_INVALID_JSON");
        mock.addResponse(componentUrl(componentId), QByteArrayLiteral("{"));

        // result：测试得到的实际结果数据。
        const CadFetchTaskResult result = CadDataLoader::fetchAndParseCadData(componentId, &mock);

        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QStringLiteral("Failed to parse CAD response JSON"));
    }

private:
    /**
     * @brief 验证 componentUrl 测试场景。
     * @param componentId 测试使用的标识或名称。
     */
    QString componentUrl(const QString& componentId) const {
        return QStringLiteral("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(componentId);
    }

    /**
     * @brief 验证 readCadFixture 测试场景。
     * @param name 测试使用的标识或名称。
     */
    QJsonObject readCadFixture(const QString& name) const {
        // error：测试期间收集的错误或诊断信息。
        QString error;
        const QByteArray data =
            TestPaths::readBytes(TestPaths::fixturePath(QStringLiteral("easyeda/%1.json").arg(name)), &error);
        if (!error.isEmpty()) {
            qFatal("%s", qPrintable(error));
        }

        // parseError：测试期间收集的错误或诊断信息。
        QJsonParseError parseError;
        // doc：测试用数据手册对象。
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            qFatal("Failed to parse CAD fixture");
        }
        return doc.object();
    }

    /**
     * @brief 验证 responseWithResult 测试场景。
     * @param result 用于接收测试结果的输出对象。
     */
    QJsonObject responseWithResult(const QJsonObject& result) const {
        // response：模拟网络响应数据。
        QJsonObject response;
        response.insert(QStringLiteral("success"), true);
        response.insert(QStringLiteral("result"), result);
        return response;
    }
};

QTEST_GUILESS_MAIN(TestCadDataLoader)
#include "test_cad_data_loader.moc"
