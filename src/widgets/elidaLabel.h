#pragma once

#include <QFontMetricsF>
#include <QLabel>
#include <QResizeEvent>
#include <cmath>
#include <utility>

// ElideLabel is a QLabel that elides its text with "…" at the
//  right edge whenever it is too narrow to show the text in full.
//
// NOTE: setText() *hides* QLabel::setText() rather than overriding it - QLabel
// does not declare it virtual. Always hold an ElideLabel by its own type; a
// call made through a QLabel* would bypass the eliding and store the raw text.

class ElideLabel : public QLabel
{
public:
    explicit ElideLabel(QString  text, QWidget* parent = nullptr)
        : QLabel(parent), m_fullText(std::move(text))
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setMinimumWidth(0);
        applyText();
    }

    // Hides QLabel::setText so callers can use it normally - see the note above.
    void setText(const QString& text)
    {
        m_fullText = text;
        applyText();
    }

    // The unelided text. text() returns what is actually painted, which may
    // have been shortened.
    [[nodiscard]] QString fullText() const { return m_fullText; }

    // The width at which the full text shows without eliding. sizeHint() is
    // no help: it measures the text as currently shown, and layouts ignore it
    // anyway (the horizontal size policy is Ignored, so the label can shrink).
    [[nodiscard]] int fullTextWidth() const
    {
        ensurePolished(); // the stylesheet's font, not the default one
        const QMargins m = contentsMargins();
        // Rounded up from the fractional width: elidedText() compares that
        // against the label's width, and a whole-pixel width rounded down
        // would still cut off the last letters.
        const int textW = static_cast<int>(std::ceil(QFontMetricsF(font()).horizontalAdvance(m_fullText)));
        return textW + m.left() + m.right() + 2 * margin();
    }

protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QLabel::resizeEvent(e);
        applyText();
    }

private:
    // Width used before the label has been laid out and has a real width.
    static constexpr int UNCONSTRAINED_WIDTH = 9999;

    void applyText()
    {
        const QString shown = elided();
        QLabel::setText(shown);
        // Only worth a tooltip when something was actually cut off; an
        // unconditional tooltip would shadow tooltips set by the parent row.
        setToolTip(shown == m_fullText ? QString() : m_fullText);
    }

    [[nodiscard]] QString elided() const
    {
        return fontMetrics().elidedText(m_fullText, Qt::ElideRight,
                                        width() > 0 ? width() : UNCONSTRAINED_WIDTH);
    }

    QString m_fullText;
};
