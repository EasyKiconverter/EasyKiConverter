#pragma once

/**
 * @file ComponentIR.h
 * @brief 顶层组件聚合结构体
 *
 * 聚合符号、封装、3D 模型和通用元数据。
 * 来源特有数据通过 sourceMetadata 扩展点传递。
 */

#include "FootprintIR.h"
#include "Model3DIR.h"
#include "SymbolIR.h"

#include <QString>
#include <QVariantMap>

namespace EasyKiConverter {
namespace IR {

/**
 * @brief 通用组件（顶层聚合）
 *
 * 将符号、封装、3D 模型和元数据聚合为一个完整的组件描述。
 * 所有字段均为 EDA 无关的通用数据。
 *
 * EasyEDA 特有字段（lcscId、uuid、docType 等）通过 sourceMetadata
 * 传递，导出器按需读取但不依赖其存在。
 */
struct ComponentIR {
    // === 通用元数据 ===
    QString name;  ///< 组件名称
    QString description;  ///< 组件描述
    QString prefix;  ///< 位号前缀（"U", "R", "C" 等）
    QString package;  ///< 封装名称
    QString manufacturer;  ///< 制造商
    QString manufacturerPart;  ///< 制造商料号
    QString datasheet;  ///< 数据手册 URL

    // === 来源特有数据扩展点 ===
    /**
     * @brief 来源特有的元数据键值对
     *
     * EasyEDA 来源可能包含：lcscId, jlcId, uuid, docType, datastrid,
     * jlcOnSale, supplierPart, supplier, jlcpcbPartClass 等。
     *
     * Altium 来源可能包含：vaultGuid, itemGuid, revisionGuid 等。
     *
     * 导出器可按需读取，但不应假设这些字段存在。
     * 使用示例：
     * @code
     *   if (ir.sourceMetadata.contains("lcscId")) {
     *       addProperty("LCSC Part", ir.sourceMetadata["lcscId"].toString());
     *   }
     * @endcode
     */
    QVariantMap sourceMetadata;  ///< 来源格式特有的元数据键值对。

    // === 组件数据 ===
    SymbolComponentIR symbol;  ///< 原理图符号
    FootprintComponentIR footprint;  ///< PCB 封装
    Model3DIR model3D;  ///< 3D 模型

    /**
     * @brief 判断是否包含有效的符号数据。
     * @return 符号含有图形或名称时返回 true。
     */
    bool hasSymbol() const {
        return symbol.hasGraphics() || !symbol.name.isEmpty();
    }

    /**
     * @brief 判断是否包含有效的封装数据。
     * @return 封装含有图形或名称时返回 true。
     */
    bool hasFootprint() const {
        return footprint.hasGraphics() || !footprint.name.isEmpty();
    }

    /**
     * @brief 判断是否包含有效的三维模型数据。
     * @return 三维模型通过自身校验时返回 true。
     */
    bool hasModel3D() const {
        return model3D.isValid();
    }

    /**
     * @brief 判断是否为完整组件。
     * @return 同时包含有效符号和封装时返回 true。
     */
    bool isComplete() const {
        return hasSymbol() && hasFootprint();
    }

    /**
     * @brief 清空组件元数据、符号、封装和三维模型。
     */
    void clear() {
        name.clear();
        description.clear();
        prefix.clear();
        package.clear();
        manufacturer.clear();
        manufacturerPart.clear();
        datasheet.clear();
        sourceMetadata.clear();
        symbol.clear();
        footprint.clear();
        model3D.clear();
    }
};

}  // namespace IR
}  // namespace EasyKiConverter
