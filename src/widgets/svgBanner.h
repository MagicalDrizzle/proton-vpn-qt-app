#pragma once

#include <QEvent>
#include <QLayout>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QWidget>
#include <tuple>
#include "../themeManager.h"
#include "pixmapCache.h"

// SVG widget that scales to fill its parent's width up to a configurable maximum,
// keeping a fixed aspect ratio (width / height, e.g. 4.0 for a 4:1 banner).
// Optionally call setLightResource() to supply an alternate SVG for light themes.
class SvgBanner : public QWidget
{
    static constexpr int DEFAULT_MAX_WIDTH = 500;

public:
    explicit SvgBanner(const QString& resource,
                       const qreal aspectRatio,
                       QWidget* parent = nullptr)
        : QWidget(parent), m_renderer(resource), m_aspect(aspectRatio)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        QWidget::setMaximumWidth(DEFAULT_MAX_WIDTH);
        m_maxWidth = DEFAULT_MAX_WIDTH;
        // sizeHint() depends on the parent's width, so the layout has to ask
        // again whenever the parent is resized; nothing else would tell it.
        if (parent != nullptr)
        {
            parent->installEventFilter(this);
        }
    }

    void setMaxWidth(const int maxWidth) // In pixels
    {
        QWidget::setMaximumWidth(maxWidth);
        m_maxWidth  = maxWidth;
        updateGeometry();
    }

    // Provide an alternate SVG resource to use when the background is light.
    // Pass an empty string to disable theme-switching.
    void setLightResource(const QString& resource)
    {
        m_lightRenderer = resource.isEmpty() ? nullptr : std::make_unique<QSvgRenderer>(resource);
        // A new renderer may reuse the old one's address, which the key holds.
        m_cache.clear();
        update();
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        const int w = qMin(availableWidth(), m_maxWidth);
        return {w, qRound(w / m_aspect)};
    }

    [[nodiscard]] int heightForWidth(int w) const override
    {
        return qRound(qMin(w, m_maxWidth) / m_aspect);
    }

    [[nodiscard]] bool hasHeightForWidth() const override { return true; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == parentWidget() && event->type() == QEvent::Resize)
        {
            updateGeometry();
        }
        return QWidget::eventFilter(watched, event);
    }

    // Width the parent can give the banner: its width less its layout's side
    // margins, so a narrow parent shrinks the banner instead of letting it run
    // into those margins.
    [[nodiscard]] int availableWidth() const
    {
        const QWidget* parent = parentWidget();
        if (parent == nullptr)
        {
            return m_maxWidth;
        }
        int w = parent->width();
        if (parent->layout() != nullptr)
        {
            const QMargins margins = parent->layout()->contentsMargins();
            w -= margins.left() + margins.right();
        }
        return w;
    }

    void paintEvent(QPaintEvent*) override
    {
        // Rendered once per size and theme and then reused: the VPN page's
        // animated globe repaints the area behind the logo on every frame, and
        // re-rendering the SVG each time was the bulk of that cost.
        const qreal dpr = devicePixelRatioF();
        QSvgRenderer* active = &activeRenderer();
        const QPixmap& banner = m_cache.get({size(), dpr, active}, [this, dpr, active]()
        {
            QPixmap pixmap = blankPixmap(QSizeF(size()), dpr);
            QPainter cachePainter(&pixmap);
            cachePainter.setRenderHint(QPainter::Antialiasing);
            active->render(&cachePainter, QRectF(QPointF(0, 0), QSizeF(size())));
            cachePainter.end();
            return pixmap;
        });
        QPainter p(this);
        p.drawPixmap(0, 0, banner);
    }

    void resizeEvent(QResizeEvent* e) override
    {
        QWidget::resizeEvent(e);
        // Lock height to the aspect-ratio-correct value for the current width
        // so the layout can never squeeze the two axes independently.
        const int correctH = qRound(width() / m_aspect);
        if (height() != correctH)
        {
            setFixedHeight(correctH);
        }
        updateGeometry();
    }

    void changeEvent(QEvent* e) override
    {
        QWidget::changeEvent(e);
        if (m_lightRenderer != nullptr && e->type() == QEvent::PaletteChange)
        {
            update();
        }
    }

private:
    // Returns the renderer appropriate for the current palette.
    QSvgRenderer& activeRenderer()
    {
        if (m_lightRenderer != nullptr)
        {
            if (ThemeManager::isDark() == false)
            {
                return *m_lightRenderer;
            }
        }
        return m_renderer;
    }


    QSvgRenderer m_renderer;
    std::unique_ptr<QSvgRenderer> m_lightRenderer;
    // By size, screen ratio and which theme's SVG it shows.
    PixmapCache<std::tuple<QSize, qreal, const QSvgRenderer*>> m_cache;
    qreal m_aspect;
    int   m_maxWidth;
};
