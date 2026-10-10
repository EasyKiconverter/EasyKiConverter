#include "PreviewImageDataEncoder.h"

#include <QFile>
#include <QRegularExpression>

namespace EasyKiConverter {

/**
 * @brief 从文件名中提取预览图序号。
 * @param path 预览图文件路径。
 * @return 文件名中的预览图序号，无法解析时返回 -1。
 */
int PreviewImageDataEncoder::imageIndexFromPath(const QString& path) {
    // 匹配不区分大小写的 preview_N.jpg 文件名。
    static const QRegularExpression re(QStringLiteral("preview_(\\d+)\\.jpg$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(path);  // 当前路径的文件名匹配结果。
    if (!match.hasMatch()) {
        return -1;
    }

    bool ok = false;  // 记录序号文本的整数转换结果。
    const int index = match.captured(1).toInt(&ok);  // 提取出的预览图序号。
    return ok ? index : -1;
}

/**
 * @brief 读取预览图文件并同时生成 Base64 和原始数据。
 * @param imagePaths 预览图文件路径列表。
 * @return 按文件名序号排列的编码结果。
 */
PreviewImageDataResult PreviewImageDataEncoder::encodeFiles(const QStringList& imagePaths) {
    PreviewImageDataResult result;  // 保存全部预览图的编码和原始字节。
    result.encodedImages.fill(QString(), 3);
    result.imageData.resize(3);

    for (const QString& path : imagePaths) {
        const int imageIndex = imageIndexFromPath(path);  // 当前文件对应的预览图序号。
        if (imageIndex < 0 || imageIndex >= result.encodedImages.size()) {
            continue;
        }

        QFile file(path);  // 当前预览图文件。
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        const QByteArray data = file.readAll();  // 当前预览图原始字节。
        result.encodedImages[imageIndex] = QString::fromLatin1(data.toBase64());
        result.imageData[imageIndex] = data;
    }

    return result;
}

}  // namespace EasyKiConverter
