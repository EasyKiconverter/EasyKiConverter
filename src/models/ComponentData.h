#ifndef COMPONENTDATA_H
#define COMPONENTDATA_H

#include "FootprintData.h"
#include "Model3DData.h"
#include "SymbolData.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 元件数据
     *
 * 包含元件的所有信息，包括符号、封装和 3D 模型
 */
class ComponentData {
public:
    ComponentData();
    ~ComponentData() = default;

    // Getter 和 Setter 方法
    /**
     * @brief 返回 LCSC 元件编号。
     * @return LCSC 元件编号。
     */
    QString lcscId() const {
        return m_lcscId;
    }

    /**
     * @brief 设置 LCSC 元件编号。
     * @param id LCSC 元件编号。
     */
    void setLcscId(const QString& id) {
        m_lcscId = id;
    }

    /**
     * @brief 返回元件名称。
     * @return 元件名称。
     */
    QString name() const {
        return m_name;
    }

    /**
     * @brief 设置元件名称。
     * @param name 元件名称。
     */
    void setName(const QString& name) {
        m_name = name;
    }

    /**
     * @brief 返回元件参考标识前缀。
     * @return 参考标识前缀。
     */
    QString prefix() const {
        return m_prefix;
    }

    /**
     * @brief 设置元件参考标识前缀。
     * @param prefix 参考标识前缀。
     */
    void setPrefix(const QString& prefix) {
        m_prefix = prefix;
    }

    /**
     * @brief 返回封装名称。
     * @return 封装名称。
     */
    QString package() const {
        return m_package;
    }

    /**
     * @brief 设置封装名称。
     * @param package 封装名称。
     */
    void setPackage(const QString& package) {
        m_package = package;
    }

    /**
     * @brief 返回制造商名称。
     * @return 制造商名称。
     */
    QString manufacturer() const {
        return m_manufacturer;
    }

    /**
     * @brief 设置制造商名称。
     * @param manufacturer 制造商名称。
     */
    void setManufacturer(const QString& manufacturer) {
        m_manufacturer = manufacturer;
    }

    /**
     * @brief 返回制造商部件号。
     * @return 制造商部件号。
     */
    QString manufacturerPart() const {
        return m_manufacturerPart;
    }

    /**
     * @brief 设置制造商部件号。
     * @param manufacturerPart 制造商部件号。
     */
    void setManufacturerPart(const QString& manufacturerPart) {
        m_manufacturerPart = manufacturerPart;
    }

    /**
     * @brief 返回数据手册链接。
     * @return 数据手册链接。
     */
    QString datasheet() const {
        return m_datasheet;
    }

    /**
     * @brief 设置数据手册链接。
     * @param datasheet 数据手册链接。
     */
    void setDatasheet(const QString& datasheet) {
        m_datasheet = datasheet;
    }

    /**
     * @brief 返回预览图链接列表。
     * @return 预览图链接列表。
     */
    QStringList previewImages() const {
        return m_previewImages;
    }

    /**
     * @brief 设置预览图链接列表。
     * @param images 预览图链接列表。
     */
    void setPreviewImages(const QStringList& images) {
        m_previewImages = images;
    }

    /**
     * @brief 追加非空且未重复的预览图链接。
     * @param imageUrl 待追加的图片链接。
     */
    void addPreviewImage(const QString& imageUrl) {
        if (!imageUrl.isEmpty() && !m_previewImages.contains(imageUrl)) {
            m_previewImages.append(imageUrl);
        }
    }

    /**
     * @brief 返回按预览图索引排列的图片数据。
     * @return 图片数据列表。
     */
    QList<QByteArray> previewImageData() const {
        return m_previewImageData;
    }

    /**
     * @brief 设置按预览图索引排列的图片数据。
     * @param data 图片数据列表。
     */
    void setPreviewImageData(const QList<QByteArray>& data) {
        m_previewImageData = data;
    }

    /**
     * @brief 按索引写入预览图数据并避免覆盖已有内容。
     * @param data 图片数据。
     * @param imageIndex 目标索引；为负数时追加到列表末尾。
     */
    void addPreviewImageData(const QByteArray& data, int imageIndex = -1) {
        qDebug() << "ComponentData::addPreviewImageData called - data size:" << data.size()
                 << "bytes, index:" << imageIndex << "current count:" << m_previewImageData.size();

        if (data.isEmpty()) {
            qDebug() << "  Skipping: data is empty";
            return;
        }

        // 指定索引时先扩容到目标位置，保证乱序回调不会改变图片与索引的对应关系。
        if (imageIndex >= 0) {
            if (imageIndex >= m_previewImageData.size()) {
                m_previewImageData.resize(imageIndex + 1);
            }

            // 如果该索引已经有数据，跳过（避免重复添加）
            if (!m_previewImageData[imageIndex].isEmpty()) {
                qDebug() << "  Skipping: index" << imageIndex << "already has data ("
                         << m_previewImageData[imageIndex].size() << "bytes)";
                return;
            }
            // 替换该索引的数据
            qDebug() << "  Replacing data at index:" << imageIndex;
            m_previewImageData[imageIndex] = data;
        } else {
            // 没有指定索引或索引超出范围，直接追加
            qDebug() << "  Appending data, new count:" << (m_previewImageData.size() + 1);
            m_previewImageData.append(data);
        }

        qDebug() << "  Final count:" << m_previewImageData.size();
    }

    /**
     * @brief 返回数据手册原始数据。
     * @return 数据手册字节数据。
     */
    QByteArray datasheetData() const {
        return m_datasheetData;
    }

    /**
     * @brief 设置数据手册原始数据。
     * @param data 数据手册字节数据。
     */
    void setDatasheetData(const QByteArray& data) {
        m_datasheetData = data;
    }

    /**
     * @brief 返回数据手册格式。
     * @return 数据手册格式。
     */
    QString datasheetFormat() const {
        return m_datasheetFormat;
    }

    /**
     * @brief 设置数据手册格式。
     * @param format 数据手册格式。
     */
    void setDatasheetFormat(const QString& format) {
        m_datasheetFormat = format;
    }

    /**
     * @brief 返回符号数据。
     * @return 符号数据共享指针。
     */
    QSharedPointer<SymbolData> symbolData() const {
        return m_symbolData;
    }

    /**
     * @brief 设置符号数据。
     * @param data 符号数据共享指针。
     */
    void setSymbolData(const QSharedPointer<SymbolData>& data) {
        m_symbolData = data;
    }

    /**
     * @brief 返回封装数据。
     * @return 封装数据共享指针。
     */
    QSharedPointer<FootprintData> footprintData() const {
        return m_footprintData;
    }

    /**
     * @brief 设置封装数据。
     * @param data 封装数据共享指针。
     */
    void setFootprintData(const QSharedPointer<FootprintData>& data) {
        m_footprintData = data;
    }

    /**
     * @brief 返回独立的三维模型数据。
     * @return 三维模型数据共享指针。
     */
    QSharedPointer<Model3DData> model3DData() const {
        return m_model3DData;
    }

    /**
     * @brief 设置独立的三维模型数据。
     * @param data 三维模型数据共享指针。
     */
    void setModel3DData(const QSharedPointer<Model3DData>& data) {
        m_model3DData = data;
    }

    // Debug 导出用的原始数据访问器
    /**
     * @brief 返回元件信息原始 JSON。
     * @return 原始元件信息 JSON。
     */
    QByteArray cinfoJsonRaw() const {
        return m_cinfoJsonRaw;
    }

    /**
     * @brief 设置元件信息原始 JSON。
     * @param data 原始元件信息 JSON。
     */
    void setCinfoJsonRaw(const QByteArray& data) {
        m_cinfoJsonRaw = data;
    }

    /**
     * @brief 返回 CAD 原始 JSON。
     * @return 原始 CAD JSON。
     */
    QByteArray cadJsonRaw() const {
        return m_cadJsonRaw;
    }

    /**
     * @brief 设置 CAD 原始 JSON。
     * @param data 原始 CAD JSON。
     */
    void setCadJsonRaw(const QByteArray& data) {
        m_cadJsonRaw = data;
    }

    /**
     * @brief 返回三维模型 OBJ 原始数据。
     * @return 原始 OBJ 数据。
     */
    QByteArray model3DObjRaw() const {
        return m_model3DObjRaw;
    }

    /**
     * @brief 设置三维模型 OBJ 原始数据。
     * @param data 原始 OBJ 数据。
     */
    void setModel3DObjRaw(const QByteArray& data) {
        m_model3DObjRaw = data;
    }

    /**
     * @brief 将组件数据序列化为 JSON。
     * @return 组件数据 JSON 对象。
     */
    QJsonObject toJson() const;

    /**
     * @brief 从 JSON 替换当前组件数据。
     * @param json 组件数据 JSON 对象。
     * @return 解析成功时返回 true。
     */
    bool fromJson(const QJsonObject& json);

    /**
     * @brief 检查组件数据是否有效。
     * @return 数据有效时返回 true。
     */
    bool isValid() const;

    /**
     * @brief 返回组件数据的校验错误。
     * @return 首个校验错误；无错误时返回空字符串。
     */
    QString validate() const;

    /**
     * @brief 清空组件及其关联数据。
     */
    void clear();

private:
    QString m_lcscId;  // LCSC 元件编号
    QString m_name;  // 元件名称
    QString m_prefix;  // 元件前缀
    QString m_package;  // 封装名称
    QString m_manufacturer;  // 制造商
    QString m_manufacturerPart;  // 制造商部件号
    QString m_datasheet;  // 数据手册链接
    QByteArray m_datasheetData;  // 数据手册数据（内存）
    QString m_datasheetFormat;  // 数据手册格式（pdf/html）
    QStringList m_previewImages;  // 预览图 URL 列表
    QList<QByteArray> m_previewImageData;  // 预览图数据列表（内存）

    QSharedPointer<SymbolData> m_symbolData;  // 符号数据
    QSharedPointer<FootprintData> m_footprintData;  // 封装数据
    QSharedPointer<Model3DData> m_model3DData;  // 3D 模型数据

    // Debug 导出用的原始数据
    QByteArray m_cinfoJsonRaw;  // 元器件信息原始 JSON
    QByteArray m_cadJsonRaw;  // CAD 数据原始 JSON（包含符号和封装）
    QByteArray m_model3DObjRaw;  // 3D 模型 OBJ 原始数据
};

}  // namespace EasyKiConverter

#endif  // COMPONENTDATA_H
