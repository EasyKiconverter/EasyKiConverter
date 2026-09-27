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

    /** 验证 GUI 只显示已完成发布验证的 KiCad 和 Altium 目标。 */
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
        QCOMPARE(targets.size(), 2);
        QCOMPARE(targets.at(0).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("kicad"));
        QCOMPARE(targets.at(1).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("altium"));
        QCOMPARE(model.currentIndex(), 0);
        QCOMPARE(model.currentTargetId(), QStringLiteral("kicad"));
    }

    /** 验证下拉框索引仍与 KiCad/Altium 的目标枚举保持一致。 */
    void keepsVerifiedTargetEnumIndices() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        const QJsonArray plugins = {
            QJsonObject{{QStringLiteral("id"), QStringLiteral("altium")},
                        {QStringLiteral("displayName"), QStringLiteral("Altium Designer")}},
            QJsonObject{{QStringLiteral("id"), QStringLiteral("kicad")},
                        {QStringLiteral("displayName"), QStringLiteral("KiCad")}},
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

        QCOMPARE(model.availableTargets().size(), 2);
        QCOMPARE(model.currentIndex(), 0);
        QCOMPARE(model.currentTargetId(), QStringLiteral("kicad"));
        model.setCurrentIndex(1);
        QCOMPARE(model.currentTargetId(), QStringLiteral("altium"));
    }
};

QTEST_GUILESS_MAIN(TestExportTargetModel)
#include "test_export_target_model.moc"
