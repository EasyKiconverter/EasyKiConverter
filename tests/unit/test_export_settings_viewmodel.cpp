#include "services/ConfigService.h"
#include "ui/viewmodels/ExportSettingsViewModel.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestExportSettingsViewModel : public QObject {
    Q_OBJECT

private slots:
    void init();
    void testLoadsCacheSettingsFromConfig();
    void testRejectsUnsafeCacheDirWithoutChangingConfig();
    void testDiskCacheLimitClamped();
    void testModel3DPathModeDefaultsAndPersists();
    void testNormalizePathMode();
    void testConfigServiceModel3DPathMode();

private:
    QTemporaryDir m_tempDir;
};

// 为每个测试建立隔离配置和临时缓存路径。
void TestExportSettingsViewModel::init() {
    QVERIFY(m_tempDir.isValid());

    ConfigService* config = ConfigService::instance();
    config->resetToDefaults();
    config->setCacheDir(QDir(m_tempDir.path()).filePath(QStringLiteral("configured-cache")));
    config->setDiskCacheLimitMB(4096);
}

// 验证视图模型从配置服务加载缓存设置。
void TestExportSettingsViewModel::testLoadsCacheSettingsFromConfig() {
    ExportSettingsViewModel viewModel(nullptr);

    QCOMPARE(viewModel.cacheDir(),
             QDir::cleanPath(QDir(m_tempDir.path()).filePath(QStringLiteral("configured-cache"))));
    QCOMPARE(viewModel.diskCacheLimitMB(), 4096);
}

// 拒绝非空的未托管目录，并保留已经生效的缓存配置。
void TestExportSettingsViewModel::testRejectsUnsafeCacheDirWithoutChangingConfig() {
    ExportSettingsViewModel viewModel(nullptr);
    const QString originalPath = viewModel.cacheDir();
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
}

// 验证磁盘缓存上限会被限制在配置服务允许的范围内。
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
void TestExportSettingsViewModel::testNormalizePathMode() {
    QCOMPARE(ExportOptions::normalizePathMode(0), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(ExportOptions::normalizePathMode(1), ExportOptions::MODEL_3D_PATH_ABSOLUTE);
    QCOMPARE(ExportOptions::normalizePathMode(999), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(ExportOptions::normalizePathMode(-1), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(ExportOptions::normalizePathMode(2), ExportOptions::MODEL_3D_PATH_RELATIVE);
}

// 验证配置服务对三维模型路径模式的读写边界保持稳定。
void TestExportSettingsViewModel::testConfigServiceModel3DPathMode() {
    ConfigService* config = ConfigService::instance();

    QCOMPARE(config->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);

    config->setExportModel3DPathMode(ExportOptions::MODEL_3D_PATH_ABSOLUTE);
    QCOMPARE(config->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_ABSOLUTE);

    config->setExportModel3DPathMode(ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(config->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);

    config->setExportModel3DPathMode(42);
    QCOMPARE(config->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);
}

QTEST_GUILESS_MAIN(TestExportSettingsViewModel)
#include "test_export_settings_viewmodel.moc"
