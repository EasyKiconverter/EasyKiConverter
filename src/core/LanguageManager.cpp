#include "LanguageManager.h"

#include "services/ConfigService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QTranslator>

namespace EasyKiConverter {

LanguageManager* LanguageManager::s_instance = nullptr;

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
    , m_currentLanguage("en")
    , m_translator(nullptr)
    , m_isInitializing(true)
    , m_loadedFromConfig(false) {
    loadLanguageSettings();

    // 延迟重置初始化标志，给 QML 组件时间完成加载
    QTimer::singleShot(500, this, [this]() {
        m_isInitializing = false;
        qInfo() << "[LanguageManager] Initialization protection period ended";
    });
}

LanguageManager::~LanguageManager() {}

/**
 * @brief 返回语言管理器单例。
 * @return 语言管理器实例。
 */
LanguageManager* LanguageManager::instance() {
    if (!s_instance) {
        s_instance = new LanguageManager();
    }
    return s_instance;
}

/**
 * @brief 设置当前界面语言并刷新翻译器。
 * @param languageCode 语言代码。
 * @param force 是否忽略初始化阶段的配置保护。
 */
void LanguageManager::setLanguage(const QString& languageCode, bool force) {
    qInfo() << "[LanguageManager] setLanguage() called with:" << languageCode << " (current:" << m_currentLanguage
            << ", initializing:" << m_isInitializing << ", loadedFromConfig:" << m_loadedFromConfig
            << ", force:" << force << ")";

    if (m_currentLanguage == languageCode) {
        qInfo() << "[LanguageManager] Language unchanged, returning";

        return;
    }

    // 如果正在初始化且已经从配置加载了语言，则拒绝覆盖（除非 force=true）
    // 这样可以防止 QML 组件在初始化时覆盖配置文件的语言设置
    if (m_isInitializing && m_loadedFromConfig && !force) {
        qWarning() << "[LanguageManager] Refusing to override language during initialization after loading from "
                      "config. Requested:"
                   << languageCode << ", Keeping:" << m_currentLanguage;

        return;
    }

    m_currentLanguage = languageCode;

    qInfo() << "[LanguageManager] Language changed to" << m_currentLanguage;

    // 只在非初始化期间保存设置

    if (!m_isInitializing) {
        saveLanguageSettings();

    } else {
        qInfo() << "[LanguageManager] Skipping save during initialization";
    }

    // 安装新的翻译器

    installTranslator(languageCode);

    emit languageChanged(languageCode);

    emit refreshRequired();

    qInfo() << "[LanguageManager] Language changed signal emitted:" << languageCode;
}

/**
 * @brief 返回当前界面语言代码。
 * @return 当前语言代码。
 */
QString LanguageManager::currentLanguage() const {
    return m_currentLanguage;
}

/**
 * @brief 返回系统语言检测结果。
 * @return 当前实现使用的默认语言代码。
 */
QString LanguageManager::detectSystemLanguage() const {
    // 不再使用系统语言检测，直接返回英文
    return "en";
}

/**
 * @brief 从配置服务加载语言设置并安装翻译器。
 */
void LanguageManager::loadLanguageSettings() {
    // 从 ConfigService 加载语言设置
    auto* configService = ConfigService::instance();
    if (configService) {
        m_currentLanguage = configService->getLanguage();
        qInfo() << "[LanguageManager] Loaded language settings from ConfigService:" << m_currentLanguage;
    } else {
        qWarning() << "[LanguageManager] ConfigService not available, using default language: en";
        m_currentLanguage = "en";
    }

    m_loadedFromConfig = true;

    // 安装翻译器
    qInfo() << "[LanguageManager] Installing translator for language:" << m_currentLanguage;
    installTranslator(m_currentLanguage);
}

/**
 * @brief 将当前语言设置保存到配置服务。
 */
void LanguageManager::saveLanguageSettings() {
    // 保存到 ConfigService
    auto* configService = ConfigService::instance();
    if (configService) {
        configService->setLanguage(m_currentLanguage);
        qInfo() << "[LanguageManager] Saved language settings to ConfigService:" << m_currentLanguage;
    } else {
        qWarning() << "[LanguageManager] ConfigService not available, failed to save language settings";
    }
}

/**
 * @brief 加载并安装指定语言的 Qt 翻译器。
 * @param languageCode 要安装的语言代码。
 */
void LanguageManager::installTranslator(const QString& languageCode) {
    QCoreApplication* app = QCoreApplication::instance();  // 当前 Qt 应用实例。
    if (!app) {
        qWarning() << "[LanguageManager] QCoreApplication instance not available";
        return;
    }

    // 移除旧翻译器
    if (m_translator) {
        app->removeTranslator(m_translator);
        m_translator->deleteLater();
        m_translator = nullptr;
    }

    // Qt 资源系统中的翻译文件路径
    // 由 qt_add_translations 自动生成并注册到资源系统
    // qt_add_translations 默认使用 /i18n 前缀
    QString resourcePath = QString(":/i18n/translations_easykiconverter_%1.qm").arg(languageCode);

    m_translator = new QTranslator(this);

    // 尝试从 Qt 资源系统加载
    if (m_translator->load(resourcePath)) {
        app->installTranslator(m_translator);
        qInfo() << "[LanguageManager] [OK] Loaded translator from resource:" << resourcePath;
        return;
    }

    // 如果资源文件不存在，尝试从文件系统加载（开发时可能有用）
    QString localPath = QString("resources/translations/translations_easykiconverter_%1.qm").arg(languageCode);
    if (m_translator->load(localPath)) {
        app->installTranslator(m_translator);
        qInfo() << "[LanguageManager] [OK] Loaded translator from filesystem:" << localPath;
        return;
    }

    qWarning() << "[LanguageManager] [FAIL] Cannot load translation:" << languageCode;
    qWarning() << "[LanguageManager] Tried paths:";
    qWarning() << "  1. Resource path:" << resourcePath;
    qWarning() << "  2. File system path:" << localPath;
    m_translator->deleteLater();
    m_translator = nullptr;
}

}  // namespace EasyKiConverter
