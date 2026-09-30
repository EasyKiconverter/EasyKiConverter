#pragma once

#include <QCryptographicHash>
#include <QString>

namespace EasyKiConverter {

/** @brief Horizon 对象的确定性 UUID 生成器（独立实现，不复制 Horizon GPL 代码）。 */
class HorizonUuid final {
public:
    static QString make(const QString& kind, const QString& semanticKey) {
        const QByteArray input =
            (QStringLiteral("EasyKiConverter/Horizon/1/") + kind + QLatin1Char('/') + semanticKey).toUtf8();
        QByteArray bytes = QCryptographicHash::hash(input, QCryptographicHash::Sha1).left(16);
        bytes[6] = char((static_cast<unsigned char>(bytes.at(6)) & 0x0f) | 0x50);
        bytes[8] = char((static_cast<unsigned char>(bytes.at(8)) & 0x3f) | 0x80);
        QString result;
        for (int i = 0; i < bytes.size(); ++i) {
            result += QStringLiteral("%1").arg(static_cast<unsigned char>(bytes.at(i)), 2, 16, QLatin1Char('0'));
            if (i == 3 || i == 5 || i == 7 || i == 9)
                result += QLatin1Char('-');
        }
        return result;
    }
};

}  // namespace EasyKiConverter
