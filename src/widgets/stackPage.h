#pragma once

#include <QWidget>

// StackPage is a QStackedWidget page that takes up no room while another
// page is shown.
//
// A QStackedWidget is as large as its largest page, so a shorter page is
// stretched to fill it and its labels drift apart. QStackedLayout asks every
// page for its sizes and its height for a width, shown or not, and nothing
// on the stack itself can change that (layouts measure a widget with a
// layout through that layout). It hides the pages it does not show, so a
// StackPage answers only while it is the one on screen.

class StackPage : public QWidget
{
public:
    using QWidget::QWidget;

    [[nodiscard]] QSize sizeHint() const override
    {
        return isHidden() ? QSize() : QWidget::sizeHint();
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return isHidden() ? QSize() : QWidget::minimumSizeHint();
    }

    [[nodiscard]] int heightForWidth(const int width) const override
    {
        return isHidden() ? -1 : QWidget::heightForWidth(width);
    }
};
