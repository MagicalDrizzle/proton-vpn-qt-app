#include <QtTest/QtTest>
#include <QStandardPaths>
#include "favoritesManager.h"

// FavoritesManager is a pure data store over a JSON file: add/remove/toggle
// with case-insensitive matching, plus persistence. All of it is testable
// without any UI or subprocess.

class TstFavoritesManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        FavoritesManager::instance().clear();
    }

    void cleanupTestCase()
    {
        FavoritesManager::instance().clear();
        QStandardPaths::setTestModeEnabled(false);
    }

    void cleanup()
    {
        FavoritesManager::instance().clear();
    }

    void add_newEntry_isStored()
    {
        FavoritesManager::instance().add(QStringLiteral("US"),
                                         QStringLiteral("United States"),
                                         QStringLiteral("Secaucus"));

        QCOMPARE(FavoritesManager::instance().entries().size(), 1);
        QVERIFY(FavoritesManager::instance().isFavorite(QStringLiteral("US"),
                                                        QStringLiteral("Secaucus")));
    }

    void add_duplicateEntry_isIgnored()
    {
        FavoritesManager::instance().add(QStringLiteral("US"), QStringLiteral("United States"),
                                         QStringLiteral("Secaucus"));
        FavoritesManager::instance().add(QStringLiteral("US"), QStringLiteral("United States"),
                                         QStringLiteral("Secaucus"));

        QCOMPARE(FavoritesManager::instance().entries().size(), 1);
    }

    void isFavorite_differingCase_matches()
    {
        FavoritesManager::instance().add(QStringLiteral("US"), QStringLiteral("United States"),
                                         QStringLiteral("Secaucus"));

        QVERIFY(FavoritesManager::instance().isFavorite(QStringLiteral("us"),
                                                        QStringLiteral("secaucus")));
    }

    void isFavorite_emptyCity_isSeparateFromCityEntry()
    {
        // An empty city means "fastest server in this country" and must not be
        // confused with a specific city in the same country.
        FavoritesManager::instance().add(QStringLiteral("US"), QStringLiteral("United States"),
                                         QString());

        QVERIFY(FavoritesManager::instance().isFavorite(QStringLiteral("US"), QString()));
        QVERIFY(FavoritesManager::instance().isFavorite(QStringLiteral("US"),
                                                        QStringLiteral("Secaucus")) == false);
    }

    void remove_existingEntry_isGone()
    {
        FavoritesManager::instance().add(QStringLiteral("DE"), QStringLiteral("Germany"),
                                         QStringLiteral("Frankfurt"));
        FavoritesManager::instance().remove(QStringLiteral("DE"), QStringLiteral("Frankfurt"));

        QVERIFY(FavoritesManager::instance().entries().isEmpty());
    }

    void remove_missingEntry_isNoOp()
    {
        FavoritesManager::instance().add(QStringLiteral("DE"), QStringLiteral("Germany"),
                                         QStringLiteral("Frankfurt"));
        FavoritesManager::instance().remove(QStringLiteral("CH"), QStringLiteral("Zurich"));

        QCOMPARE(FavoritesManager::instance().entries().size(), 1);
    }

    void toggle_addsThenRemoves()
    {
        FavoritesManager::instance().toggle(QStringLiteral("JP"), QStringLiteral("Japan"),
                                            QStringLiteral("Tokyo"));
        QVERIFY(FavoritesManager::instance().isFavorite(QStringLiteral("JP"),
                                                        QStringLiteral("Tokyo")));

        FavoritesManager::instance().toggle(QStringLiteral("JP"), QStringLiteral("Japan"),
                                            QStringLiteral("Tokyo"));
        QVERIFY(FavoritesManager::instance().isFavorite(QStringLiteral("JP"),
                                                        QStringLiteral("Tokyo")) == false);
    }

    void changed_isEmittedOnMutationOnly()
    {
        QSignalSpy spy(&FavoritesManager::instance(), &FavoritesManager::changed);

        FavoritesManager::instance().add(QStringLiteral("FR"), QStringLiteral("France"),
                                         QStringLiteral("Paris"));
        QCOMPARE(spy.count(), 1);

        // Adding the same entry again changes nothing, so no signal.
        FavoritesManager::instance().add(QStringLiteral("FR"), QStringLiteral("France"),
                                         QStringLiteral("Paris"));
        QCOMPARE(spy.count(), 1);

        FavoritesManager::instance().remove(QStringLiteral("FR"), QStringLiteral("Paris"));
        QCOMPARE(spy.count(), 2);
    }

    void clear_emptyStore_doesNotEmit()
    {
        QSignalSpy spy(&FavoritesManager::instance(), &FavoritesManager::changed);
        FavoritesManager::instance().clear();
        QCOMPARE(spy.count(), 0);
    }

    void hasAnyEntries_reflectsContent()
    {
        QVERIFY(FavoritesManager::instance().hasAnyEntries() == false);
        FavoritesManager::instance().add(QStringLiteral("SE"), QStringLiteral("Sweden"),
                                         QStringLiteral("Stockholm"));
        QVERIFY(FavoritesManager::instance().hasAnyEntries());
    }

    void entries_preserveInsertionOrder()
    {
        FavoritesManager::instance().add(QStringLiteral("US"), QStringLiteral("United States"), QString());
        FavoritesManager::instance().add(QStringLiteral("DE"), QStringLiteral("Germany"), QString());
        FavoritesManager::instance().add(QStringLiteral("JP"), QStringLiteral("Japan"), QString());

        const QList<FavoriteEntry> entries = FavoritesManager::instance().entries();
        QCOMPARE(entries.size(), 3);
        QCOMPARE(entries.at(0).countryCode, QStringLiteral("US"));
        QCOMPARE(entries.at(1).countryCode, QStringLiteral("DE"));
        QCOMPARE(entries.at(2).countryCode, QStringLiteral("JP"));
    }
};

QTEST_MAIN(TstFavoritesManager)
#include "tst_favoritesManager.moc"
