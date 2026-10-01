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

    /** 验证 GUI 显示已通过当前验证门槛的目标格式。 */
    void exposesOnlyVerifiedTargets() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

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

        const QString configPath = temporary.filePath(QStringLiteral("export_plugins.json"));
        QFile config(configPath);
        QVERIFY(config.open(QIODevice::WriteOnly));
        QVERIFY(config.write(QJsonDocument(QJsonObject{{QStringLiteral("plugins"), plugins}}).toJson()) > 0);
        config.close();

        ExportTargetModel model;
        model.loadPlugins(configPath);

        const QVariantList targets = model.availableTargets();
        QCOMPARE(targets.size(), 4);
        QCOMPARE(targets.at(0).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("kicad"));
        QCOMPARE(targets.at(1).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("altium"));
        QCOMPARE(targets.at(2).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("horizon"));
        QCOMPARE(targets.at(3).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("librepcb"));
        QCOMPARE(model.currentIndex(), 0);
        QCOMPARE(model.currentTargetId(), QStringLiteral("kicad"));
    }

    /** 验证可见下拉索引与稳定目标枚举值分离。 */
    void keepsVerifiedTargetEnumIndices() {
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
        const QString configPath = temporary.filePath(QStringLiteral("export_plugins.json"));
        QFile config(configPath);
        QVERIFY(config.open(QIODevice::WriteOnly));
        QVERIFY(config.write(QJsonDocument(QJsonObject{{QStringLiteral("plugins"), plugins}}).toJson()) > 0);
        config.close();

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
};

QTEST_GUILESS_MAIN(TestExportTargetModel)
#include "test_export_target_model.moc"
