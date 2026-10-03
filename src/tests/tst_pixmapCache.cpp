#include <QtTest/QtTest>
#include <tuple>
#include "widgets/pixmapCache.h"

// PixmapCache keeps the globe's repaints cheap only if it redraws exactly
// when its key changes: too often and the savings are gone, too rarely and
// a stale icon or theme stays on screen.

namespace
{
using Key = std::tuple<int, qreal, bool>;
} // namespace

class TstPixmapCache : public QObject
{
    Q_OBJECT

private slots:
    void get_sameKey_drawsOnce()
    {
        PixmapCache<Key> cache;
        int draws = 0;
        const auto draw = [&draws]()
        {
            ++draws;
            return blankPixmap(QSizeF(4, 4), 1.0);
        };
        (void)cache.get({4, 1.0, true}, draw);
        (void)cache.get({4, 1.0, true}, draw);
        QCOMPARE(draws, 1);
    }

    void get_anyKeyPartChanges_redraws()
    {
        PixmapCache<Key> cache;
        int draws = 0;
        const auto draw = [&draws]()
        {
            ++draws;
            return blankPixmap(QSizeF(4, 4), 1.0);
        };
        (void)cache.get({4, 1.0, true}, draw);
        (void)cache.get({5, 1.0, true}, draw);   // size
        (void)cache.get({5, 2.0, true}, draw);   // screen ratio
        (void)cache.get({5, 2.0, false}, draw);  // theme
        QCOMPARE(draws, 4);
    }

    void clear_sameKey_redraws()
    {
        PixmapCache<Key> cache;
        int draws = 0;
        const auto draw = [&draws]()
        {
            ++draws;
            return blankPixmap(QSizeF(4, 4), 1.0);
        };
        (void)cache.get({4, 1.0, true}, draw);
        cache.clear();
        (void)cache.get({4, 1.0, true}, draw);
        QCOMPARE(draws, 2);
    }

    void blankPixmap_hiDpi_isSharpAndTransparent()
    {
        const QPixmap pixmap = blankPixmap(QSizeF(10, 6), 2.0);
        QCOMPARE(pixmap.size(), QSize(20, 12));
        QCOMPARE(pixmap.deviceIndependentSize(), QSizeF(10, 6));
        QCOMPARE(pixmap.toImage().pixelColor(0, 0).alpha(), 0);
    }
};

QTEST_MAIN(TstPixmapCache)
#include "tst_pixmapCache.moc"
