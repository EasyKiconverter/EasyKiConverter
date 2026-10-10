#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#include <QString>
#include <QTranslator>

namespace EasyKiConverter {

/**
 * @brief 语言管理器
 *
 * 负责管理应用程序的多语言支持
 * - 默认使用英文语言
 * - 支持手动切换语言
 * - 持久化语言设置到软件根目录
 * - 动态加载翻译文件
 */
class LanguageManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY languageChanged)

public:
    /**
     * @brief 获取单例实例
     * @return LanguageManager 单例
     */
    static LanguageManager* instance();

    /**
     * @brief 设置语言
     * @param languageCode 语言代码 ("zh_CN", "en")
     * @param force 是否强制设置（忽略初始化保护，用于命令行参数覆盖）
     */
    Q_INVOKABLE void setLanguage(const QString& languageCode, bool force = false);

    /**
     * @brief 获取当前语言代码
     * @return 当前语言代码
     */
    Q_INVOKABLE QString currentLanguage() const;

    /**
     * @brief 检测系统语言（已禁用，直接返回英文）
     * @return 返回英文语言代码
     */
    Q_INVOKABLE QString detectSystemLanguage() const;

signals:
    /**
     * @brief 语言改变信号
     * @param language 新的语言代码
     */
    void languageChanged(const QString& language);

    /**
     * @brief 请求刷新界面信号
     *
     * 当语言切换时发出此信号，通知 QML 刷新界面
     */
    void refreshRequired();

private:
    explicit LanguageManager(QObject* parent = nullptr);
    ~LanguageManager() override;

    void loadLanguageSettings();
    void saveLanguageSettings();
    void installTranslator(const QString& languageCode);

    static LanguageManager* s_instance;  ///< 语言管理器的全局实例指针。
    QString m_currentLanguage;  ///< 当前生效的语言代码。
    QTranslator* m_translator;  ///< 当前安装的 Qt 翻译器。
    bool m_isInitializing;  ///< 是否处于初始化保护阶段。
    bool m_loadedFromConfig;  ///< 是否已从配置加载语言设置。
};

}  // namespace EasyKiConverter

#endif  // LANGUAGEMANAGER_H
