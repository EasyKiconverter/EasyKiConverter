#ifndef CACHEDATAVALIDATOR_H
#define CACHEDATAVALIDATOR_H

#include <QByteArray>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 校验不同类型缓存数据是否可安全使用。
 *
 * 该类只处理内存中的数据格式，不负责文件访问、缓存目录和生命周期管理。
 */
class CacheDataValidator final {
public:
    /**
     * @brief 判断数据是否为可解析的 CAD JSON 对象。
     * @param data 待校验的 CAD JSON 数据。
     */
    static bool isValidCadData(const QByteArray& data);

    /**
     * @brief 判断数据是否为 Qt 可解码的预览图。
     * @param data 待校验的预览图数据。
     */
    static bool isValidPreviewImage(const QByteArray& data);

    /**
     * @brief 判断数据是否符合 PDF 或 HTML 数据手册格式。
     * @param data 数据手册内容。
     * @param format 声明格式（pdf/html）。
     */
    static bool isValidDatasheet(const QByteArray& data, const QString& format);

    /**
     * @brief 判断三维模型内容是否包含可用几何数据。
     * @param data 模型文件内容。
     * @param extension 文件扩展名（obj/wrl/step）。
     */
    static bool isUsableModel3D(const QByteArray& data, const QString& extension);
};

}  // namespace EasyKiConverter

#endif  // CACHEDATAVALIDATOR_H
