#include "core/utils/GzipUtils.h"

#include <QByteArray>
#include <QDebug>

#include <zlib.h>

// GZIP 文件头魔数为 0x1f 0x8b。
static constexpr int GZIP_MAGIC_1 = 0x1f;  ///< GZIP 头部第一个魔数。
static constexpr int GZIP_MAGIC_2 = 0x8b;  ///< GZIP 头部第二个魔数。

// 安全限制：防止解压炸弹
static constexpr qint64 MAX_DECOMPRESSED_SIZE = 500 * 1024 * 1024;  ///< 解压后允许的最大数据量（字节）。
static constexpr int MAX_COMPRESSION_RATIO = 100;  ///< 允许的最大压缩比。

/**
 * @brief 判断输入数据是否带有 GZIP 文件头。
 * @param data 待检查的数据。
 * @return 数据带有 GZIP 文件头时返回 true。
 */
bool GzipUtils::isGzipped(const QByteArray& data) {
    if (data.size() < 2) {
        return false;
    }
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data.constData());
    return bytes[0] == GZIP_MAGIC_1 && bytes[1] == GZIP_MAGIC_2;
}

/**
 * @brief 解压 GZIP 数据并执行解压大小和压缩比安全限制。
 * @param compressedData 待解压的数据。
 * @return 解压结果及成功状态。
 */
GzipUtils::DecompressResult GzipUtils::decompress(const QByteArray& compressedData) {
    if (compressedData.size() < 18) {
        // Too small to be valid gzip, return failure
        return DecompressResult();
    }

    // If not gzip, return as-is with success=true (caller should handle non-gzip data)
    if (!isGzipped(compressedData)) {
        return DecompressResult(compressedData, true);
    }

    z_stream stream;  ///< zlib 解压状态。
    memset(&stream, 0, sizeof(stream));

    if (inflateInit2(&stream, 15 + 16) != Z_OK) {
        qWarning() << "Failed to initialize zlib";
        return DecompressResult();
    }

    stream.next_in = (Bytef*)compressedData.data();
    stream.avail_in = compressedData.size();
    stream.next_out = nullptr;
    stream.avail_out = 0;

    QByteArray decompressed;  ///< 累积的解压数据。
    const int bufferSize = 8192;  ///< 每轮解压使用的缓冲区大小。
    char buffer[bufferSize];  ///< 当前轮次的解压缓冲区。

    int ret;  ///< zlib 当前轮次的处理结果。
    do {
        stream.avail_out = bufferSize;
        stream.next_out = (Bytef*)buffer;

        ret = inflate(&stream, Z_NO_FLUSH);

        if (ret == Z_OK || ret == Z_STREAM_END) {
            decompressed.append(buffer, bufferSize - stream.avail_out);

            // 安全检查：解压大小上限
            if (decompressed.size() > MAX_DECOMPRESSED_SIZE) {
                qWarning() << "Gzip decompression aborted: output exceeds" << MAX_DECOMPRESSED_SIZE << "bytes";
                inflateEnd(&stream);
                return DecompressResult();
            }

            // 安全检查：压缩比阈值（防止解压炸弹）
            if (compressedData.size() > 0) {
                int ratio = decompressed.size() / compressedData.size();  ///< 当前解压与压缩数据的大小比。
                if (ratio > MAX_COMPRESSION_RATIO) {
                    qWarning() << "Gzip decompression aborted: compression ratio" << ratio << "exceeds limit of"
                               << MAX_COMPRESSION_RATIO;
                    inflateEnd(&stream);
                    return DecompressResult();
                }
            }
        }
    } while (ret == Z_OK);

    inflateEnd(&stream);

    if (ret != Z_STREAM_END) {
        qWarning() << "Failed to decompress gzip data, error:" << ret;
        return DecompressResult();
    }

    return DecompressResult(decompressed, true);
}
