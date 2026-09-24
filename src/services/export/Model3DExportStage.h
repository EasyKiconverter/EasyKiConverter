#pragma once

#include "ExportProgress.h"
#include "ExportTypeStage.h"
#include "TempFileManager.h"

#include <QHash>
#include <QSet>

namespace EasyKiConverter {

/**
 * @brief 3D模型导出阶段
 *
 * 管理3D模型(Model3D)的并行导出任务。
 * 每个元器件的3D模型导出由Model3DExportWorker执行。
 *
 * 目录结构:
 *   <outputPath>/
 *   └── 3dmodels/               ← 3D模型目录
 *       ├── component1.wrl
 *       ├── component2.wrl
 *       └── ...
 *
 * 使用 TempFileManager 进行临时写入、备份提交和失败回滚。
 *
 * 并发配置:
 * - 最大并发数: 2（3D模型文件较大，I/O较慢）
 */
class Model3DExportStage : public ExportTypeStage {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit Model3DExportStage(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~Model3DExportStage() override;

    /**
     * @brief 设置导出选项
     * @param options 导出选项配置
     */
    void setOptions(const ExportOptions& options) {
        m_options = options;
    }

    /**
     * @brief 开始3D模型导出
     * @param componentIds 需要导出的元器件ID列表
     * @param cachedData 预加载的元器件数据（key: componentId, value: ComponentData）
     */
    void start(const QStringList& componentIds,
               const QMap<QString, QSharedPointer<ComponentData>>& cachedData) override;

    /**
     * @brief 取消导出
     */
    void cancel() override;

protected:
    /**
     * @brief 创建Model3DExportWorker实例
     * @return 新的worker对象
     */
    QObject* createWorker() override;

    /**
     * @brief 启动worker执行3D模型导出
     * @param worker worker实例
     * @param componentId 元器件ID
     * @param data 预加载的元器件数据
     */
    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override;

private:
    /**
     * @brief 写入独立三维模型与组件库对象之间的关联清单。
     *
     * 清单使用项目自有 JSON 格式，仅描述实际生成的文件和导出状态，
     * 不冒充目标 EDA 的原生三维关联数据。
     */
    void writeAssociationManifest();

    struct TempFilePaths {
        QString wrlTempPath;
        QString wrlFinalPath;
        QString stepTempPath;
        QString stepFinalPath;
    };

    struct ExportOptions m_options;  ///< 导出选项
    TempFileManager m_tempManager;  ///< 临时文件管理器
    QMap<QString, TempFilePaths> m_componentPaths;  ///< componentId -> temp/final paths
    QHash<QString, QString> m_modelFileStems;  ///< componentId -> 去重后的三维模型文件名主体
    QSet<QString> m_skippedComponents;  ///< 没有可导出三维数据或格式的元器件
    QHash<QString, QString> m_preflightErrors;  ///< 启动前路径准备失败的元器件及错误信息
};

}  // namespace EasyKiConverter
