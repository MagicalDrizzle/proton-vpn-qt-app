#include <QtTest/QtTest>
#include <cmath>
#include <numbers>
#include "globe/countryCenters.h"
#include "globe/globeMath.h"
#include "globe/globeRenderer.h"

// The globe's geometry is what makes "turn to face the country" true, so it is
// pinned down here: the orientations, the fast atan2, the two render paths
// agreeing with each other, and the country lookup.

namespace
{
constexpr float VECTOR_TOLERANCE = 1e-4f;
constexpr double DEGREE_TOLERANCE = 1e-3;

bool fuzzyEqual(const QVector3D& a, const QVector3D& b)
{
    return (a - b).length() < VECTOR_TOLERANCE;
}

// Mask whose left half (longitudes -180..0) is land and right half water.
QImage halfLandMask()
{
    QImage mask(256, 128, QImage::Format_Grayscale8);
    mask.fill(0);
    for (int y = 0; y < mask.height(); ++y)
    {
        uchar* line = mask.scanLine(y);
        for (int x = 0; x < mask.width() / 2; ++x)
        {
            line[x] = 255;
        }
    }
    return mask;
}

QColor pixelAt(const QImage& image, const int x, const int y)
{
    return QColor::fromRgba(image.pixel(x, y));
}

// A renderer over the half-land mask: blue water, green land.
GlobeRenderer halfLandRenderer(const int size)
{
    GlobeRenderer renderer;
    renderer.setLandMask(halfLandMask());
    renderer.setColors(Qt::blue, Qt::green);
    renderer.setSize(size);
    return renderer;
}
} // namespace

class TstGlobe : public QObject
{
    Q_OBJECT

private slots:
    // GlobeMath ------------------------------------------------------------

    void fastAtan2_wholeCircle_matchesStdAtan2()
    {
        double worst = 0.0;
        for (int i = 0; i < 3600; ++i)
        {
            const double angle = i / 3600.0 * 2.0 * std::numbers::pi;
            for (const float radius : {0.01f, 0.5f, 1.0f, 7.0f})
            {
                const float y = radius * static_cast<float>(std::sin(angle));
                const float x = radius * static_cast<float>(std::cos(angle));
                worst = std::max(worst, std::abs(static_cast<double>(GlobeMath::fastAtan2(y, x)) - std::atan2(y, x)));
            }
        }
        QVERIFY2(worst < 1e-4, qPrintable(QStringLiteral("max error %1 rad").arg(worst)));
    }

    void lookAt_target_isAtViewCenter()
    {
        for (const GeoPoint p : {GeoPoint{0, 0}, GeoPoint{40.7, -74.0}, GeoPoint{-33.9, 151.2},
                                 GeoPoint{64.1, -21.9}, GeoPoint{-54.8, -68.3}})
        {
            const QVector3D view = GlobeMath::lookAt(p).rotatedVector(GlobeMath::toWorld(p));
            QVERIFY(fuzzyEqual(view, QVector3D(0.0f, 0.0f, 1.0f)));
        }
    }

    void lookAt_northStaysUp()
    {
        // The north pole projects straight above the center: x = 0, y > 0.
        const QVector3D pole = GlobeMath::lookAt(GeoPoint{40.7, -74.0}).rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));
        QVERIFY(std::abs(pole.x()) < VECTOR_TOLERANCE);
        QVERIFY(pole.y() > 0.0f);
    }

    void centerOf_lookAt_returnsTarget()
    {
        const GeoPoint target{-33.9, 151.2};
        const GeoPoint center = GlobeMath::centerOf(GlobeMath::lookAt(target));
        QVERIFY(std::abs(center.latitude - target.latitude) < DEGREE_TOLERANCE);
        QVERIFY(std::abs(center.longitude - target.longitude) < DEGREE_TOLERANCE);
    }

    void idle_axisLeansRightByAxialTilt()
    {
        const QVector3D pole = GlobeMath::idle(0.0).rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));
        QVERIFY(pole.x() > 0.0f);
        const double tilt = qRadiansToDegrees(std::atan2(static_cast<double>(pole.x()), static_cast<double>(pole.y())));
        QVERIFY(std::abs(tilt - GlobeMath::AXIAL_TILT_DEGREES) < DEGREE_TOLERANCE);
    }

    void idle_centerLongitude_facesViewer()
    {
        const GeoPoint center = GlobeMath::centerOf(GlobeMath::idle(120.0));
        QVERIFY(std::abs(center.longitude - 120.0) < DEGREE_TOLERANCE);
    }

    // GlobeRenderer ----------------------------------------------------------

    void render_targetOnLand_centerPixelIsLand()
    {
        GlobeRenderer renderer = halfLandRenderer(64);

        // Longitude -90 is land in the half mask, +90 is water.
        const QImage land = renderer.render(GlobeMath::lookAt(GeoPoint{0, -90}));
        QCOMPARE(pixelAt(land, 32, 32).green() > pixelAt(land, 32, 32).blue(), true);
        const QImage water = renderer.render(GlobeMath::lookAt(GeoPoint{0, 90}));
        QCOMPARE(pixelAt(water, 32, 32).blue() > pixelAt(water, 32, 32).green(), true);
    }

    void render_outsideDisc_isTransparent()
    {
        GlobeRenderer renderer = halfLandRenderer(64);
        const QImage image = renderer.render(GlobeMath::idle(0.0));
        QCOMPARE(qAlpha(image.pixel(0, 0)), 0);
        QCOMPARE(qAlpha(image.pixel(63, 63)), 0);
    }

    void renderSpin_matchesGeneralPath()
    {
        // The fast spin path must draw what the general path draws for the
        // same orientation; allow a few pixels of rounding along coastlines.
        GlobeRenderer renderer = halfLandRenderer(128);

        for (const double longitude : {0.0, 37.5, -142.0})
        {
            const QImage fast = renderer.renderSpin(GlobeMath::idleFrame(), longitude).copy();
            const QImage general = renderer.render(GlobeMath::idle(longitude)).copy();
            int differing = 0;
            int total = 0;
            for (int y = 0; y < fast.height(); ++y)
            {
                for (int x = 0; x < fast.width(); ++x)
                {
                    if (qAlpha(general.pixel(x, y)) == 0) continue;
                    ++total;
                    if (fast.pixel(x, y) != general.pixel(x, y))
                    {
                        ++differing;
                    }
                }
            }
            QVERIFY2(differing * 100 < total,
                     qPrintable(QStringLiteral("%1 of %2 pixels differ at longitude %3").arg(differing).arg(total).arg(longitude)));
        }
    }

    // CountryCenters ---------------------------------------------------------

    void countryCenters_knownCode_isInsideCountry()
    {
        const std::optional<GeoPoint> us = CountryCenters::find(QStringLiteral("US"));
        QVERIFY(us.has_value());
        QVERIFY(us->latitude > 25.0 && us->latitude < 50.0);
        QVERIFY(us->longitude > -125.0 && us->longitude < -65.0);
    }

    void countryCenters_protonUkCode_matchesGb()
    {
        const std::optional<GeoPoint> uk = CountryCenters::find(QStringLiteral("uk"));
        const std::optional<GeoPoint> gb = CountryCenters::find(QStringLiteral("GB"));
        QVERIFY(uk.has_value() && gb.has_value());
        QVERIFY(*uk == *gb);
    }

    void countryCenters_smallCountries_arePresent()
    {
        for (const char* code : {"MC", "SG", "AD", "LI", "MT", "MO", "HK", "XK"})
        {
            QVERIFY2(CountryCenters::find(QString::fromLatin1(code)).has_value(), code);
        }
    }

    void countryCenters_unknownCode_isEmpty()
    {
        QVERIFY(CountryCenters::find(QStringLiteral("ZZ")).has_value() == false);
        QVERIFY(CountryCenters::find(QString()).has_value() == false);
    }
};

QTEST_MAIN(TstGlobe)
#include "tst_globe.moc"
