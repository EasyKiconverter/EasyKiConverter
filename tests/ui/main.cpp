
#include "utils/FileUtils.h"

#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QString>
#include <QtQuickTest>

class FakeLanguageManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setLanguage NOTIFY languageChanged)

public:
    /**
     * @brief 验证 FakeLanguageManager 测试场景。
     * @param parent Qt 父对象。
     */
    explicit FakeLanguageManager(QObject* parent = nullptr) : QObject(parent) {}

    /**
     * @brief 验证 currentLanguage 测试场景。
     */
    QString currentLanguage() const {
        return m_currentLanguage;
    }

    /**
     * @brief 验证 setLanguage 测试场景。
     * @param language 待设置的语言代码。
     */
    Q_INVOKABLE void setLanguage(const QString& language) {
        if (m_currentLanguage == language) {
            return;
        }

        m_currentLanguage = language;
        emit languageChanged(m_currentLanguage);
    }

signals:
    void languageChanged(const QString& language);

private:
    QString m_currentLanguage{QStringLiteral("zh_CN")};
};

class Setup : public QObject {
    Q_OBJECT

public:
    Setup() {
        QQuickStyle::setStyle("Basic");
    }

public slots:

    /**
     * @brief 验证 qmlEngineAvailable 测试场景。
     * @param engine 待执行的 QML 引擎。
     */
    void qmlEngineAvailable(QQmlEngine* engine) {
        engine->addImportPath("qrc:/qt/qml");
        engine->rootContext()->setContextProperty("LanguageManager", new FakeLanguageManager(engine));
        engine->rootContext()->setContextProperty("fileUtils", new EasyKiConverter::FileUtils(engine));

        // 注册 AppStyle 单例类型 (必须手动注册以匹配 ModernButton.qml 中的导入 URI)
        qmlRegisterSingletonType(QUrl("qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/styles/AppStyle.qml"),
                                 "EasyKiconverter_Cpp_Version.src.ui.qml.styles",
                                 1,
                                 0,
                                 "AppStyle");
    }
};

QUICK_TEST_MAIN_WITH_SETUP(ui_tests, Setup)

#include "main.moc"
