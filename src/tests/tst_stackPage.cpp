#include <QtTest/QtTest>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include "widgets/stackPage.h"

// StackPage keeps a QStackedWidget sized to the page it shows, so the
// sign-in views are not stretched to the tallest one's height (which pulled
// their labels apart).

namespace
{
constexpr int PAGE_WIDTH        = 300;
constexpr int TALL_PAGE_BUTTONS = 10;
constexpr int WINDOW_HEIGHT     = 1000;

// The tallest page: many buttons.
void fillTall(QWidget* page)
{
    QVBoxLayout* layout = new QVBoxLayout(page);
    for (int i = 0; i < TALL_PAGE_BUTTONS; ++i)
    {
        layout->addWidget(new QPushButton(QStringLiteral("Button"), page));
    }
}

// A short page whose word-wrapped label gives it a height for a width, as
// the security key view's status line does.
void fillShort(QWidget* page)
{
    QVBoxLayout* layout = new QVBoxLayout(page);
    QLabel* label = new QLabel(QStringLiteral("Multiple security keys were found. Tap the one you want to use."), page);
    label->setWordWrap(true);
    layout->addWidget(label);
    layout->addWidget(new QPushButton(QStringLiteral("Go Back"), page));
}

// A window centering a stack of a tall and a short page, as the login card
// is centered.
struct Window
{
    QWidget window;
    QStackedWidget* stack = nullptr;
    QWidget* tall = nullptr;
    QWidget* shortPage = nullptr;

    template<typename Page>
    void build()
    {
        QVBoxLayout* layout = new QVBoxLayout(&window);
        stack = new QStackedWidget(&window);
        stack->setFixedWidth(PAGE_WIDTH);
        tall = new Page();
        shortPage = new Page();
        fillTall(tall);
        fillShort(shortPage);
        stack->addWidget(tall);
        stack->addWidget(shortPage);
        layout->addWidget(stack, 0, Qt::AlignCenter);
        window.resize(PAGE_WIDTH, WINDOW_HEIGHT);
        window.show();
    }

    // The stack's height once laid out with `page` on screen.
    int heightShowing(QWidget* page) const
    {
        stack->setCurrentWidget(page);
        window.layout()->activate();
        return stack->height();
    }
};
} // namespace

class TstStackPage : public QObject
{
    Q_OBJECT

private slots:
    void hiddenPage_reportsNoSize()
    {
        StackPage page;
        fillTall(&page);
        page.hide();
        QCOMPARE(page.sizeHint(), QSize());
        QCOMPARE(page.minimumSizeHint(), QSize());
        QCOMPARE(page.heightForWidth(PAGE_WIDTH), -1);
    }

    void shortPage_keepsItsOwnHeight()
    {
        Window w;
        w.build<StackPage>();
        const int height = w.heightShowing(w.shortPage);
        QCOMPARE(height, w.shortPage->heightForWidth(PAGE_WIDTH));
    }

    void tallPage_keepsItsOwnHeight()
    {
        Window w;
        w.build<StackPage>();
        const int height = w.heightShowing(w.tall);
        QCOMPARE(height, w.tall->sizeHint().height());
    }

    void switchingBack_shrinksAgain()
    {
        Window w;
        w.build<StackPage>();
        const int shortHeight = w.heightShowing(w.shortPage);
        QVERIFY(w.heightShowing(w.tall) > shortHeight);
        QCOMPARE(w.heightShowing(w.shortPage), shortHeight);
    }

    void plainPages_stretchShortPage()
    {
        // What StackPage prevents: with plain pages the short one is laid
        // out at the tall one's height.
        Window w;
        w.build<QWidget>();
        const int tallHeight = w.tall->sizeHint().height();
        QVERIFY(w.heightShowing(w.shortPage) >= tallHeight);
    }
};

QTEST_MAIN(TstStackPage)
#include "tst_stackPage.moc"
