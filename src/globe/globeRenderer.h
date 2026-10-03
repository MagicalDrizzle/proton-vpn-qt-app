#pragma once

// globeRenderer.h
// Software renderer for the VPN page's background globe: an orthographic view
// of the unit sphere, colored from an equirectangular land mask, with simple
// lighting and an antialiased edge.
//
// No OpenGL: the globe has to work the same in the native build, the AppImage
// and the Flatpak, including on machines and VMs without GPU acceleration.
// Two paths keep that cheap:
//   - renderSpin(): the orientation is a fixed frame followed by a spin about
//     the polar axis. Spinning only shifts longitude, so after a one-off pass
//     per frame each pixel costs a table lookup. Idle rotation and holding on
//     a country both use it.
//   - render(): any orientation, at a few multiplications and two fast atan2
//     calls per pixel. Only used while turning between orientations.

#include <QColor>
#include <QImage>
#include <QQuaternion>
#include <cstdint>
#include <vector>

class GlobeRenderer
{
public:
    // Land mask: equirectangular, longitude -180..180 left to right, latitude
    // 90..-90 top to bottom; any non-black pixel is land. Its width must be a
    // power of two (longitudes wrap with a bit mask).
    void setLandMask(const QImage& mask);

    // Colors may carry alpha; the edge of the disc is antialiased on top.
    void setColors(const QColor& water, const QColor& land);

    // Lighting: each pixel's color is scaled by ambient + diffuse * (how
    // squarely it faces the light), so ambient + diffuse = 1 at its brightest.
    // Strong shading reads well on a dark page but muddies pale colors.
    void setLighting(float ambient, float diffuse);

    // Width and height of the square image, in pixels.
    void setSize(int size);
    [[nodiscard]] int size() const { return m_size; }

    [[nodiscard]] const QImage& render(const QQuaternion& orientation);
    // The most recently rendered frame.
    [[nodiscard]] const QImage& image() const { return m_image; }
    [[nodiscard]] const QImage& renderSpin(const QQuaternion& frame, double centerLongitude);

    // Lighting until setLighting is called: shading strong enough to read as
    // a sphere on a dark page.
    static constexpr float DEFAULT_AMBIENT = 0.62f;
    static constexpr float DEFAULT_DIFFUSE = 0.38f;

private:
    // Where a world direction falls on the land mask: its row, and its
    // column in fixed point (FIXED_ONE steps per column, unwrapped) so the
    // spin path can shift it by part of a column. Both render paths use it.
    struct MaskPosition
    {
        int row;
        std::int32_t column;
    };

    void rebuildDisc();
    void rebuildColors();
    void prepareSpin(const QQuaternion& frame);
    [[nodiscard]] bool hasFrameToDraw() const;
    [[nodiscard]] MaskPosition maskPosition(float wx, float wy, float wz) const;
    [[nodiscard]] bool isLandAt(std::size_t maskIndex) const;

    // Land mask, one byte per texel for fast lookups.
    std::vector<std::uint8_t> m_landMask;
    int m_maskWidth  = 0;
    int m_maskHeight = 0;
    float m_columnsPerRadian = 0.0f;
    float m_rowsPerRadian    = 0.0f;

    QColor m_waterFill;
    QColor m_landFill;
    float m_ambient = DEFAULT_AMBIENT;
    float m_diffuse = DEFAULT_DIFFUSE;
    int m_size = 0;
    QImage m_image;

    // One entry per pixel the disc covers, including its antialiased edge.
    std::vector<std::int32_t>  m_pixelOffset; // index into the image
    std::vector<float>         m_viewX;       // view-space direction of the pixel
    std::vector<float>         m_viewY;
    std::vector<float>         m_viewZ;
    std::vector<float>         m_diffuseTerm; // 0..1: how squarely the pixel faces the light
    std::vector<float>         m_coverage;    // 0..1, below 1 only along the edge
    std::vector<std::uint32_t> m_waterColor;  // premultiplied ARGB with shade/coverage
    std::vector<std::uint32_t> m_landColor;

    // renderSpin() tables, valid for m_spinFrame.
    QQuaternion m_spinFrame;
    bool m_spinValid = false;
    std::vector<std::int32_t> m_spinRowOffset;   // mask row * mask width
    std::vector<std::int32_t> m_spinColumn;      // MaskPosition::column
};
