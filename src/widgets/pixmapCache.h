#pragma once

// pixmapCache.h
// A pixmap kept until what it was drawn from changes. The widgets over the
// VPN page's animated globe repaint with every globe frame, and redrawing an
// SVG or a gradient each time was most of that cost.
//
// The cache is keyed on the inputs (size, device pixel ratio, theme, ...),
// never on values read back from the pixmap: a helper may tag a pixmap with a
// different ratio than the one asked for (GeoUtils::svgPixmap() uses the
// application's), and a cache comparing against that rebuilds on every paint.

#include <QPixmap>
#include <QSizeF>
#include <utility>

// A transparent pixmap of `size` device-independent pixels, sharp at `dpr`.
inline QPixmap blankPixmap(const QSizeF& size, const qreal dpr)
{
    QPixmap pixmap((size * dpr).toSize());
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

template <typename Key>
class PixmapCache
{
public:
    // The pixmap for `key`, drawn by `draw()` only when `key` differs from
    // the one the cached pixmap was drawn for.
    template <typename Draw>
    const QPixmap& get(const Key& key, Draw&& draw)
    {
        if (m_valid == false || m_key != key)
        {
            m_pixmap = std::forward<Draw>(draw)();
            m_key = key;
            m_valid = true;
        }
        return m_pixmap;
    }

    // Forgets the pixmap, for a change the key does not capture.
    void clear()
    {
        m_pixmap = QPixmap();
        m_valid = false;
    }

private:
    Key m_key{};
    QPixmap m_pixmap;
    bool m_valid = false;
};
