#ifndef EXPORTER3DMODEL_H
#define EXPORTER3DMODEL_H

#include "core/interfaces/IModel3DExporter.h"
#include "core/ir/Model3DIR.h"

#include <QByteArray>
#include <QObject>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 3D 模型导出器类
 *
 * 用于处理 EasyEDA 3D 模型数据的下载和转换
 */
class Exporter3DModel : public QObject, public IModel3DExporter {
    Q_OBJECT

public:
    /**
     * @brief 3D 模型格式枚举
     */
    enum class ModelFormat {
        OBJ,  // Wavefront OBJ 格式
        STEP  // STEP 格式
    };
    Q_ENUM(ModelFormat)

    /**
     * @brief 构造函数
         *
     * @param parent 父对象
         */
    explicit Exporter3DModel(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~Exporter3DModel() override;

    /**
     * @brief 下载 3D 模型（OBJ 格式
         *
     * @param uuid 模型 UUID
     * @param savePath 保存路径
     */
    void downloadObjModel(const QString& uuid, const QString& savePath);

    /**
     * @brief 下载 3D 模型（STEP 格式
         *
     * @param uuid 模型 UUID
     * @param savePath 保存路径
     */
    void downloadStepModel(const QString& uuid, const QString& savePath);

    bool downloadObjModelSync(const QString& uuid, const QString& savePath, QString* errorMessage = nullptr);

    bool downloadStepModelSync(const QString& uuid, const QString& savePath, QString* errorMessage = nullptr);

    bool downloadObjDataSync(const QString& uuid, QByteArray* data, QString* errorMessage = nullptr);

    bool downloadStepDataSync(const QString& uuid, QByteArray* data, QString* errorMessage = nullptr);

    /**
     * @brief 计算 OBJ 顶点数据的最小 Z 坐标（毫米）
     *
     * @param objData OBJ 格式的原始数据
     * @return 毫米单位下的最小 Z 坐标
     */
    static double calculateObjMinZ(const QByteArray& objData);

    /**
     * @brief 检查 OBJ 数据是否包含可导出的顶点和面
     *
     * @param objData OBJ 模型的原始数据
     * @return true 表示数据至少包含一个有效顶点和一个有效面
     */
    static bool hasUsableObjGeometry(const QByteArray& objData);

    /**
     * @brief 检查 WRL 数据是否包含可显示的坐标和面索引
     *
     * @param wrlData WRL 模型的原始数据
     * @return true 表示数据包含坐标点和面索引
     */
    static bool hasUsableWrlGeometry(const QByteArray& wrlData);

    /**
     * @brief 检查 STEP 数据是否具有可接受的交换文件结构
     *
     * @param stepData STEP 模型的原始数据
     * @return true 表示数据包含完整的 STEP 文件头和结束标记
     */
    static bool hasUsableStepData(const QByteArray& stepData);

    /**
     * @brief 计算 WRL 文件在 KiCad 显示单位下的最小 Z 坐标
     *
     * @param wrlData WRL 文件数据
     * @return KiCad 显示单位下的最小 Z 坐标
     */
    static double calculateWrlDisplayMinZ(const QByteArray& wrlData);

    /**
     * @brief 导出模型为 KiCad WRL 格式
     *
     * @param model 模型 IR 数据
     * @param savePath 保存路径
     * @return bool 是否成功
     */
    bool exportToWrl(const IR::Model3DIR& model, const QString& savePath) override;

    /**
     * @brief 导出模型为 STEP 格式
     *
     * @param model 模型 IR 数据
     * @param savePath 保存路径
     * @return bool 是否成功
     */
    bool exportToStep(const IR::Model3DIR& model, const QString& savePath) override;

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
    /**
     * @brief 生成 WRL 文件内容
     *
     * @param model 模型 IR 数据
     * @param objData OBJ 模型数据
     * @return QString WRL 文件内容
     */
    QString generateWrlContent(const IR::Model3DIR& model, const QByteArray& objData);

    /**
     * @brief 解析 OBJ 数据
     *
     * @param objData OBJ 模型数据
     * @return QJsonObject 解析后的数据
     */
    QJsonObject parseObjData(const QByteArray& objData);

    /**
     * @brief 获取模型 URL
     *
     * @param uuid 模型 UUID
     * @param format 模型格式
     * @return QString 模型 URL
     */
    QString getModelUrl(const QString& uuid, ModelFormat format) const;

    /**
     * @brief 同步下载指定格式的三维模型。
     * @param uuid 模型 UUID。
     * @param savePath 保存路径。
     * @param format 模型格式。
     * @param errorMessage 可选的错误信息输出指针。
     * @return 下载成功时返回 true。
     */
    bool downloadModelSync(const QString& uuid,
                           const QString& savePath,
                           ModelFormat format,
                           QString* errorMessage = nullptr);

    /**
     * @brief 同步下载指定格式的三维模型数据。
     * @param uuid 模型 UUID。
     * @param format 模型格式。
     * @param data 输出模型数据。
     * @param errorMessage 可选的错误信息输出指针。
     * @return 下载成功时返回 true。
     */
    bool downloadModelDataSync(const QString& uuid,
                               ModelFormat format,
                               QByteArray* data,
                               QString* errorMessage = nullptr);

private:
    QString m_currentUuid;  ///< 当前处理的三维模型 UUID。
    QString m_savePath;  ///< 当前三维模型的本地保存路径。
};

}  // namespace EasyKiConverter

#endif  // EXPORTER3DMODEL_H
