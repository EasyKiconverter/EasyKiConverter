#include "ConfigManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

namespace EasyKiConverter {

ConfigManager::ConfigManager(QObject* parent) : QObject(parent), m_settings(nullptr) {
    // 创建 QSettings 对象
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configPath);

    QString configFile = configPath + "/config.ini";  // 配置文件的完整路径。
    m_settings = new QSettings(configFile, QSettings::IniFormat, this);

    // 初始化默认配
    initDefaults();

    // 加载配置
    load();
}

ConfigManager::~ConfigManager() {
    if (m_settings) {
        delete m_settings;
    }
}

/**
 * @brief 将当前配置写入持久化存储。
 * @return 写入成功返回 true，否则返回 false。
 */
bool ConfigManager::save() {
    try {
        // 保存导出路径
        QJsonValue exportPathValue = m_config.value("export_path");
        m_settings->setValue("export/export_path", exportPathValue.isUndefined() ? "" : exportPathValue.toString());

        QJsonValue libNameValue = m_config.value("lib_name");  // 当前库名称配置值。
        m_settings->setValue("export/lib_name",
                             libNameValue.isUndefined() ? "easyeda_convertlib" : libNameValue.toString());

        // 保存导出选项
        QJsonValue exportOptionsValue = m_config.value("export_options");
        QJsonObject exportOptions =
            exportOptionsValue.isUndefined() ? QJsonObject() : exportOptionsValue.toObject();  // 导出选项对象。

        QJsonValue symbolValue = exportOptions.value("symbol");  // 符号库导出开关。
        m_settings->setValue("export/export_symbol", symbolValue.isUndefined() ? true : symbolValue.toBool());

        QJsonValue footprintValue = exportOptions.value("footprint");  // 封装库导出开关。
        m_settings->setValue("export/export_footprint", footprintValue.isUndefined() ? true : footprintValue.toBool());

        QJsonValue model3dValue = exportOptions.value("model3d");  // 三维模型导出开关。
        m_settings->setValue("export/export_3d_model", model3dValue.isUndefined() ? true : model3dValue.toBool());

        // 同步到磁
        m_settings->sync();

        qDebug() << "Configuration saved successfully";
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Failed to save configuration:" << e.what();
        return false;
    }
}

/**
 * @brief 从持久化存储加载配置。
 * @return 加载成功返回 true，否则返回 false。
 */
bool ConfigManager::load() {
    try {
        // 加载导出路径
        m_config["export_path"] = m_settings->value("export/export_path", "").toString();
        m_config["lib_name"] = m_settings->value("export/lib_name", "easyeda_convertlib").toString();

        // 加载导出选项
        QJsonObject exportOptions;
        exportOptions["symbol"] = m_settings->value("export/export_symbol", true).toBool();
        exportOptions["footprint"] = m_settings->value("export/export_footprint", true).toBool();
        exportOptions["model3d"] = m_settings->value("export/export_3d_model", true).toBool();
        m_config["export_options"] = exportOptions;

        qDebug() << "Configuration loaded successfully";
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Failed to load configuration:" << e.what();
        return false;
    }
}

/**
 * @brief 返回完整配置对象。
 * @return 当前配置对象。
 */
QJsonObject ConfigManager::getConfig() const {
    return m_config;
}

/**
 * @brief 替换完整配置对象。
 * @param config 新的配置对象。
 */
void ConfigManager::setConfig(const QJsonObject& config) {
    m_config = config;
    emit configChanged();
}

/**
 * @brief 返回导出路径。
 * @return 当前导出路径。
 */
QString ConfigManager::getExportPath() const {
    QJsonValue value = m_config.value("export_path");  // 导出路径配置值。
    return value.isUndefined() ? "" : value.toString();
}

/**
 * @brief 设置导出路径。
 * @param path 新的导出路径。
 */
void ConfigManager::setExportPath(const QString& path) {
    m_config["export_path"] = path;
    emit configChanged();
}

/**
 * @brief 返回库名称。
 * @return 当前库名称。
 */
QString ConfigManager::getLibName() const {
    QJsonValue value = m_config.value("lib_name");  // 库名称配置值。
    return value.isUndefined() ? "easyeda_convertlib" : value.toString();
}

/**
 * @brief 设置库名称。
 * @param name 新的库名称。
 */
void ConfigManager::setLibName(const QString& name) {
    m_config["lib_name"] = name;
    emit configChanged();
}

/**
 * @brief 返回导出选项。
 * @return 当前导出选项对象。
 */
QJsonObject ConfigManager::getExportOptions() const {
    QJsonValue value = m_config.value("export_options");  // 导出选项配置值。
    return value.isUndefined() ? QJsonObject() : value.toObject();
}

/**
 * @brief 设置导出选项。
 * @param options 新的导出选项对象。
 */
void ConfigManager::setExportOptions(const QJsonObject& options) {
    m_config["export_options"] = options;
    emit configChanged();
}

/**
 * @brief 将配置恢复为默认值。
 */
void ConfigManager::resetToDefaults() {
    initDefaults();
    emit configChanged();
}

/**
 * @brief 初始化默认配置。
 */
void ConfigManager::initDefaults() {
    // 设置默认导出路径
    m_config["export_path"] = "";

    // 设置默认库名称
    m_config["lib_name"] = "easyeda_convertlib";

    // 设置默认导出选项
    QJsonObject exportOptions;
    exportOptions["symbol"] = true;
    exportOptions["footprint"] = true;
    exportOptions["model3d"] = true;
    m_config["export_options"] = exportOptions;

    qDebug() << "Default configuration initialized";
}

}  // namespace EasyKiConverter
