#include "core/easyeda/EasyedaApi.h"
#include "tests/common/MockNetworkClient.hpp"

#include <QSignalSpy>
#include <QtTest>

#include <memory>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestEasyedaApi : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 为测试用例初始化隔离环境。
     */
    void init() {
        m_mockClient = std::make_unique<MockNetworkClient>();
        m_api = std::make_unique<EasyedaApi>(m_mockClient.get());
    }

    /**
     * @brief 验证 testFetchComponentInfo_Success 测试场景。
     */
    void testFetchComponentInfo_Success() {
        // lcscId：测试使用的元器件标识或名称。
        const QString lcscId = QStringLiteral("C12345");
        const QString expectedUrl = QStringLiteral("https://easyeda.com/api/products/C12345/components?version=6.5.51");

        // mockResponse：模拟网络响应数据。
        QJsonObject mockResponse;
        mockResponse.insert(QStringLiteral("success"), true);
        mockResponse.insert(QStringLiteral("lcscId"), lcscId);
        m_mockClient->addJsonResponse(expectedUrl, mockResponse);

        QSignalSpy spy(m_api.get(), &EasyedaApi::componentInfoFetched);
        m_api->fetchComponentInfo(lcscId);

        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(m_mockClient->lastUrl(), expectedUrl);

        QCOMPARE(spy.at(0).at(0).toString(), lcscId);
        // result：测试得到的实际结果数据。
        const QJsonObject result = spy.at(0).at(1).toJsonObject();
        QCOMPARE(result.value(QStringLiteral("lcscId")).toString(), lcscId);
    }

    /**
     * @brief 验证 testFetchCadData_NoResult 测试场景。
     */
    void testFetchCadData_NoResult() {
        // lcscId：测试使用的元器件标识或名称。
        const QString lcscId = QStringLiteral("C2041");
        const QString expectedUrl = QStringLiteral("https://easyeda.com/api/products/C2041/components?version=6.5.51");

        // mockResponse：模拟网络响应数据。
        QJsonObject mockResponse;
        mockResponse.insert(QStringLiteral("success"), true);
        m_mockClient->addJsonResponse(expectedUrl, mockResponse);

        QSignalSpy spy(m_api.get(), qOverload<const QString&, const QString&>(&EasyedaApi::fetchError));
        m_api->fetchCadData(lcscId);

        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), lcscId);
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("No result"));
    }

    /**
     * @brief 验证 testFetchComponentInfo_InvalidId 测试场景。
     */
    void testFetchComponentInfo_InvalidId() {
        QSignalSpy spy(m_api.get(), qOverload<const QString&>(&EasyedaApi::fetchError));
        m_api->fetchComponentInfo(QStringLiteral("INVALID"));

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains(QStringLiteral("Invalid LCSC ID format")));
    }

private:
    // m_mockClient：测试用模拟网络客户端。
    std::unique_ptr<MockNetworkClient> m_mockClient;
    // m_api：测试用 EasyEDA API 客户端。
    std::unique_ptr<EasyedaApi> m_api;
};

QTEST_GUILESS_MAIN(TestEasyedaApi)
#include "test_easyeda_api.moc"
