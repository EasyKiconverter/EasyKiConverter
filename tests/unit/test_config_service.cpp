#include "services/ConfigService.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestConfigService : public QObject {
    Q_OBJECT

private slots:
    void testCacheSettingsPersistAcrossLoad();
    void testDiskCacheLimitClamped();
    void testExportModePersistAcrossLoad();
    void testTargetFormatIdPersistAcrossLoad();
};

/**
 * @brief 验证 testCacheSettingsPersistAcrossLoad 测试场景。
 */
void TestConfigService::testCacheSettingsPersistAcrossLoad() {
    // tempDir：测试使用的文件系统路径。
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // configPath：测试使用的文件系统路径。
    const QString configPath = QDir(tempDir.path()).filePath(QStringLiteral("config.json"));
    // cacheDir：测试使用的文件系统路径。
    const QString cacheDir = QDir(tempDir.path()).filePath(QStringLiteral("custom-cache"));

    // config：测试用配置对象。
    ConfigService* config = ConfigService::instance();
    config->resetToDefaults();
    config->setCacheDir(cacheDir);
    config->setDiskCacheLimitMB(2048);
    QVERIFY(config->saveConfig(configPath));

    config->resetToDefaults();
    QVERIFY(config->loadConfig(configPath));

    QCOMPARE(config->getCacheDir(), QDir::cleanPath(cacheDir));
    QCOMPARE(config->getDiskCacheLimitMB(), 2048);
}

/**
 * @brief 验证 testDiskCacheLimitClamped 测试场景。
 */
void TestConfigService::testDiskCacheLimitClamped() {
    // config：测试用配置对象。
    ConfigService* config = ConfigService::instance();

    config->setDiskCacheLimitMB(0);
    QCOMPARE(config->getDiskCacheLimitMB(), 1);

    config->setDiskCacheLimitMB(1048577);
    QCOMPARE(config->getDiskCacheLimitMB(), 1048576);
}

/**
 * @brief 验证 testExportModePersistAcrossLoad 测试场景。
 */
void TestConfigService::testExportModePersistAcrossLoad() {
    // tempDir：测试使用的文件系统路径。
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // configPath：测试使用的文件系统路径。
    const QString configPath = QDir(tempDir.path()).filePath(QStringLiteral("config.json"));

    // config：测试用配置对象。
    ConfigService* config = ConfigService::instance();
    config->resetToDefaults();
    QCOMPARE(config->getExportMode(), 0);

    config->setExportMode(1);
    QCOMPARE(config->getExportMode(), 1);
    QVERIFY(config->saveConfig(configPath));

    config->resetToDefaults();
    QCOMPARE(config->getExportMode(), 0);

    QVERIFY(config->loadConfig(configPath));
    QCOMPARE(config->getExportMode(), 1);
}

/**
 * @brief 验证目标格式 ID 能够跨配置加载持久化。
 */
void TestConfigService::testTargetFormatIdPersistAcrossLoad() {
    // tempDir：测试使用的隔离临时目录。
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // configPath：测试使用的隔离配置文件路径。
    const QString configPath = QDir(tempDir.path()).filePath(QStringLiteral("config.json"));

    // config：测试使用的配置服务实例。
    ConfigService* config = ConfigService::instance();
    QVERIFY(config->saveConfig(configPath));
    QVERIFY(config->loadConfig(configPath));
    config->resetToDefaults();
    QCOMPARE(config->getTargetFormatId(), QStringLiteral("kicad"));

    config->setTargetFormatId(QStringLiteral("altium"));
    QVERIFY(config->saveConfig(configPath));
    QVERIFY(config->loadConfig(configPath));
    QCOMPARE(config->getTargetFormatId(), QStringLiteral("altium"));
}

QTEST_GUILESS_MAIN(TestConfigService)
#include "test_config_service.moc"
