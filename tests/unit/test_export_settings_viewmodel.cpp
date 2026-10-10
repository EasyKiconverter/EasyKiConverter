#include "services/CacheSafety.h"
#include "services/ComponentCacheService.h"
#include "services/ConfigService.h"
#include "ui/viewmodels/ExportSettingsViewModel.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

namespace {
/**
 * @brief 验证 canonicalTempPath 测试场景。
 * @param tempDir 待处理的文件系统路径。
 */
QString canonicalTempPath(const QTemporaryDir& tempDir) {
    return QFileInfo(tempDir.path()).canonicalFilePath();
}
}  // namespace

class TestExportSettingsViewModel : public QObject {
    Q_OBJECT

private slots:
    void init();
    void testLoadsCacheSettingsFromConfig();
    void testRejectsUnsafeCacheDirWithoutChangingConfig();
    void testAcceptsCustomChildAndPersistsCacheDir();
    void testReportsMigrationFailureWithoutChangingConfig();
    void testReportsUnverifiableMigrationEntry();
    void testDiskCacheLimitClamped();
    void testModel3DPathModeDefaultsAndPersists();
    void testNormalizePathMode();
    void testConfigServiceModel3DPathMode();
    void testFullReplacementTargetDisablesAppendMode();
    void testHorizonAllowsAppendAndUpdateModes();

private:
    // m_tempDir：测试使用的文件系统路径。
    QTemporaryDir m_tempDir;
    // m_cachePath：与测试配置文件分离的隔离缓存根目录。
    QString m_cachePath;
};

// 为每个测试建立隔离配置和临时缓存路径。
/**
 * @brief 为测试用例初始化隔离环境。
 */
void TestExportSettingsViewModel::init() {
    QVERIFY(m_tempDir.isValid());
    // tempPath：测试使用的文件系统路径。
    const QString tempPath = canonicalTempPath(m_tempDir);
    // m_cachePath：每次测试使用的隔离缓存根目录。
    m_cachePath = QDir(tempPath).filePath(QStringLiteral("cache"));
    QVERIFY(QDir().mkpath(m_cachePath));

    // config：测试用配置对象。
    ConfigService* config = ConfigService::instance();
    // configPath：隔离测试配置文件路径。
    const QString configPath = QDir(tempPath).filePath(QStringLiteral("config/config.json"));
    QVERIFY(QDir().mkpath(QFileInfo(configPath).absolutePath()));
    QFile configSeed(configPath);
    QVERIFY(configSeed.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(configSeed.write(QByteArrayLiteral("{}")) > 0);
    configSeed.close();
    QVERIFY(config->loadConfig(configPath));
    config->resetToDefaults();
    config->setCacheDir(QDir(tempPath).filePath(QStringLiteral("configured-cache")));
    config->setDiskCacheLimitMB(4096);

    // cache：测试使用的缓存或元器件对象。
    ComponentCacheService* cache = ComponentCacheService::instance();
    // cacheError：测试期间收集的错误或诊断信息。
    QString cacheError;
    QVERIFY2(cache->setCacheDir(m_cachePath, false, &cacheError), qPrintable(cacheError));
}

// 验证视图模型从配置服务加载缓存设置。
/**
 * @brief 验证 testLoadsCacheSettingsFromConfig 测试场景。
 */
void TestExportSettingsViewModel::testLoadsCacheSettingsFromConfig() {
    ExportSettingsViewModel viewModel(nullptr);

    QCOMPARE(viewModel.cacheDir(),
             QDir::cleanPath(QDir(canonicalTempPath(m_tempDir)).filePath(QStringLiteral("configured-cache"))));
    QCOMPARE(viewModel.diskCacheLimitMB(), 4096);
}

// 拒绝非空的未托管目录，并保留已经生效的缓存配置。
/**
 * @brief 验证 testRejectsUnsafeCacheDirWithoutChangingConfig 测试场景。
 */
void TestExportSettingsViewModel::testRejectsUnsafeCacheDirWithoutChangingConfig() {
    ExportSettingsViewModel viewModel(nullptr);
    QSignalSpy rejectionSpy(&viewModel, &ExportSettingsViewModel::cacheDirChangeRejected);
    // originalPath：测试使用的文件系统路径。
    const QString originalPath = viewModel.cacheDir();
    // unsafePath：测试使用的文件系统路径。
    const QString unsafePath = QDir(m_tempDir.path()).filePath(QStringLiteral("non-empty-cache"));
    QVERIFY(QDir().mkpath(unsafePath));

    QFile userFile(QDir(unsafePath).filePath(QStringLiteral("user-data.txt")));
    QVERIFY(userFile.open(QIODevice::WriteOnly));
    QVERIFY(userFile.write("user data") > 0);
    userFile.close();

    viewModel.setCacheDir(unsafePath);

    QCOMPARE(viewModel.cacheDir(), originalPath);
    QCOMPARE(ConfigService::instance()->getCacheDir(), originalPath);
    QVERIFY(QFile::exists(userFile.fileName()));
    QVERIFY(!viewModel.status().isEmpty());
    QCOMPARE(rejectionSpy.count(), 1);
    QCOMPARE(rejectionSpy.at(0).at(0).toString(), unsafePath);
    QVERIFY(!rejectionSpy.at(0).at(1).toString().isEmpty());
}

// 验证合法的自定义子目录能够同步更新服务、配置和新建视图模型。
/**
 * @brief 验证 testAcceptsCustomChildAndPersistsCacheDir 测试场景。
 */
void TestExportSettingsViewModel::testAcceptsCustomChildAndPersistsCacheDir() {
    // cache：测试使用的缓存或元器件对象。
    ComponentCacheService* cache = ComponentCacheService::instance();
    // tempPath：测试使用的文件系统路径。
    const QString cachePath = m_cachePath;
    QVERIFY(cache->setCacheDir(cachePath, false));

    // customChild：隔离临时目录下的自定义缓存子目录。
    const QString customChild = QDir(cachePath).filePath(QStringLiteral("custom-cache"));

    ExportSettingsViewModel viewModel(nullptr);
    viewModel.setCacheDir(customChild);

    // normalized：规范化后的测试数据。
    const QString normalized = QFileInfo(customChild).absoluteFilePath();
    QCOMPARE(viewModel.cacheDir(), normalized);
    QCOMPARE(cache->cacheDir(), normalized);
    QCOMPARE(ConfigService::instance()->getCacheDir(), normalized);

    ExportSettingsViewModel reloadedViewModel(nullptr);
    QCOMPARE(reloadedViewModel.cacheDir(), normalized);

    QVERIFY(cache->setCacheDir(cachePath, false));
    ConfigService::instance()->setCacheDir(cachePath);
}

// 验证迁移失败时视图模型保留原路径并展示具体冲突原因。
/**
 * @brief 验证 testReportsMigrationFailureWithoutChangingConfig 测试场景。
 */
void TestExportSettingsViewModel::testReportsMigrationFailureWithoutChangingConfig() {
    // cache：测试使用的缓存或元器件对象。
    ComponentCacheService* cache = ComponentCacheService::instance();
    // cachePath：迁移失败测试使用的当前缓存根目录。
    const QString cachePath = m_cachePath;
    QVERIFY(cache->setCacheDir(cachePath, false));
    ConfigService::instance()->setCacheDir(cachePath);

    // data：测试使用的文本或数据内容。
    ComponentData data;
    data.setLcscId(QStringLiteral("C90001"));
    data.setName(QStringLiteral("Migration conflict"));
    cache->saveComponentMetadata(QStringLiteral("C90001"), data);

    // targetDir：测试使用的文件系统路径。
    QTemporaryDir targetDir;
    QVERIFY(targetDir.isValid());
    // targetPath：测试使用的文件系统路径。
    const QString targetPath = canonicalTempPath(targetDir);
    // ownershipError：测试期间收集的错误或诊断信息。
    QString ownershipError;
    QVERIFY2(CacheSafety::ensureOwnedRoot(targetPath, &ownershipError), qPrintable(ownershipError));
    // targetComponent：测试使用的缓存或元器件对象。
    const QString targetComponent = targetDir.filePath(QStringLiteral("C90001"));
    QVERIFY(QDir().mkpath(targetComponent));
    QFile targetMetadata(QDir(targetComponent).filePath(QStringLiteral("component.json")));
    QVERIFY(targetMetadata.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(targetMetadata.write(QByteArrayLiteral(
                "{\"lcscId\":\"C90001\",\"cacheOwner\":\"EasyKiConverter\",\"cacheEntryVersion\":1}")) > 0);
    targetMetadata.close();

    ExportSettingsViewModel viewModel(nullptr);
    QSignalSpy rejectionSpy(&viewModel, &ExportSettingsViewModel::cacheDirChangeRejected);
    viewModel.setCacheDir(targetPath);

    QCOMPARE(viewModel.cacheDir(), cachePath);
    QCOMPARE(cache->cacheDir(), cachePath);
    QCOMPARE(ConfigService::instance()->getCacheDir(), cachePath);
    QVERIFY(viewModel.status().contains(QStringLiteral("同名文件")));
    QCOMPARE(rejectionSpy.count(), 1);
    QCOMPARE(rejectionSpy.at(0).at(0).toString(), targetPath);
    QVERIFY(rejectionSpy.at(0).at(1).toString().contains(QStringLiteral("同名文件")));
    QVERIFY(QFileInfo::exists(QDir(cachePath).filePath(QStringLiteral("C90001/component.json"))));
    QVERIFY(QFileInfo::exists(targetMetadata.fileName()));
}

// 验证迁移遇到未验证组件内容时保留条目路径并传递具体诊断。
/**
 * @brief 验证 testReportsUnverifiableMigrationEntry 测试场景。
 */
void TestExportSettingsViewModel::testReportsUnverifiableMigrationEntry() {
    // cache：测试使用的缓存或元器件对象。
    ComponentCacheService* cache = ComponentCacheService::instance();
    // sourcePath：测试使用的文件系统路径。
    const QString sourcePath = m_cachePath;
    QVERIFY(cache->setCacheDir(sourcePath, false));
    ConfigService::instance()->setCacheDir(sourcePath);

    // data：测试使用的文本或数据内容。
    ComponentData data;
    data.setLcscId(QStringLiteral("C90002"));
    data.setName(QStringLiteral("Unverifiable migration entry"));
    cache->saveComponentMetadata(QStringLiteral("C90002"), data);

    // unknownPath：测试使用的文件系统路径。
    const QString unknownPath = QDir(sourcePath).filePath(QStringLiteral("C90002/user-data.bin"));
    QFile unknownFile(unknownPath);
    QVERIFY(unknownFile.open(QIODevice::WriteOnly));
    QVERIFY(unknownFile.write("user data") > 0);
    unknownFile.close();

    // targetDir：测试使用的文件系统路径。
    QTemporaryDir targetDir;
    QVERIFY(targetDir.isValid());
    // targetPath：测试使用的文件系统路径。
    const QString targetPath = canonicalTempPath(targetDir);
    // ownershipError：测试期间收集的错误或诊断信息。
    QString ownershipError;
    QVERIFY2(CacheSafety::ensureOwnedRoot(targetPath, &ownershipError), qPrintable(ownershipError));

    ExportSettingsViewModel viewModel(nullptr);
    QSignalSpy rejectionSpy(&viewModel, &ExportSettingsViewModel::cacheDirChangeRejected);
    viewModel.setCacheDir(targetPath);

    QCOMPARE(viewModel.cacheDir(), sourcePath);
    QCOMPARE(ConfigService::instance()->getCacheDir(), sourcePath);
    QCOMPARE(rejectionSpy.count(), 1);
    QVERIFY(rejectionSpy.at(0).at(1).toString().contains(QStringLiteral("缓存迁移遇到无法验证的组件内容")));
    QVERIFY(rejectionSpy.at(0).at(1).toString().contains(unknownPath));
    QVERIFY(QFileInfo::exists(unknownPath));
}

// 验证磁盘缓存上限会被限制在配置服务允许的范围内。
/**
 * @brief 验证 testDiskCacheLimitClamped 测试场景。
 */
void TestExportSettingsViewModel::testDiskCacheLimitClamped() {
    ExportSettingsViewModel viewModel(nullptr);
    QSignalSpy limitSpy(&viewModel, &ExportSettingsViewModel::diskCacheLimitMBChanged);

    viewModel.setDiskCacheLimitMB(0);
    QCOMPARE(viewModel.diskCacheLimitMB(), 1);
    QCOMPARE(ConfigService::instance()->getDiskCacheLimitMB(), 1);

    viewModel.setDiskCacheLimitMB(1048577);
    QCOMPARE(viewModel.diskCacheLimitMB(), 1048576);
    QCOMPARE(ConfigService::instance()->getDiskCacheLimitMB(), 1048576);
    QCOMPARE(limitSpy.count(), 2);
}

// 验证磁盘缓存路径模式会规范化非法输入并持久化合法选择。
/**
 * @brief 验证 testModel3DPathModeDefaultsAndPersists 测试场景。
 */
void TestExportSettingsViewModel::testModel3DPathModeDefaultsAndPersists() {
    ExportSettingsViewModel viewModel(nullptr);
    QSignalSpy pathModeSpy(&viewModel, &ExportSettingsViewModel::exportModel3DPathModeChanged);

    QCOMPARE(viewModel.exportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);

    viewModel.setExportModel3DPathMode(ExportOptions::MODEL_3D_PATH_ABSOLUTE);
    QCOMPARE(viewModel.exportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_ABSOLUTE);
    QCOMPARE(ConfigService::instance()->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_ABSOLUTE);

    viewModel.setExportModel3DPathMode(999);
    QCOMPARE(viewModel.exportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(ConfigService::instance()->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(pathModeSpy.count(), 2);
}

// 验证路径模式规范化不会把未知枚举值传播到配置层。
/**
 * @brief 验证 testNormalizePathMode 测试场景。
 */
void TestExportSettingsViewModel::testNormalizePathMode() {
    QCOMPARE(ExportOptions::normalizePathMode(0), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(ExportOptions::normalizePathMode(1), ExportOptions::MODEL_3D_PATH_ABSOLUTE);
    QCOMPARE(ExportOptions::normalizePathMode(999), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(ExportOptions::normalizePathMode(-1), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(ExportOptions::normalizePathMode(2), ExportOptions::MODEL_3D_PATH_RELATIVE);
}

// 验证配置服务对三维模型路径模式的读写边界保持稳定。
/**
 * @brief 验证 testConfigServiceModel3DPathMode 测试场景。
 */
void TestExportSettingsViewModel::testConfigServiceModel3DPathMode() {
    // config：测试用配置对象。
    ConfigService* config = ConfigService::instance();

    QCOMPARE(config->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);

    config->setExportModel3DPathMode(ExportOptions::MODEL_3D_PATH_ABSOLUTE);
    QCOMPARE(config->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_ABSOLUTE);

    config->setExportModel3DPathMode(ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(config->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);

    config->setExportModel3DPathMode(42);
    QCOMPARE(config->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);
}

// 验证 LibrePCB 等完整重建目标不会接受追加模式。
/**
 * @brief 验证 testFullReplacementTargetDisablesAppendMode 测试场景。
 */
void TestExportSettingsViewModel::testFullReplacementTargetDisablesAppendMode() {
    // targetModel：测试使用的配置值或状态。
    ExportTargetModel targetModel;
    // pluginConfigPath：测试使用的文件系统路径。
    const QString pluginConfigPath = m_tempDir.filePath(QStringLiteral("export_plugins.json"));
    QFile pluginConfig(pluginConfigPath);
    QVERIFY(pluginConfig.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(pluginConfig.write(QByteArrayLiteral(
                R"({"plugins":[{"id":"kicad","displayName":"KiCad"},{"id":"librepcb","displayName":"LibrePCB"}]})")) >
            0);
    pluginConfig.close();
    targetModel.loadPlugins(pluginConfigPath);
    // targets：测试目标格式集合。
    const QVariantList targets = targetModel.availableTargets();
    // librePcbIndex：测试使用的数量或索引值。
    int librePcbIndex = -1;
    for (int i = 0; i < targets.size(); ++i) {
        if (targets.at(i).toMap().value(QStringLiteral("id")).toString() == QStringLiteral("librepcb")) {
            librePcbIndex = i;
            break;
        }
    }
    QVERIFY(librePcbIndex >= 0);

    ExportSettingsViewModel viewModel(nullptr);
    viewModel.setTargetModel(&targetModel);
    targetModel.setCurrentIndex(librePcbIndex);

    QVERIFY(viewModel.requiresFullReplacement());
    QCOMPARE(viewModel.exportMode(), 2);
    viewModel.setExportMode(0);
    QCOMPARE(viewModel.exportMode(), 2);
    viewModel.setExportMode(1);
    QCOMPARE(viewModel.exportMode(), 2);
}

// 验证 Horizon Pool 的临时重建策略允许追加和更新模式。
/**
 * @brief 验证 testHorizonAllowsAppendAndUpdateModes 测试场景。
 */
void TestExportSettingsViewModel::testHorizonAllowsAppendAndUpdateModes() {
    // targetModel：测试使用的配置值或状态。
    ExportTargetModel targetModel;
    // pluginConfigPath：测试使用的文件系统路径。
    const QString pluginConfigPath = m_tempDir.filePath(QStringLiteral("export_plugins_horizon.json"));
    QFile pluginConfig(pluginConfigPath);
    QVERIFY(pluginConfig.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(pluginConfig.write(QByteArrayLiteral(
                R"({"plugins":[{"id":"kicad","displayName":"KiCad"},{"id":"horizon","displayName":"Horizon EDA"}]})")) >
            0);
    pluginConfig.close();
    targetModel.loadPlugins(pluginConfigPath);
    // targets：测试目标格式集合。
    const QVariantList targets = targetModel.availableTargets();
    // horizonIndex：测试使用的数量或索引值。
    int horizonIndex = -1;
    for (int i = 0; i < targets.size(); ++i) {
        if (targets.at(i).toMap().value(QStringLiteral("id")).toString() == QStringLiteral("horizon")) {
            horizonIndex = i;
            break;
        }
    }
    QVERIFY(horizonIndex >= 0);

    ExportSettingsViewModel viewModel(nullptr);
    viewModel.setTargetModel(&targetModel);
    targetModel.setCurrentIndex(horizonIndex);

    QVERIFY(!viewModel.requiresFullReplacement());
    viewModel.setExportMode(0);
    QCOMPARE(viewModel.exportMode(), 0);
    viewModel.setExportMode(1);
    QCOMPARE(viewModel.exportMode(), 1);
}

QTEST_GUILESS_MAIN(TestExportSettingsViewModel)
#include "test_export_settings_viewmodel.moc"

/*
 * 我該怎麼說我現在的感覺，很平靜但也很難過，平靜是因為我終於接受了，
 * 某些無法改變的事實，而難過也恰恰來自這份平靜，我就這麼接受了，
 * 已經分不清自己是沒招了，還是釋懷了，
 * 從前我認為有問題就該溝通，直到有一天我也變成啞巴了。
 */
