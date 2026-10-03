#include "countryCenters.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
// Each entry is [latitude, longitude].
constexpr qsizetype POINT_SIZE = 2;

QHash<QString, GeoPoint> loadCenters()
{
    QHash<QString, GeoPoint> centers;
    QFile file(QStringLiteral(":/globe/country_centers.json"));
    if (file.open(QIODevice::ReadOnly) == false)
    {
        return centers;
    }

    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
    {
        const QJsonArray point = it.value().toArray();
        if (point.size() != POINT_SIZE) continue;
        centers.insert(it.key(), GeoPoint{point.at(0).toDouble(), point.at(1).toDouble()});
    }
    return centers;
}
} // namespace

std::optional<GeoPoint> CountryCenters::find(const QString& countryCode)
{
    // Loaded on first use and kept: ~240 small entries.
    static const QHash<QString, GeoPoint> centers = loadCenters();

    const auto it = centers.constFind(countryCode.trimmed().toUpper());
    if (it == centers.constEnd())
    {
        return std::nullopt;
    }
    return it.value();
}
