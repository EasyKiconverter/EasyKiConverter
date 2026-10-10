#include "SystemTrayManager.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QWindow>

namespace EasyKiConverter {

/**
 * @brief 创建系统托盘管理器。
 * @param parent Qt 对象父级。
 */
SystemTrayManager::SystemTrayManager(QObject* parent)
    : QObject(parent)
    , m_trayIcon(nullptr)
    , m_contextMenu(nullptr)
    , m_appName(QStringLiteral("EasyKiConverter"))
    , m_initialized(false) {}

/**
 * @brief 销毁系统托盘管理器并释放托盘资源。
 */
SystemTrayManager::~SystemTrayManager() {
    if (m_trayIcon) {
        m_trayIcon->hide();
        if (m_contextMenu) {
            m_trayIcon->setContextMenu(nullptr);
            m_contextMenu->deleteLater();
            m_contextMenu = nullptr;
        }
        m_trayIcon->deleteLater();
        m_trayIcon = nullptr;
    }
}

/**
 * @brief 初始化系统托盘图标、菜单和信号连接。
 */
void SystemTrayManager::initialize() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qDebug() << "System tray is not available on this platform";
        return;
    }

    qDebug() << "Initializing system tray icon...";

    m_trayIcon = new QSystemTrayIcon(this);

    QIcon appIcon;  // 待设置到系统托盘的应用图标。
    QStringList iconPaths = {// 按优先级排列的应用图标资源路径。
                             ":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/app_icon.ico",
                             ":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/app_icon.png",
                             ":/resources/icons/app_icon.ico",
                             ":/resources/icons/app_icon.png",
                             "resources/icons/app_icon.ico",
                             "resources/icons/app_icon.png"};

    for (const QString& path : iconPaths) {
        appIcon = QIcon(path);
        if (!appIcon.isNull()) {
            qDebug() << "System tray icon loaded successfully from:" << path;
            break;
        }
    }

    if (appIcon.isNull()) {
        qWarning() << "Failed to load icon from all paths, using default icon";
        appIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        if (appIcon.isNull()) {
            qCritical() << "Failed to load even default icon";
            return;
        }
    }

    m_trayIcon->setIcon(appIcon);
    qDebug() << "System tray icon set successfully";

    m_trayIcon->setToolTip(tr("EasyKiConverter - LCSC 转换工具"));

    setupContextMenu();
    setupConnections();

    m_trayIcon->show();

    if (m_trayIcon->isVisible()) {
        qDebug() << "System tray icon is visible";
    } else {
        qWarning() << "System tray icon is not visible after show()";
    }

    m_initialized = true;
    qDebug() << "System tray icon initialized and shown";
}

/**
 * @brief 创建系统托盘右键菜单。
 */
void SystemTrayManager::setupContextMenu() {
    m_contextMenu = new QMenu();
    m_contextMenu->setWindowFlags(m_contextMenu->windowFlags() | Qt::Popup);

    QAction* showAction = m_contextMenu->addAction(tr("显示窗口"));  // 显示主窗口的菜单动作。
    connect(showAction, &QAction::triggered, this, [this]() {
        qDebug() << "Show window action triggered";
        QTimer::singleShot(0, m_contextMenu, &QMenu::close);
        emit showWindowRequested();
    });

    QAction* quitAction = m_contextMenu->addAction(tr("退出"));  // 请求退出应用的菜单动作。
    connect(quitAction, &QAction::triggered, this, [this]() {
        qDebug() << "Quit action triggered";
        QTimer::singleShot(0, m_contextMenu, &QMenu::close);
        emit quitRequested();
    });

    m_trayIcon->setContextMenu(m_contextMenu);
}

/**
 * @brief 连接系统托盘激活事件。
 */
void SystemTrayManager::setupConnections() {
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            qDebug() << "System tray icon triggered (left click/double click)";
            emit showWindowRequested();
        }
    });
}

/**
 * @brief 显示导出结果系统通知。
 * @param successCount 成功导出的元器件数量。
 * @param failureCount 导出失败的元器件数量。
 * @param symbolCount 已导出的符号数量。
 * @param footprintCount 已导出的封装数量。
 * @param model3DCount 已导出的三维模型数量。
 * @param duration 导出耗时文本。
 */
void SystemTrayManager::showExportNotification(int successCount,
                                               int failureCount,
                                               int symbolCount,
                                               int footprintCount,
                                               int model3DCount,
                                               const QString& duration) {
    if (!m_trayIcon) {
        qDebug() << "System tray icon not initialized";
        return;
    }

    if (!QSystemTrayIcon::supportsMessages()) {
        qDebug() << "System tray does not support messages on this platform";
        return;
    }

    QString title;  // 通知标题。
    QString message;  // 通知正文。
    QSystemTrayIcon::MessageIcon iconType = QSystemTrayIcon::Information;  // 通知图标类型。

    if (failureCount > 0) {
        title = tr("导出完成");

        double successRate = 0.0;  // 导出成功率百分比。
        if (successCount + failureCount > 0) {
            successRate = (static_cast<double>(successCount) * 100.0) / (successCount + failureCount);
        }

        if (successCount == 0) {
            iconType = QSystemTrayIcon::Critical;
            message = tr("导出失败：%1 个元器件全部失败").arg(successCount + failureCount);
        } else if (successRate < 50.0) {
            iconType = QSystemTrayIcon::Warning;
            message = tr("成功 %1 个，失败 %2 个").arg(successCount).arg(failureCount);
        } else {
            message = tr("成功 %1 个，失败 %2 个").arg(successCount).arg(failureCount);
        }

        if (successCount > 0) {
            message += tr("\n输出：符号 %1 · 封装 %2 · 3D %3").arg(symbolCount).arg(footprintCount).arg(model3DCount);
        }
    } else {
        iconType = QSystemTrayIcon::Information;
        title = tr("导出完成");

        if (successCount == 1) {
            message = tr("成功导出 1 个元器件");
        } else {
            message = tr("成功导出 %1 个元器件").arg(successCount);
        }

        QStringList stats;  // 按行组织的导出统计信息。
        stats << tr("输出：符号 %1 · 封装 %2 · 3D %3").arg(symbolCount).arg(footprintCount).arg(model3DCount);

        if (!duration.isEmpty()) {
            stats << tr("耗时：%1").arg(duration);
        }

        message += "\n" + stats.join("\n");
    }

    qDebug() << "Attempting to show notification:" << title << message;
    m_trayIcon->showMessage(title, message, iconType, 12000);
    qDebug() << "Notification shown successfully";
}

/**
 * @brief 判断当前平台是否提供系统托盘。
 * @return 支持系统托盘时返回 true。
 */
bool SystemTrayManager::isAvailable() const {
    return QSystemTrayIcon::isSystemTrayAvailable();
}

/**
 * @brief 判断托盘图标当前是否可见。
 * @return 托盘图标已创建且可见时返回 true。
 */
bool SystemTrayManager::isVisible() const {
    return m_trayIcon && m_trayIcon->isVisible();
}

}  // namespace EasyKiConverter
