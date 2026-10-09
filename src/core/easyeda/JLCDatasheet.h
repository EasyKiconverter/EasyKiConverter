#ifndef JLCDATASHEET_H
#define JLCDATASHEET_H

#include "core/network/AsyncNetworkRequest.h"

#include <QFile>
#include <QObject>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief JLC 数据手册下载器类
 *
 * 用于下载 JLC/LCSC 元件的数据手
     */
class JLCDatasheet : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
         *
     * @param parent 父对象
         */
    explicit JLCDatasheet(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~JLCDatasheet() override;

    /**
     * @brief 下载数据手册
     *
     * @param datasheetUrl 数据手册 URL
     * @param savePath 保存路径
     */
    void downloadDatasheet(const QString& datasheetUrl, const QString& savePath);

    void setWeakNetworkSupport(bool enabled);
    bool weakNetworkSupport() const;

    /**
     * @brief 取消下载
     */
    void cancel();

signals:
    /**
     * @brief 下载成功信号
     *
     * @param filePath 保存的文件路
         */
    void downloadSuccess(const QString& filePath);

    /**
     * @brief 下载失败信号
     *
     * @param errorMessage 错误信息
     */
    void downloadError(const QString& errorMessage);

    /**
     * @brief 下载进度信号
     *
     * @param bytesReceived 已接收的字节
         * @param bytesTotal 总字节数
     */
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    AsyncNetworkRequest* m_activeRequest;  ///< 当前进行中的网络请求。
    QString m_savePath;  ///< 数据手册保存路径。
    QString m_datasheetUrl;  ///< 当前数据手册下载地址。
    bool m_isDownloading;  ///< 是否正在下载数据手册。
    bool m_weakNetworkSupport;  ///< 是否启用弱网重试策略。
};

}  // namespace EasyKiConverter

#endif  // JLCDATASHEET_H
