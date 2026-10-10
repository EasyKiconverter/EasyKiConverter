#ifndef EASYEDASYMBOLIMPORTER_H
#define EASYEDASYMBOLIMPORTER_H

#include "models/SymbolData.h"

#include <QJsonObject>
#include <QSharedPointer>
#include <QStringList>

namespace EasyKiConverter {

class EasyedaSymbolImporter {
public:
    EasyedaSymbolImporter();

    QSharedPointer<SymbolData> importSymbolData(const QJsonObject& cadData);

private:
    SymbolPin importPinData(const QString& pinData);
    SymbolRectangle importRectangleData(const QString& rectangleData);
    SymbolCircle importCircleData(const QString& circleData);
    SymbolArc importArcData(const QString& arcData);
    SymbolEllipse importEllipseData(const QString& ellipseData);
    SymbolPolyline importPolylineData(const QString& polylineData);
    SymbolPolygon importPolygonData(const QString& polygonData);
    SymbolPath importPathData(const QString& pathData);
    SymbolImage importImageData(const QString& imageData);
    SymbolText importTextData(const QString& textData);

    QList<QStringList> parsePinDataString(const QString& pinData) const;
};

}  // namespace EasyKiConverter

#endif  // EASYEDASYMBOLIMPORTER_H
