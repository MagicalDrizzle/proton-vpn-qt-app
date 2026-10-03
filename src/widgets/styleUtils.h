#pragma once
// styleUtils.h
// Helper for driving QSS attribute selectors from code.

#include <QStyle>
#include <QVariant>
#include <QWidget>

// Sets a dynamic property that a QSS rule selects on (e.g. [vpnState="error"])
// and re-polishes the widget so the new rule takes effect immediately.
//
// Qt does not restyle a widget when a property changes, which is why every
// call site has to unpolish/polish - collecting that here keeps the callers
// from open-coding it (or forgetting it and wondering why nothing changed).
inline void setStyleProperty(QWidget* widget, const char* name, const QVariant& value)
{
    if (widget == nullptr) return;

    widget->setProperty(name, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
