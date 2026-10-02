#include "cliNotRespondingPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSvgWidget>
#include <QVBoxLayout>

namespace
{
// Caps the text column at a comfortable reading width; the side margins keep
// it off the window edges when the window is at its minimum width.
constexpr int CLI_NOT_RESPONDING_CONTENT_MAX_WIDTH = 440;
constexpr int CLI_NOT_RESPONDING_H_MARGIN          = 32;
constexpr int CLI_NOT_RESPONDING_ICON_SIZE         = 96;
constexpr int CLI_NOT_RESPONDING_ICON_SPACING      = 20;
constexpr int CLI_NOT_RESPONDING_TITLE_SPACING     = 12;
constexpr int CLI_NOT_RESPONDING_PARAGRAPH_SPACING = 10;
constexpr int CLI_NOT_RESPONDING_BUTTON_SPACING    = 24;
} // namespace

CliNotRespondingPage::CliNotRespondingPage(QWidget* parent)
    : QWidget(parent)
{
    // The content column fills the available width up to its cap and stays
    // centered. Centering the labels themselves would shrink each word-wrapped
    // label to its narrowest size hint instead.
    QHBoxLayout* outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(CLI_NOT_RESPONDING_H_MARGIN, 0, CLI_NOT_RESPONDING_H_MARGIN, 0);

    QWidget* content = new QWidget(this);
    content->setMaximumWidth(CLI_NOT_RESPONDING_CONTENT_MAX_WIDTH);
    outerLayout->addStretch();
    outerLayout->addWidget(content, 1);
    outerLayout->addStretch();

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addStretch();

    QSvgWidget* icon = new QSvgWidget(QStringLiteral(":/assets/maintenance-icon.svg"), content);
    icon->setFixedSize(CLI_NOT_RESPONDING_ICON_SIZE, CLI_NOT_RESPONDING_ICON_SIZE);
    layout->addWidget(icon, 0, Qt::AlignHCenter);

    layout->addSpacing(CLI_NOT_RESPONDING_ICON_SPACING);

    QLabel* titleLabel = new QLabel(tr("ProtonVPN CLI Not Responding"), content);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    layout->addSpacing(CLI_NOT_RESPONDING_TITLE_SPACING);

    // What happened...
    QLabel* summaryLabel = new QLabel(
        tr("The <b>protonvpn</b> command-line tool stopped responding while "
           "checking whether you are signed in."),
        content);
    summaryLabel->setAlignment(Qt::AlignCenter);
    summaryLabel->setWordWrap(true);
    layout->addWidget(summaryLabel);

    layout->addSpacing(CLI_NOT_RESPONDING_PARAGRAPH_SPACING);

    // ...and what to do about it.
    QLabel* hintLabel = new QLabel(
        tr("This usually means it is waiting for your keyring or wallet to be "
           "unlocked. Unlock it (the prompt may be behind other windows), then "
           "try again."),
        content);
    hintLabel->setObjectName(QStringLiteral("fieldLabel"));
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    layout->addSpacing(CLI_NOT_RESPONDING_BUTTON_SPACING);

    QPushButton* retryBtn = new QPushButton(tr("Try Again"), content);
    retryBtn->setObjectName(QStringLiteral("primaryButton"));
    retryBtn->setCursor(Qt::PointingHandCursor);
    connect(retryBtn, &QPushButton::clicked, this, &CliNotRespondingPage::retryRequested);
    layout->addWidget(retryBtn, 0, Qt::AlignHCenter);

    layout->addStretch();
}
