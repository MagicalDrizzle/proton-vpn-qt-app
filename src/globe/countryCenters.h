#pragma once

// countryCenters.h
// Where the globe turns to for a country: Natural Earth's label point for it,
// a spot inside the country chosen for legibility (the plain centroid can fall
// in the sea for coastal or island countries). Generated into
// :/globe/country_centers.json by tools/generate_globe_data.py.

#include <QString>
#include <optional>
#include "globeMath.h"

namespace CountryCenters
{
// `countryCode` is an ISO 3166-1 alpha-2 code, case-insensitive, plus the
// "UK" Proton VPN uses for the United Kingdom. Unknown codes give nullopt.
[[nodiscard]] std::optional<GeoPoint> find(const QString& countryCode);
} // namespace CountryCenters
