#pragma once

#include <QLabel>
#include <QResizeEvent>
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
