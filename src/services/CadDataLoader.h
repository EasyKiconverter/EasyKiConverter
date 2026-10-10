#pragma once

#include "models/ComponentData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"

#include <QJsonObject>
#include <QString>

namespace EasyKiConverter {

class INetworkClient;

struct CadParseResult {
    QString componentId;  ///< 解析请求对应的元器件编号。
    QJsonObject resultData;  ///< 原始 CAD 响应中的结果对象。
    ComponentData componentData;  ///< 解析得到的元器件数据。
    QString errorMessage;  ///< 解析失败时的可读错误信息。
    bool success = false;  ///< 解析是否成功。
};

struct CadFetchTaskResult {
    QString componentId;  ///< 获取任务对应的元器件编号。
    CadParseResult parsed;  ///< 网络响应解析结果。
    QString errorMessage;  ///< 获取或解析失败时的可读错误信息。
    bool success = false;  ///< 获取和解析是否均成功。
};

class CadDataLoader {
public:
    /**
     * @brief 解析 CAD 响应中的元器件数据。
     * @param componentId 元器件编号
     * @param rawData CAD 接口返回的原始 JSON 对象
     * @return 包含解析状态和元器件数据的结果
     */
    static CadParseResult parseCadPayload(const QString& componentId, const QJsonObject& rawData);
    /**
     * @brief 获取并解析指定元器件的 CAD 数据。
     * @param componentId 元器件编号
     * @param networkClient 可选网络客户端，为空时使用默认客户端
     * @return 包含获取和解析状态的任务结果
     */
    static CadFetchTaskResult fetchAndParseCadData(const QString& componentId, INetworkClient* networkClient = nullptr);
};

}  // namespace EasyKiConverter
