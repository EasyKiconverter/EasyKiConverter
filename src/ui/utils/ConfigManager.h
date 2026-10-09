#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QJsonObject>
#include <QObject>
#include <QSettings>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 配置管理
     *
 * 管理应用程序的配置信息，包括导出路径、库名称、导出选项
     */
class ConfigManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
         *
     * @param parent 父对象
         */
    explicit ConfigManager(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ConfigManager() override;

    /**
     * @brief 保存配置
     *
     * @return bool 保存是否成功
     */
    bool save();

    /**
     * @brief 加载配置
     *
     * @return bool 加载是否成功
     */
    bool load();

    /**
     * @brief 获取配置对象
     *
     * @return QJsonObject 配置对象
     */
    QJsonObject getConfig() const;

    /**
     * @brief 设置配置对象
     *
     * @param config 配置对象
     */
    void setConfig(const QJsonObject& config);

    /**
     * @brief 获取导出路径
     *
     * @return QString 导出路径
     */
    QString getExportPath() const;

    /**
     * @brief 设置导出路径
     *
     * @param path 导出路径
     */
    void setExportPath(const QString& path);

    /**
     * @brief 获取库名称
         *
     * @return QString 库名称
         */
    QString getLibName() const;

    /**
     * @brief 设置库名称
         *
     * @param name 库名称
         */
    void setLibName(const QString& name);

    /**
     * @brief 获取导出选项
     *
     * @return QJsonObject 导出选项
     */
    QJsonObject getExportOptions() const;

    /**
     * @brief 设置导出选项
     *
     * @param options 导出选项
     */
    void setExportOptions(const QJsonObject& options);

    /**
     * @brief 重置配置为默认
     */
    void resetToDefaults();

signals:
    /**
     * @brief 配置已更改信
     */
    void configChanged();

private:
    /**
     * @brief 初始化默认配
     */
    void initDefaults();

private:
    QSettings* m_settings;  ///< 保存配置文件读写对象。
    QJsonObject m_config;  ///< 保存当前配置项。
};

}  // namespace EasyKiConverter

#endif  // CONFIGMANAGER_H
