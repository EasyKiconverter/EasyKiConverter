#ifndef THEMESETTINGSVIEWMODEL_H
#define THEMESETTINGSVIEWMODEL_H

#include "services/ConfigService.h"

#include <QObject>

namespace EasyKiConverter {

class ThemeSettingsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isDarkMode READ isDarkMode WRITE setDarkMode NOTIFY darkModeChanged)

public:
    explicit ThemeSettingsViewModel(QObject* parent = nullptr);
    ~ThemeSettingsViewModel() override;

    /**
     * @brief 返回当前是否为深色模式。
     * @return 当前启用深色模式时返回 true。
     */
    bool isDarkMode() const {
        return m_isDarkMode;
    }

    Q_INVOKABLE void setDarkMode(bool darkMode);

signals:
    void darkModeChanged();

private:
    ConfigService* m_configService;  ///< 保存主题设置的配置服务。
    bool m_isDarkMode;  ///< 当前是否启用深色模式。
};

}  // namespace EasyKiConverter

#endif  // THEMESETTINGSVIEWMODEL_H
