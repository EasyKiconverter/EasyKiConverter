#include "ThemeSettingsViewModel.h"

#include <QDebug>

namespace EasyKiConverter {

/**
 * @brief 创建主题设置视图模型。
 * @param parent Qt 对象父级。
 */
ThemeSettingsViewModel::ThemeSettingsViewModel(QObject* parent)
    : QObject(parent), m_configService(ConfigService::instance()), m_isDarkMode(false) {
    m_isDarkMode = m_configService->getDarkMode();
}

/**
 * @brief 销毁主题设置视图模型。
 */
ThemeSettingsViewModel::~ThemeSettingsViewModel() {}

/**
 * @brief 设置深色模式并持久化配置。
 * @param darkMode 是否启用深色模式。
 */
void ThemeSettingsViewModel::setDarkMode(bool darkMode) {
    if (m_isDarkMode != darkMode) {
        m_isDarkMode = darkMode;
        m_configService->setDarkMode(darkMode);
        emit darkModeChanged();
        qDebug() << "Dark mode changed to:" << darkMode;
    }
}

}  // namespace EasyKiConverter
