#include "globeRenderer.h"
#include "globeMath.h"

#include <QMatrix3x3>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
// Texture columns are tracked in 8-bit fixed point so the spin offset can be
// added without losing sub-texel precision.
constexpr int FIXED_SHIFT = 8;
constexpr int FIXED_ONE   = 1 << FIXED_SHIFT;

// Lighting: a soft key light from the upper left, in view space. How much it
// matters against ambient light is set per theme through setLighting().
constexpr float LIGHT_X        = -0.40f;
constexpr float LIGHT_Y        = 0.45f;
constexpr float LIGHT_Z        = 0.80f;

// Offset from a pixel's corner to its center.
constexpr float PIXEL_CENTER = 0.5f;

// Mask texels at or above this gray level count as land.
constexpr int LAND_THRESHOLD = 128;

// Premultiplied ARGB32 packing.
constexpr float MAX_CHANNEL = 255.0f;
constexpr int   ALPHA_SHIFT = 24;
constexpr int   RED_SHIFT   = 16;
constexpr int   GREEN_SHIFT = 8;


std::uint32_t premultiplied(const QColor& c, const float shade, const float coverage)
{
    const float alpha = static_cast<float>(c.alphaF()) * coverage;
    const auto channel = [&](const float value)
    {
        return static_cast<std::uint32_t>(std::lround(std::clamp(value * shade, 0.0f, 1.0f) * alpha * MAX_CHANNEL));
    };
    return (static_cast<std::uint32_t>(std::lround(alpha * MAX_CHANNEL)) << ALPHA_SHIFT)
         | (channel(static_cast<float>(c.redF())) << RED_SHIFT)
         | (channel(static_cast<float>(c.greenF())) << GREEN_SHIFT)
         | channel(static_cast<float>(c.blueF()));
}
} // namespace

void GlobeRenderer::setLandMask(const QImage& mask)
{
    const QImage gray = mask.convertToFormat(QImage::Format_Grayscale8);
    m_maskWidth  = gray.width();
    m_maskHeight = gray.height();
    m_landMask.assign(static_cast<std::size_t>(m_maskWidth) * m_maskHeight, 0);
    for (int y = 0; y < m_maskHeight; ++y)
    {
        const uchar* line = gray.constScanLine(y);
        for (int x = 0; x < m_maskWidth; ++x)
        {
            m_landMask[static_cast<std::size_t>(y) * m_maskWidth + x] = (line[x] >= LAND_THRESHOLD) ? 1 : 0;
        }
    }
    m_columnsPerRadian = static_cast<float>(m_maskWidth / (2.0 * std::numbers::pi));
    m_rowsPerRadian    = static_cast<float>(m_maskHeight / std::numbers::pi);
    m_spinValid = false;
}

void GlobeRenderer::setColors(const QColor& water, const QColor& land)
{
    if (water == m_waterFill && land == m_landFill) return;
    m_waterFill = water;
    m_landFill  = land;
    rebuildColors();
}

void GlobeRenderer::setLighting(const float ambient, const float diffuse)
{
    if (ambient == m_ambient && diffuse == m_diffuse) return;
    m_ambient = ambient;
    m_diffuse = diffuse;
    rebuildColors();
}

void GlobeRenderer::setSize(const int size)
{
    if (size == m_size) return;
    m_size = size;
    rebuildDisc();
}

void GlobeRenderer::rebuildDisc()
{
    m_pixelOffset.clear();
    m_viewX.clear();
    m_viewY.clear();
    m_viewZ.clear();
    m_diffuseTerm.clear();
    m_coverage.clear();
    m_spinValid = false;

    m_image = QImage(m_size, m_size, QImage::Format_ARGB32_Premultiplied);
    m_image.fill(Qt::transparent);
    if (m_size <= 0) return;

    const float radius = m_size / 2.0f;
    const float lightLength = std::sqrt(LIGHT_X * LIGHT_X + LIGHT_Y * LIGHT_Y + LIGHT_Z * LIGHT_Z);
    for (int py = 0; py < m_size; ++py)
    {
        for (int px = 0; px < m_size; ++px)
        {
            const float x = (px + PIXEL_CENTER - radius) / radius;
            const float y = (radius - (py + PIXEL_CENTER)) / radius;
            const float r = std::sqrt(x * x + y * y);
            // Coverage falls from 1 to 0 across the one-pixel band at the edge.
            const float coverage = std::clamp((1.0f - r) * radius + PIXEL_CENTER, 0.0f, 1.0f);
            if (coverage <= 0.0f) continue;

            // Pixels in the edge band just outside the sphere use the point on
            // its rim, so the antialiased fringe has the right color.
            const float scale = (r > 1.0f) ? 1.0f / r : 1.0f;
            const float vx = x * scale;
            const float vy = y * scale;
            const float vz = std::sqrt(std::max(0.0f, 1.0f - vx * vx - vy * vy));
            const float diffuse = std::max(0.0f, (vx * LIGHT_X + vy * LIGHT_Y + vz * LIGHT_Z) / lightLength);

            m_pixelOffset.push_back(py * m_size + px);
            m_viewX.push_back(vx);
            m_viewY.push_back(vy);
            m_viewZ.push_back(vz);
            m_diffuseTerm.push_back(diffuse);
            m_coverage.push_back(coverage);
        }
    }
    rebuildColors();
}

void GlobeRenderer::rebuildColors()
{
    const std::size_t count = m_pixelOffset.size();
    m_waterColor.resize(count);
    m_landColor.resize(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const float shade = m_ambient + m_diffuse * m_diffuseTerm[i];
        m_waterColor[i] = premultiplied(m_waterFill, shade, m_coverage[i]);
        m_landColor[i]  = premultiplied(m_landFill,  shade, m_coverage[i]);
    }
}

bool GlobeRenderer::hasFrameToDraw() const
{
    return m_landMask.empty() == false && m_pixelOffset.empty() == false;
}

GlobeRenderer::MaskPosition GlobeRenderer::maskPosition(const float wx, const float wy, const float wz) const
{
    // Longitude runs left to right from the mask's left edge at -180;
    // latitude top to bottom from +90.
    const float lon = GlobeMath::fastAtan2(wx, wz);
    const float lat = GlobeMath::fastAtan2(wy, std::sqrt(wx * wx + wz * wz));
    const int row = std::clamp(static_cast<int>(m_maskHeight / 2.0f - lat * m_rowsPerRadian), 0, m_maskHeight - 1);
    const float column = m_maskWidth / 2.0f + lon * m_columnsPerRadian;
    return {row, static_cast<std::int32_t>(column * FIXED_ONE)};
}

bool GlobeRenderer::isLandAt(const std::size_t maskIndex) const
{
    return m_landMask[maskIndex] != 0;
}

const QImage& GlobeRenderer::render(const QQuaternion& orientation)
{
    if (hasFrameToDraw() == false)
    {
        return m_image;
    }

    // World direction of each pixel: the inverse orientation applied to its
    // view direction.
    const QMatrix3x3 m = orientation.conjugated().toRotationMatrix();
    const float m00 = m(0, 0), m01 = m(0, 1), m02 = m(0, 2);
    const float m10 = m(1, 0), m11 = m(1, 1), m12 = m(1, 2);
    const float m20 = m(2, 0), m21 = m(2, 1), m22 = m(2, 2);
    const std::int32_t widthMask = m_maskWidth - 1;

    std::uint32_t* pixels = reinterpret_cast<std::uint32_t*>(m_image.bits());
    const std::size_t count = m_pixelOffset.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        const float vx = m_viewX[i], vy = m_viewY[i], vz = m_viewZ[i];
        const MaskPosition pos = maskPosition(m00 * vx + m01 * vy + m02 * vz,
                                              m10 * vx + m11 * vy + m12 * vz,
                                              m20 * vx + m21 * vy + m22 * vz);
        const std::int32_t column = (pos.column >> FIXED_SHIFT) & widthMask;
        const std::size_t maskIndex = static_cast<std::size_t>(pos.row) * m_maskWidth + column;
        pixels[m_pixelOffset[i]] = isLandAt(maskIndex) ? m_landColor[i] : m_waterColor[i];
    }
    return m_image;
}

void GlobeRenderer::prepareSpin(const QQuaternion& frame)
{
    if (m_spinValid == true && frame == m_spinFrame) return;
    m_spinFrame = frame;
    m_spinValid = true;

    const QQuaternion inverse = frame.conjugated();
    const std::size_t count = m_pixelOffset.size();
    m_spinRowOffset.resize(count);
    m_spinColumn.resize(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const QVector3D world = inverse.rotatedVector(QVector3D(m_viewX[i], m_viewY[i], m_viewZ[i]));
        const MaskPosition pos = maskPosition(world.x(), world.y(), world.z());
        m_spinRowOffset[i] = pos.row * m_maskWidth;
        m_spinColumn[i] = pos.column;
    }
}

const QImage& GlobeRenderer::renderSpin(const QQuaternion& frame, const double centerLongitude)
{
    if (hasFrameToDraw() == false)
    {
        return m_image;
    }
    prepareSpin(frame);

    // Spinning by centerLongitude turns every pixel's longitude by the same
    // amount: one shared offset added to each pixel's precomputed column.
    const double turns = centerLongitude / GlobeMath::DEGREES_PER_TURN;
    const std::int32_t spinOffset = static_cast<std::int32_t>(std::lround(
        (turns - std::floor(turns)) * m_maskWidth * FIXED_ONE));
    const std::int32_t widthMask = m_maskWidth - 1;

    std::uint32_t* pixels = reinterpret_cast<std::uint32_t*>(m_image.bits());
    const std::size_t count = m_pixelOffset.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::int32_t column = ((m_spinColumn[i] + spinOffset) >> FIXED_SHIFT) & widthMask;
        pixels[m_pixelOffset[i]] = isLandAt(static_cast<std::size_t>(m_spinRowOffset[i] + column))
            ? m_landColor[i] : m_waterColor[i];
    }
    return m_image;
}
