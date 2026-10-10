#include "ui/viewmodels/ValidationStateManager.h"

#include <QSignalSpy>
#include <QTest>

using namespace EasyKiConverter;

class TestValidationStateManager : public QObject {
    Q_OBJECT

private slots:

    // === 初始状态 ===

    /**
     * @brief 验证 initialState 测试场景。
     */
    void initialState() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        QCOMPARE(manager.pendingCount(), 0);
        QCOMPARE(manager.validatedCount(), 0);
        QVERIFY(manager.isAllDone());
        QVERIFY(!manager.isPreviewFetchEnabled());
        QVERIFY(manager.validatedComponentIds().isEmpty());
    }

    // === startValidation ===

    /**
     * @brief 验证 startValidation_SetsPendingCount 测试场景。
     */
    void startValidation_SetsPendingCount() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(5);
        QCOMPARE(manager.pendingCount(), 5);
        QVERIFY(!manager.isAllDone());
    }

    /**
     * @brief 验证 startValidation_ClearsPreviousState 测试场景。
     */
    void startValidation_ClearsPreviousState() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentValidated(QStringLiteral("C1"));

        // 重新开始应清除之前的状态
        manager.startValidation(2);
        QCOMPARE(manager.pendingCount(), 2);
        QCOMPARE(manager.validatedCount(), 0);
        QVERIFY(manager.validatedComponentIds().isEmpty());
    }

    /**
     * @brief 验证 startValidation_DisablesPreview 测试场景。
     */
    void startValidation_DisablesPreview() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(1);
        QVERIFY(!manager.isPreviewFetchEnabled());
    }

    /**
     * @brief 验证 startValidation_EmitsSignal 测试场景。
     */
    void startValidation_EmitsSignal() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.startValidation(3);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
    }

    // === addValidation ===

    /**
     * @brief 验证 addValidation_IncrementsCounts 测试场景。
     */
    void addValidation_IncrementsCounts() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.addValidation(2);
        QCOMPARE(manager.pendingCount(), 5);
    }

    /**
     * @brief 验证 addValidation_EmitsSignal 测试场景。
     */
    void addValidation_EmitsSignal() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(1);
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.addValidation(3);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 4);
    }

    /**
     * @brief 验证 addValidation_IgnoresNonPositive 测试场景。
     */
    void addValidation_IgnoresNonPositive() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.addValidation(0);
        manager.addValidation(-1);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(manager.pendingCount(), 3);
    }

    // === cancelValidation ===

    /**
     * @brief 验证 cancelValidation_DecrementsCounts 测试场景。
     */
    void cancelValidation_DecrementsCounts() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(5);
        manager.cancelValidation(2);
        QCOMPARE(manager.pendingCount(), 3);
    }

    /**
     * @brief 验证 cancelValidation_FloorsAtZero 测试场景。
     */
    void cancelValidation_FloorsAtZero() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(2);
        manager.cancelValidation(5);
        QCOMPARE(manager.pendingCount(), 0);
    }

    /**
     * @brief 验证 cancelValidation_IgnoresNonPositive 测试场景。
     */
    void cancelValidation_IgnoresNonPositive() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.cancelValidation(0);
        QCOMPARE(spy.count(), 0);
    }

    /**
     * @brief 验证 cancelValidation_EmitsSignal 测试场景。
     */
    void cancelValidation_EmitsSignal() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(5);
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.cancelValidation(2);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
    }

    /**
     * @brief 验证 cancelValidation_DoesNotTriggerCompletion 测试场景。
     */
    void cancelValidation_DoesNotTriggerCompletion() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(2);

        QSignalSpy completionSpy(&manager, &ValidationStateManager::validationCompleted);
        manager.cancelValidation(2);

        // cancelValidation 将 total 也减为 0，条件 total > 0 不满足，不触发完成信号
        QCOMPARE(completionSpy.count(), 0);
        QVERIFY(manager.isAllDone());
    }

    // === onComponentValidated ===

    /**
     * @brief 验证 onComponentValidated_DecrementsPending 测试场景。
     */
    void onComponentValidated_DecrementsPending() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentValidated(QStringLiteral("C1"));
        QCOMPARE(manager.pendingCount(), 2);
        QCOMPARE(manager.validatedCount(), 1);
    }

    /**
     * @brief 验证 onComponentValidated_DeduplicatesId 测试场景。
     */
    void onComponentValidated_DeduplicatesId() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentValidated(QStringLiteral("C1"));
        manager.onComponentValidated(QStringLiteral("C1"));
        QCOMPARE(manager.validatedCount(), 1);
        QCOMPARE(manager.pendingCount(), 1);
    }

    /**
     * @brief 验证 onComponentValidated_EmitsStateChanged 测试场景。
     */
    void onComponentValidated_EmitsStateChanged() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(2);
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.onComponentValidated(QStringLiteral("C1"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
    }

    /**
     * @brief 验证 onComponentValidated_ReturnsCorrectIds 测试场景。
     */
    void onComponentValidated_ReturnsCorrectIds() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentValidated(QStringLiteral("C1"));
        manager.onComponentValidated(QStringLiteral("C2"));
        // ids：测试使用的元器件标识或名称。
        QStringList ids = manager.validatedComponentIds();
        QCOMPARE(ids.size(), 2);
        QVERIFY(ids.contains(QStringLiteral("C1")));
        QVERIFY(ids.contains(QStringLiteral("C2")));
    }

    // === onComponentFailed ===

    /**
     * @brief 验证 onComponentFailed_DecrementsPending 测试场景。
     */
    void onComponentFailed_DecrementsPending() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentFailed(QStringLiteral("C1"));
        QCOMPARE(manager.pendingCount(), 2);
        QCOMPARE(manager.validatedCount(), 0);  // 失败的不计入 validated
    }

    /**
     * @brief 验证 onComponentFailed_DeduplicatesId 测试场景。
     */
    void onComponentFailed_DeduplicatesId() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentFailed(QStringLiteral("C1"));
        manager.onComponentFailed(QStringLiteral("C1"));
        QCOMPARE(manager.pendingCount(), 1);
    }

    // === 完成流程 ===

    /**
     * @brief 验证 completion_AllValidated 测试场景。
     */
    void completion_AllValidated() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);

        QSignalSpy completionSpy(&manager, &ValidationStateManager::validationCompleted);
        manager.onComponentValidated(QStringLiteral("C1"));
        manager.onComponentValidated(QStringLiteral("C2"));
        QCOMPARE(completionSpy.count(), 0);  // 还没完成

        manager.onComponentValidated(QStringLiteral("C3"));
        QCOMPARE(completionSpy.count(), 1);  // 全部完成

        // completedIds：测试使用的元器件标识或名称。
        QStringList completedIds = completionSpy.at(0).at(0).toStringList();
        QCOMPARE(completedIds.size(), 3);
    }

    /**
     * @brief 验证 completion_MixedValidatedAndFailed 测试场景。
     */
    void completion_MixedValidatedAndFailed() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);

        QSignalSpy completionSpy(&manager, &ValidationStateManager::validationCompleted);
        manager.onComponentValidated(QStringLiteral("C1"));
        manager.onComponentFailed(QStringLiteral("C2"));
        QCOMPARE(completionSpy.count(), 0);  // 还没完成

        manager.onComponentValidated(QStringLiteral("C3"));
        QCOMPARE(completionSpy.count(), 1);  // 全部完成（2 成功 + 1 失败 = 3）

        // completedIds：测试使用的元器件标识或名称。
        QStringList completedIds = completionSpy.at(0).at(0).toStringList();
        QCOMPARE(completedIds.size(), 2);  // 只包含成功的
        QVERIFY(completedIds.contains(QStringLiteral("C1")));
        QVERIFY(completedIds.contains(QStringLiteral("C3")));
    }

    /**
     * @brief 验证 completion_AllFailed 测试场景。
     */
    void completion_AllFailed() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(2);

        QSignalSpy completionSpy(&manager, &ValidationStateManager::validationCompleted);
        manager.onComponentFailed(QStringLiteral("C1"));
        manager.onComponentFailed(QStringLiteral("C2"));
        QCOMPARE(completionSpy.count(), 1);

        // completedIds：测试使用的元器件标识或名称。
        QStringList completedIds = completionSpy.at(0).at(0).toStringList();
        QVERIFY(completedIds.isEmpty());  // 全部失败，没有成功的 ID
    }

    /**
     * @brief 验证 completion_WithAddValidation 测试场景。
     */
    void completion_WithAddValidation() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(1);

        QSignalSpy completionSpy(&manager, &ValidationStateManager::validationCompleted);

        // startValidation(1) + onComponentValidated → 第一次完成
        manager.onComponentValidated(QStringLiteral("C1"));
        QCOMPARE(completionSpy.count(), 1);

        // addValidation 增加 pending 和 total
        manager.addValidation(1);
        QCOMPARE(manager.pendingCount(), 1);

        // 第二次验证 → 第二次完成
        manager.onComponentValidated(QStringLiteral("C2"));
        QCOMPARE(completionSpy.count(), 2);
    }

    // === reset ===

    /**
     * @brief 验证 reset_ClearsAllState 测试场景。
     */
    void reset_ClearsAllState() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentValidated(QStringLiteral("C1"));
        manager.onComponentFailed(QStringLiteral("C2"));

        manager.reset();
        QCOMPARE(manager.pendingCount(), 0);
        QCOMPARE(manager.validatedCount(), 0);
        QVERIFY(manager.isAllDone());
        QVERIFY(manager.validatedComponentIds().isEmpty());
        QVERIFY(!manager.isPreviewFetchEnabled());
    }

    /**
     * @brief 验证 reset_EmitsSignal 测试场景。
     */
    void reset_EmitsSignal() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(3);
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.reset();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
    }

    // === isAllDone ===

    /**
     * @brief 验证 isAllDone_TrueWhenPendingZero 测试场景。
     */
    void isAllDone_TrueWhenPendingZero() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(1);
        QVERIFY(!manager.isAllDone());
        manager.onComponentValidated(QStringLiteral("C1"));
        QVERIFY(manager.isAllDone());
    }

    /**
     * @brief 验证 isAllDone_TrueWhenCancelledBelowZero 测试场景。
     */
    void isAllDone_TrueWhenCancelledBelowZero() {
        // manager：被测管理器实例。
        ValidationStateManager manager;
        manager.startValidation(2);
        manager.cancelValidation(5);
        QVERIFY(manager.isAllDone());
    }
};

QTEST_GUILESS_MAIN(TestValidationStateManager)
#include "test_validation_state_manager.moc"
