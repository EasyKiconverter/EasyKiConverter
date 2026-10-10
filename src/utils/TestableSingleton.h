#ifndef TESTABLESINGLETON_H
#define TESTABLESINGLETON_H

#include <QMutex>
#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief Template for testable singleton pattern
 *
 * Usage:
 *   class MyService : public TestableSingleton<MyService> {
 *       friend class TestableSingleton<MyService>;
 *       MyService() = default;
 *   public:
 *       void doWork();
 *   };
 *
 * Testing:
 *   MyService::setTestingInstance(new MockMyService());
 *   // run tests
 *   MyService::destroyInstance();
 */
template <typename T>
class TestableSingleton {
public:
    /**
     * @brief 返回测试可替换的单例实例。
     * @return 当前 mock 实例或延迟创建的正式实例。
     */
    static T& instance() {
        if (s_mockInstance) {
            return *s_mockInstance;
        }

        QMutexLocker locker(&s_mutex);
        if (!s_instance) {
            s_instance = new T();
        }
        return *s_instance;
    }

    /**
     * @brief 销毁正式单例实例。
     */
    static void destroyInstance() {
        QMutexLocker locker(&s_mutex);
        delete s_instance;
        s_instance = nullptr;
    }

    /**
     * @brief 设置测试用单例实例。
     * @param mock 测试替身实例，传入 nullptr 可清除正式实例。
     */
    static void setTestingInstance(T* mock) {
        QMutexLocker locker(&s_mutex);
        delete s_instance;
        s_instance = mock;
        s_mockInstance = mock;
    }

    /**
     * @brief 清除测试替身引用。
     */
    static void clearMockInstance() {
        QMutexLocker locker(&s_mutex);
        s_mockInstance = nullptr;
    }

    /**
     * @brief 判断正式单例实例是否存在。
     * @return 正式实例已创建时返回 true。
     */
    static bool hasInstance() {
        QMutexLocker locker(&s_mutex);
        return s_instance != nullptr;
    }

    /**
     * @brief 判断测试替身实例是否存在。
     * @return 测试替身已设置时返回 true。
     */
    static bool hasMockInstance() {
        return s_mockInstance != nullptr;
    }

protected:
    TestableSingleton() = default;
    virtual ~TestableSingleton() = default;

private:
    static QMutex s_mutex;  ///< 保护单例实例指针的互斥锁。
    static T* s_instance;  ///< 正式单例实例指针。
    static T* s_mockInstance;  ///< 测试替身实例指针。
};

template <typename T>
QMutex TestableSingleton<T>::s_mutex;

template <typename T>
T* TestableSingleton<T>::s_instance = nullptr;

template <typename T>
T* TestableSingleton<T>::s_mockInstance = nullptr;

}  // namespace EasyKiConverter

#endif  // TESTABLESINGLETON_H
