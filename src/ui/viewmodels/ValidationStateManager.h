#ifndef VALIDATIONSTATEMANAGER_H
#define VALIDATIONSTATEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class ValidationStateManager : public QObject {
    Q_OBJECT
public:
    explicit ValidationStateManager(QObject* parent = nullptr);
    ~ValidationStateManager() override;

    void startValidation(int count);
    void addValidation(int count);
    void cancelValidation(int count);
    void onComponentValidated(const QString& componentId);
    void onComponentFailed(const QString& componentId);
    void reset();

    bool isAllDone() const;
    bool isPreviewFetchEnabled() const;

    int pendingCount() const;
    int validatedCount() const;
    QStringList validatedComponentIds() const;

signals:
    void validationCompleted(const QStringList& validatedIds);
    void validationStateChanged(int pendingCount);

private:
    void checkAndNotifyCompletion();

private:
    int m_pendingValidationCount;  ///< 保存尚未完成验证的数量。
    int m_totalValidationCount;  ///< 保存本轮需要验证的总数。
    QStringList m_validatedComponentIds;  ///< 保存已验证成功的元器件编号。
    QStringList m_failedComponentIds;  ///< 保存验证失败的元器件编号。
    bool m_previewFetchEnabled{false};  ///< 标记验证完成后是否允许获取预览图。
};

}  // namespace EasyKiConverter

#endif  // VALIDATIONSTATEMANAGER_H
