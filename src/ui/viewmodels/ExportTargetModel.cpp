#include "ExportTargetModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace EasyKiConverter {

/**
 * @brief 构造函数
 */
ExportTargetModel::ExportTargetModel(QObject* parent) : QObject(parent) {}

/**
 * @brief 获取当前选中的目标索引
 * @return 当前目标在可见目标列表中的索引
 */
int ExportTargetModel::currentIndex() const {
    return m_currentIndex;
}

int ExportTargetModel::currentTargetFormat() const {
    const QString id = currentTargetId();
    if (id == QStringLiteral("altium"))
        return 1;
    if (id == QStringLiteral("xpedition"))
        return 2;
    if (id == QStringLiteral("allegro"))
        return 3;
    if (id == QStringLiteral("pads"))
        return 4;
    if (id == QStringLiteral("eagle"))
        return 5;
    if (id == QStringLiteral("pcad"))
        return 6;
    if (id == QStringLiteral("cadstar"))
        return 7;
    if (id == QStringLiteral("orcad"))
        return 8;
    if (id == QStringLiteral("librepcb"))
        return 9;
    if (id == QStringLiteral("horizon"))
        return 10;
    return 0;
}

/**
 * @brief 设置当前选中的目标索引
 * @param index 目标在可见目标列表中的索引
 */
void ExportTargetModel::setCurrentIndex(int index) {
    if (index < 0 || index >= m_targets.size()) {
        return;
    }
    if (m_currentIndex == index) {
        return;
    }
    m_currentIndex = index;
    emit currentTargetChanged();
}

/**
 * @brief 获取当前目标标识符
 * @return 当前目标的稳定标识符，无有效选择时返回空字符串
 */
QString ExportTargetModel::currentTargetId() const {
    if (m_currentIndex >= 0 && m_currentIndex < m_targets.size()) {
        return m_targets[m_currentIndex].id;
    }
    return QString();
}

/**
 * @brief 获取当前目标显示名称
 * @return 当前目标的显示名称，无有效选择时返回空字符串
 */
QString ExportTargetModel::currentDisplayName() const {
    if (m_currentIndex >= 0 && m_currentIndex < m_targets.size()) {
        return m_targets[m_currentIndex].displayName;
    }
    return QString();
}

/**
 * @brief 获取当前目标图标资源名
 * @return 当前目标的图标资源名，无有效选择时返回空字符串
 */
QString ExportTargetModel::currentIcon() const {
    if (m_currentIndex >= 0 && m_currentIndex < m_targets.size()) {
        return m_targets[m_currentIndex].icon;
    }
    return QString();
}

/**
 * @brief 获取当前目标的 QML 设置组件
 * @return 当前目标对应的 QML 组件文件名，无有效选择时返回空字符串
 */
QString ExportTargetModel::currentOptionsComponent() const {
    if (m_currentIndex >= 0 && m_currentIndex < m_targets.size()) {
        return m_targets[m_currentIndex].optionsComponent;
    }
    return QString();
}

/**
 * @brief 获取 GUI 可用目标列表
 * @return 供 QML ComboBox 使用的目标元数据列表
 */
QVariantList ExportTargetModel::availableTargets() const {
    return m_availableTargetsCache;
}

/**
 * @brief 判断目标格式是否向 GUI 暴露
 * @param targetId 目标格式标识符
 * @return true 表示目标格式可以显示给 GUI 用户
 */
bool ExportTargetModel::isUserVisibleTarget(const QString& targetId) {
    // 仅将已完成 GUI 导出流程接入的格式放入选择器。
    return targetId == QStringLiteral("kicad") || targetId == QStringLiteral("altium") ||
           targetId == QStringLiteral("horizon") || targetId == QStringLiteral("librepcb");
}

/**
 * @brief 从 JSON 文件加载插件配置
 */
void ExportTargetModel::loadPlugins(const QString& configPath) {
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ExportTargetModel: Failed to open plugin config:" << configPath;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "ExportTargetModel: Invalid JSON in plugin config:" << configPath;
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray plugins = root["plugins"].toArray();

    m_targets.clear();
    for (const QJsonValue& val : plugins) {
        QJsonObject obj = val.toObject();
        TargetInfo info;
        info.id = obj["id"].toString();
        info.displayName = obj["displayName"].toString();
        info.icon = obj["icon"].toString();
        info.optionsComponent = obj["optionsComponent"].toString();
        if (isUserVisibleTarget(info.id) && !info.displayName.isEmpty()) {
            m_targets.append(info);
        }
    }

    // 下拉索引会直接映射到 TargetEdaFormat，必须与枚举顺序保持一致，不能依赖配置文件顺序。
    std::sort(m_targets.begin(), m_targets.end(), [](const TargetInfo& left, const TargetInfo& right) {
        const auto targetOrder = [](const QString& id) {
            if (id == QStringLiteral("kicad"))
                return 0;
            if (id == QStringLiteral("altium"))
                return 1;
            if (id == QStringLiteral("horizon"))
                return 2;
            if (id == QStringLiteral("librepcb"))
                return 3;
            return 4;
        };
        return targetOrder(left.id) < targetOrder(right.id);
    });

    // 重建缓存
    m_availableTargetsCache.clear();
    for (const TargetInfo& info : m_targets) {
        QVariantMap map;
        map["id"] = info.id;
        map["displayName"] = info.displayName;
        map["icon"] = info.icon;
        map["optionsComponent"] = info.optionsComponent;
        m_availableTargetsCache.append(map);
    }

    // 确保索引有效
    bool indexChanged = false;
    if (m_currentIndex >= m_targets.size()) {
        m_currentIndex = 0;
        indexChanged = true;
    }

    emit availableTargetsChanged();
    if (indexChanged) {
        emit currentTargetChanged();
    }
}

}  // namespace EasyKiConverter
