#pragma once

// globeMath.h
// Coordinates and orientations for the VPN page's background globe.
//
// World space is the unit sphere with the north pole at +Y and the point at
// latitude 0, longitude 0 facing the viewer at +Z; east is toward +X. View
// space is the same axes as seen on screen: +X right, +Y up, +Z toward the
// viewer. An orientation is the rotation that takes world space to view space.

#include <QQuaternion>
#include <QVector3D>
#include <QtMath>
#include <algorithm>
#include <cmath>

struct GeoPoint
{
    double latitude  = 0.0; // degrees, north positive
    double longitude = 0.0; // degrees, east positive

    bool operator==(const GeoPoint&) const = default;
};

namespace GlobeMath
{
// Earth's axial tilt, which a desk globe's axis is mounted at.
constexpr float AXIAL_TILT_DEGREES = 23.44f;

constexpr double DEGREES_PER_TURN      = 360.0;
constexpr double DEGREES_PER_HALF_TURN = 180.0;

inline QVector3D toWorld(const GeoPoint& p)
{
    const double lat = qDegreesToRadians(p.latitude);
    const double lon = qDegreesToRadians(p.longitude);
    return QVector3D(static_cast<float>(std::cos(lat) * std::sin(lon)),
                     static_cast<float>(std::sin(lat)),
                     static_cast<float>(std::cos(lat) * std::cos(lon)));
}

inline GeoPoint fromWorld(const QVector3D& v)
{
    const QVector3D n = v.normalized();
    return {qRadiansToDegrees(std::asin(std::clamp(static_cast<double>(n.y()), -1.0, 1.0))),
            qRadiansToDegrees(std::atan2(static_cast<double>(n.x()), static_cast<double>(n.z())))};
}

// Rotation about the polar axis that brings `longitude` round to face the viewer.
inline QQuaternion spin(const double longitude)
{
    return QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, static_cast<float>(-longitude));
}

// Fixed part of the idle orientation: the axis tilted in the screen plane with
// the north pole leaning right, the way a desk globe sits on its stand.
inline QQuaternion idleFrame()
{
    return QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, -AXIAL_TILT_DEGREES);
}

// Fixed part of the orientation that looks straight at `latitude`, north up.
inline QQuaternion lookAtFrame(const double latitude)
{
    return QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, static_cast<float>(latitude));
}

// Idle orientation: `centerLongitude` faces the viewer on the tilted globe.
inline QQuaternion idle(const double centerLongitude)
{
    return idleFrame() * spin(centerLongitude);
}

// Orientation that puts `target` dead center, facing the viewer, north up.
inline QQuaternion lookAt(const GeoPoint& target)
{
    return lookAtFrame(target.latitude) * spin(target.longitude);
}

// The point of the globe at the center of the view under `orientation`.
inline GeoPoint centerOf(const QQuaternion& orientation)
{
    return fromWorld(orientation.conjugated().rotatedVector(QVector3D(0.0f, 0.0f, 1.0f)));
}

// Angle in degrees between two orientations (0 to 180).
inline double angleBetween(const QQuaternion& a, const QQuaternion& b)
{
    const double dot = std::clamp(std::abs(static_cast<double>(QQuaternion::dotProduct(a, b))), 0.0, 1.0);
    return qRadiansToDegrees(2.0 * std::acos(dot));
}

namespace Detail
{
// Minimax polynomial for atan on [0, 1]; |error| < 1e-5 rad.
constexpr float ATAN_C1 = 0.99997726f;
constexpr float ATAN_C3 = -0.33262347f;
constexpr float ATAN_C5 = 0.19354346f;
constexpr float ATAN_C7 = -0.11643287f;
constexpr float ATAN_C9 = 0.05265332f;
constexpr float ATAN_C11 = -0.01172120f;
constexpr float HALF_PI = 1.57079633f;
constexpr float PI = 3.14159265f;
} // namespace Detail

// atan2 accurate to about 1e-5 rad and several times faster than std::atan2;
// used per pixel while the globe turns.
inline float fastAtan2(const float y, const float x)
{
    const float ax = std::fabs(x);
    const float ay = std::fabs(y);
    const float maxAbs = std::max(ax, ay);
    if (maxAbs == 0.0f)
    {
        return 0.0f;
    }
    const float a = std::min(ax, ay) / maxAbs;
    const float s = a * a;
    float r = ((((((Detail::ATAN_C11 * s + Detail::ATAN_C9) * s + Detail::ATAN_C7) * s
                 + Detail::ATAN_C5) * s + Detail::ATAN_C3) * s + Detail::ATAN_C1) * a);
    if (ay > ax)
    {
        r = Detail::HALF_PI - r;
    }
    if (x < 0.0f)
    {
        r = Detail::PI - r;
    }
    return (y < 0.0f) ? -r : r;
}
} // namespace GlobeMath
