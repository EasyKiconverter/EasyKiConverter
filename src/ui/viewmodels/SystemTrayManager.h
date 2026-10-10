#ifndef SYSTEMTRAYMANAGER_H
#define SYSTEMTRAYMANAGER_H

#include <QMenu>
#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

namespace EasyKiConverter {

class SystemTrayManager : public QObject {
    Q_OBJECT
public:
    explicit SystemTrayManager(QObject* parent = nullptr);
    ~SystemTrayManager();

    void initialize();
    void showExportNotification(int successCount,
                                int failureCount,
                                int symbolCount,
                                int footprintCount,
                                int model3DCount,
                                const QString& duration = QString());  ///< 显示导出完成通知。

    bool isAvailable() const;
    bool isVisible() const;

signals:
    void showWindowRequested();
    void quitRequested();

private:
    void setupTrayIcon();
    void setupContextMenu();
    void setupConnections();

private:
    QSystemTrayIcon* m_trayIcon;  ///< 保存系统托盘图标对象。
    QMenu* m_contextMenu;  ///< 保存托盘菜单对象。
    QString m_appName;  ///< 保存应用显示名称。
    bool m_initialized;  ///< 标记托盘功能是否已初始化。
};

}  // namespace EasyKiConverter

#endif  // SYSTEMTRAYMANAGER_H
