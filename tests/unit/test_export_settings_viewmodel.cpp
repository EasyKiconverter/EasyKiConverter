#include "services/CacheSafety.h"
#include "services/ComponentCacheService.h"
#include "services/ConfigService.h"
#include "ui/viewmodels/ExportSettingsViewModel.h"

#include <QCoreApplication>
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
    void testAcceptsHomeChildAndPersistsCacheDir();
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
};

// 为每个测试建立隔离配置和临时缓存路径。
/**
 * @brief 为测试用例初始化隔离环境。
 */
void TestExportSettingsViewModel::init() {
    QVERIFY(m_tempDir.isValid());
    // tempPath：测试使用的文件系统路径。
    const QString tempPath = canonicalTempPath(m_tempDir);

    // config：测试用配置对象。
    ConfigService* config = ConfigService::instance();
    config->resetToDefaults();
    config->setCacheDir(QDir(tempPath).filePath(QStringLiteral("configured-cache")));
    config->setDiskCacheLimitMB(4096);

    // cache：测试使用的缓存或元器件对象。
    ComponentCacheService* cache = ComponentCacheService::instance();
    // cacheError：测试期间收集的错误或诊断信息。
    QString cacheError;
    QVERIFY2(cache->setCacheDir(tempPath, false, &cacheError), qPrintable(cacheError));
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

// 验证合法的主目录子目录能够同步更新服务、配置和新建视图模型。
/**
 * @brief 验证 testAcceptsHomeChildAndPersistsCacheDir 测试场景。
 */
void TestExportSettingsViewModel::testAcceptsHomeChildAndPersistsCacheDir() {
    // cache：测试使用的缓存或元器件对象。
    ComponentCacheService* cache = ComponentCacheService::instance();
    // tempPath：测试使用的文件系统路径。
    const QString tempPath = canonicalTempPath(m_tempDir);
    QVERIFY(cache->setCacheDir(tempPath, false));

    const QString homeChild =
        QDir(QDir::homePath())
            .filePath(QStringLiteral(".easykiconverter-viewmodel-cache-%1").arg(QCoreApplication::applicationPid()));
    QDir(homeChild).removeRecursively();

    ExportSettingsViewModel viewModel(nullptr);
    viewModel.setCacheDir(homeChild);

    // normalized：规范化后的测试数据。
    const QString normalized = QFileInfo(homeChild).absoluteFilePath();
    QCOMPARE(viewModel.cacheDir(), normalized);
    QCOMPARE(cache->cacheDir(), normalized);
    QCOMPARE(ConfigService::instance()->getCacheDir(), normalized);

    ExportSettingsViewModel reloadedViewModel(nullptr);
    QCOMPARE(reloadedViewModel.cacheDir(), normalized);

    QVERIFY(cache->setCacheDir(tempPath, false));
    ConfigService::instance()->setCacheDir(tempPath);
    QVERIFY(QDir(homeChild).removeRecursively());
}

// 验证迁移失败时视图模型保留原路径并展示具体冲突原因。
/**
 * @brief 验证 testReportsMigrationFailureWithoutChangingConfig 测试场景。
 */
void TestExportSettingsViewModel::testReportsMigrationFailureWithoutChangingConfig() {
    // cache：测试使用的缓存或元器件对象。
    ComponentCacheService* cache = ComponentCacheService::instance();
    // tempPath：测试使用的文件系统路径。
    const QString tempPath = canonicalTempPath(m_tempDir);
    QVERIFY(cache->setCacheDir(tempPath, false));
    ConfigService::instance()->setCacheDir(tempPath);

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

    QCOMPARE(viewModel.cacheDir(), tempPath);
    QCOMPARE(cache->cacheDir(), tempPath);
    QCOMPARE(ConfigService::instance()->getCacheDir(), tempPath);
    QVERIFY(viewModel.status().contains(QStringLiteral("同名文件")));
    QCOMPARE(rejectionSpy.count(), 1);
    QCOMPARE(rejectionSpy.at(0).at(0).toString(), targetPath);
    QVERIFY(rejectionSpy.at(0).at(1).toString().contains(QStringLiteral("同名文件")));
    QVERIFY(QFileInfo::exists(m_tempDir.filePath(QStringLiteral("C90001/component.json"))));
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
    const QString sourcePath = canonicalTempPath(m_tempDir);
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
