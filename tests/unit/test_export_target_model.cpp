#include "services/ConfigService.h"
#include "ui/viewmodels/ExportTargetModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestExportTargetModel final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    /**
     * @brief 验证 GUI 显示已通过当前验证门槛的目标格式。
     */
    void exposesOnlyVerifiedTargets() {
        // temporary：测试隔离环境中的临时对象。
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        // plugins：已加载的插件集合。
        QJsonArray plugins;
        const auto addPlugin = [&plugins](const QString& id, const QString& displayName) {
            plugins.append(
                QJsonObject{{QStringLiteral("id"), id},
                            {QStringLiteral("displayName"), displayName},
                            {QStringLiteral("icon"), QStringLiteral("kicad")},
                            {QStringLiteral("optionsComponent"), QStringLiteral("ExportSettingsBaseCard.qml")}});
        };
        addPlugin(QStringLiteral("kicad"), QStringLiteral("KiCad"));
        addPlugin(QStringLiteral("altium"), QStringLiteral("Altium Designer"));
        addPlugin(QStringLiteral("horizon"), QStringLiteral("Horizon EDA"));
        addPlugin(QStringLiteral("librepcb"), QStringLiteral("LibrePCB"));
        addPlugin(QStringLiteral("xpedition"), QStringLiteral("Xpedition"));
        addPlugin(QStringLiteral("allegro"), QStringLiteral("Allegro PCB"));
        addPlugin(QStringLiteral("eagle"), QStringLiteral("Eagle Library"));

        // configPath：测试使用的文件系统路径。
        const QString configPath = temporary.filePath(QStringLiteral("export_plugins.json"));
        QFile config(configPath);
        QVERIFY(config.open(QIODevice::WriteOnly));
        QVERIFY(config.write(QJsonDocument(QJsonObject{{QStringLiteral("plugins"), plugins}}).toJson()) > 0);
        config.close();

        // model：测试使用的配置值或状态。
        ExportTargetModel model;
        model.loadPlugins(configPath);

        // targets：测试目标格式集合。
        const QVariantList targets = model.availableTargets();
        QCOMPARE(targets.size(), 4);
        QCOMPARE(targets.at(0).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("kicad"));
        QCOMPARE(targets.at(1).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("altium"));
        QCOMPARE(targets.at(2).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("horizon"));
        QCOMPARE(targets.at(3).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("librepcb"));
        QCOMPARE(model.currentIndex(), 0);
        QCOMPARE(model.currentTargetId(), QStringLiteral("kicad"));
    }

    /**
     * @brief 验证可见下拉索引与稳定目标枚举值分离。
     */
    void keepsVerifiedTargetEnumIndices() {
        // temporary：测试隔离环境中的临时对象。
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        const QJsonArray plugins = {
            QJsonObject{{QStringLiteral("id"), QStringLiteral("altium")},
                        {QStringLiteral("displayName"), QStringLiteral("Altium Designer")}},
            QJsonObject{{QStringLiteral("id"), QStringLiteral("kicad")},
                        {QStringLiteral("displayName"), QStringLiteral("KiCad")}},
            QJsonObject{{QStringLiteral("id"), QStringLiteral("horizon")},
                        {QStringLiteral("displayName"), QStringLiteral("Horizon EDA")}},
            QJsonObject{{QStringLiteral("id"), QStringLiteral("librepcb")},
                        {QStringLiteral("displayName"), QStringLiteral("LibrePCB")}},
            QJsonObject{{QStringLiteral("id"), QStringLiteral("pads")},
                        {QStringLiteral("displayName"), QStringLiteral("PADS Library")}},
        };
        // configPath：测试使用的文件系统路径。
        const QString configPath = temporary.filePath(QStringLiteral("export_plugins.json"));
        QFile config(configPath);
        QVERIFY(config.open(QIODevice::WriteOnly));
        QVERIFY(config.write(QJsonDocument(QJsonObject{{QStringLiteral("plugins"), plugins}}).toJson()) > 0);
        config.close();

        // model：测试使用的配置值或状态。
        ExportTargetModel model;
        model.loadPlugins(configPath);

        QCOMPARE(model.availableTargets().size(), 4);
        QCOMPARE(model.currentIndex(), 0);
        QCOMPARE(model.currentTargetId(), QStringLiteral("kicad"));
        model.setCurrentIndex(1);
        QCOMPARE(model.currentTargetId(), QStringLiteral("altium"));
        model.setCurrentIndex(2);
        QCOMPARE(model.currentTargetId(), QStringLiteral("horizon"));
        model.setCurrentIndex(3);
        QCOMPARE(model.currentTargetId(), QStringLiteral("librepcb"));
        QCOMPARE(model.currentTargetFormat(), 9);
    }

    /**
     * @brief 验证目标模型按稳定 ID 恢复上次选择的目标格式。
     */
    void restoresPersistedTargetByStableId() {
        // configPath：测试使用的隔离配置文件路径。
        const QString configPath = m_configDirectory.filePath(QStringLiteral("target-model-config.json"));
        // config：测试使用的配置服务实例。
        ConfigService* config = ConfigService::instance();
        QVERIFY(config->saveConfig(configPath));
        QVERIFY(config->loadConfig(configPath));
        config->resetToDefaults();

        // plugins：测试使用的目标格式配置集合。
        const QJsonArray plugins = {
            QJsonObject{{QStringLiteral("id"), QStringLiteral("kicad")},
                        {QStringLiteral("displayName"), QStringLiteral("KiCad")}},
            QJsonObject{{QStringLiteral("id"), QStringLiteral("altium")},
                        {QStringLiteral("displayName"), QStringLiteral("Altium Designer")}},
        };
        // pluginsPath：测试使用的插件配置路径。
        const QString pluginsPath = m_configDirectory.filePath(QStringLiteral("export_plugins.json"));
        // pluginsFile：测试使用的插件配置文件。
        QFile pluginsFile(pluginsPath);
        QVERIFY(pluginsFile.open(QIODevice::WriteOnly));
        QVERIFY(pluginsFile.write(QJsonDocument(QJsonObject{{QStringLiteral("plugins"), plugins}}).toJson()) > 0);
        pluginsFile.close();

        // firstModel：首次运行中用于保存 Altium 选择的目标模型。
        ExportTargetModel firstModel;
        firstModel.loadPlugins(pluginsPath);
        firstModel.setCurrentIndex(1);
        QCOMPARE(config->getTargetFormatId(), QStringLiteral("altium"));

        // restoredModel：模拟下次启动时重新创建的目标模型。
        ExportTargetModel restoredModel;
        restoredModel.loadPlugins(pluginsPath);
        QCOMPARE(restoredModel.currentTargetId(), QStringLiteral("altium"));
        QCOMPARE(restoredModel.currentIndex(), 1);
    }

private:
    // m_configDirectory：测试期间持久化配置使用的隔离目录。
    QTemporaryDir m_configDirectory;
};

/**
 * @brief 初始化目标格式模型测试使用的隔离配置。
 */
void TestExportTargetModel::initTestCase() {
    QVERIFY(m_configDirectory.isValid());

    // configPath：测试期间 ConfigService 使用的隔离配置文件路径。
    const QString configPath = m_configDirectory.filePath(QStringLiteral("config.json"));
    // config：测试使用的配置服务实例。
    ConfigService* config = ConfigService::instance();
    QVERIFY(config->saveConfig(configPath));
    QVERIFY(config->loadConfig(configPath));
    config->resetToDefaults();
}

/**
 * @brief 恢复目标格式模型测试的默认配置状态。
 */
void TestExportTargetModel::cleanup() {
    // config：测试使用的配置服务实例。
    ConfigService* config = ConfigService::instance();
    config->resetToDefaults();
}

QTEST_GUILESS_MAIN(TestExportTargetModel)
#include "test_export_target_model.moc"
