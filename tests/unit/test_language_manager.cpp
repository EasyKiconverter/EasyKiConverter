#include "core/LanguageManager.h"
#include "services/ConfigService.h"

#include <QCoreApplication>
#include <QtTest>

using namespace EasyKiConverter;

class TestLanguageManager : public QObject {
    Q_OBJECT

private slots:
    void testForceOverrideDuringInitialization();
    void testLanguageFromConfig();
};

/**
 * @brief 验证 testForceOverrideDuringInitialization 测试场景。
 */
void TestLanguageManager::testForceOverrideDuringInitialization() {
    // 设置配置文件语言为中文
    ConfigService::instance()->setLanguage("zh_CN");

    // LanguageManager 从配置加载 zh_CN
    auto* lang = LanguageManager::instance();
    QCOMPARE(lang->currentLanguage(), QStringLiteral("zh_CN"));

    // force=true 应该允许在初始化保护期内覆盖
    lang->setLanguage("en", /*force=*/true);
    QCOMPARE(lang->currentLanguage(), QStringLiteral("en"));
}

/**
 * @brief 验证 testLanguageFromConfig 测试场景。
 */
void TestLanguageManager::testLanguageFromConfig() {
    // 设置配置文件语言为英文
    ConfigService::instance()->setLanguage("en");

    // LanguageManager 应该从 ConfigService 加载语言
    auto* lang = LanguageManager::instance();
    QCOMPARE(lang->currentLanguage(), QStringLiteral("en"));
}

QTEST_GUILESS_MAIN(TestLanguageManager)
#include "test_language_manager.moc"
