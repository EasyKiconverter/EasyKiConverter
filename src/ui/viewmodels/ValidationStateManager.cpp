#include "ValidationStateManager.h"

#include <QDebug>

namespace EasyKiConverter {

/**
 * @brief 创建验证状态管理器。
 * @param parent Qt 对象父级。
 */
ValidationStateManager::ValidationStateManager(QObject* parent)
    : QObject(parent), m_pendingValidationCount(0), m_totalValidationCount(0), m_previewFetchEnabled(false) {}

/**
 * @brief 销毁验证状态管理器。
 */
ValidationStateManager::~ValidationStateManager() {}

/**
 * @brief 开始一轮元器件验证。
 * @param count 待验证的元器件数量。
 */
void ValidationStateManager::startValidation(int count) {
    m_pendingValidationCount = count;
    m_totalValidationCount = count;
    m_validatedComponentIds.clear();
    m_failedComponentIds.clear();
    m_previewFetchEnabled = false;
    emit validationStateChanged(m_pendingValidationCount);
    qDebug() << "Validation started with" << count << "components pending, total:" << m_totalValidationCount;
}

/**
 * @brief 向当前验证批次追加元器件。
 * @param count 追加的元器件数量。
 */
void ValidationStateManager::addValidation(int count) {
    if (count <= 0) {
        return;
    }
    m_pendingValidationCount += count;
    m_totalValidationCount += count;
    emit validationStateChanged(m_pendingValidationCount);
    qDebug() << "Added" << count << "components to validation, pending count:" << m_pendingValidationCount
             << ", total:" << m_totalValidationCount;
}

/**
 * @brief 取消当前批次中的部分验证任务。
 * @param count 取消的元器件数量。
 */
void ValidationStateManager::cancelValidation(int count) {
    if (count <= 0) {
        return;
    }
    m_pendingValidationCount = qMax(0, m_pendingValidationCount - count);
    m_totalValidationCount = qMax(0, m_totalValidationCount - count);
    emit validationStateChanged(m_pendingValidationCount);
    qDebug() << "Cancelled" << count << "components from validation, pending count:" << m_pendingValidationCount
             << ", total:" << m_totalValidationCount;
    checkAndNotifyCompletion();
}

/**
 * @brief 记录元器件验证成功。
 * @param componentId 已验证元器件编号。
 */
void ValidationStateManager::onComponentValidated(const QString& componentId) {
    if (m_pendingValidationCount > 0) {
        m_pendingValidationCount--;
    }
    if (!m_validatedComponentIds.contains(componentId)) {
        m_validatedComponentIds.append(componentId);
    }
    emit validationStateChanged(m_pendingValidationCount);
    checkAndNotifyCompletion();
}

/**
 * @brief 记录元器件验证失败。
 * @param componentId 验证失败的元器件编号。
 */
void ValidationStateManager::onComponentFailed(const QString& componentId) {
    if (m_pendingValidationCount > 0) {
        m_pendingValidationCount--;
    }
    if (!m_failedComponentIds.contains(componentId)) {
        m_failedComponentIds.append(componentId);
    }
    emit validationStateChanged(m_pendingValidationCount);
    checkAndNotifyCompletion();
}

/**
 * @brief 清除当前验证批次的所有状态。
 */
void ValidationStateManager::reset() {
    m_pendingValidationCount = 0;
    m_totalValidationCount = 0;
    m_validatedComponentIds.clear();
    m_failedComponentIds.clear();
    m_previewFetchEnabled = false;
    emit validationStateChanged(m_pendingValidationCount);
}

/**
 * @brief 判断当前验证批次是否已没有待处理任务。
 * @return 没有待处理任务时返回 true。
 */
bool ValidationStateManager::isAllDone() const {
    return m_pendingValidationCount <= 0;
}

/**
 * @brief 返回待验证元器件数量。
 * @return 当前待验证数量。
 */
int ValidationStateManager::pendingCount() const {
    return m_pendingValidationCount;
}

/**
 * @brief 返回是否允许获取预览数据。
 * @return 允许获取预览数据时返回 true。
 */
bool ValidationStateManager::isPreviewFetchEnabled() const {
    return m_previewFetchEnabled;
}

/**
 * @brief 返回验证成功的元器件数量。
 * @return 当前验证成功数量。
 */
int ValidationStateManager::validatedCount() const {
    return m_validatedComponentIds.size();
}

/**
 * @brief 返回验证成功的元器件编号列表。
 * @return 验证成功的元器件编号列表。
 */
QStringList ValidationStateManager::validatedComponentIds() const {
    return m_validatedComponentIds;
}

/**
 * @brief 检查验证批次是否完成并发出完成信号。
 */
void ValidationStateManager::checkAndNotifyCompletion() {
    const int completedCount =  // 已结束的成功和失败验证任务总数。
        m_validatedComponentIds.size() + m_failedComponentIds.size();
    if (m_totalValidationCount > 0 && m_pendingValidationCount <= 0 && completedCount >= m_totalValidationCount) {
        qDebug() << "All validations completed," << m_validatedComponentIds.size() << "components validated,"
                 << m_failedComponentIds.size() << "components failed (total was" << m_totalValidationCount
                 << "), triggering preview fetch for valid components";
        emit validationCompleted(m_validatedComponentIds);
    }
}

}  // namespace EasyKiConverter
