#include <QtTest/QtTest>
#include "widgets/elidaLabel.h"

// The picker popups are sized from ElideLabel::fullTextWidth(), so a label
// given exactly that width must show its whole text. It used to come up a
// fraction of a pixel short (rounded down), and the last letters were cut.

namespace
{
// A size and hinting that give fractional text widths, as on a real screen;
// with whole-pixel widths the rounding this guards against never shows.
constexpr qreal FRACTIONAL_POINT_SIZE = 9.7;

void useFractionalFont(QLabel& label)
{
    QFont font = label.font();
    font.setPointSizeF(FRACTIONAL_POINT_SIZE);
    font.setHintingPreference(QFont::PreferNoHinting);
    label.setFont(font);
}
} // namespace

class TstElideLabel : public QObject
{
    Q_OBJECT

private slots:
    void fullTextWidth_labelThatWide_showsWholeText()
    {
        for (const char* text : {"United States, Secaucus, New Jersey", "Fastest in Bosnia and Herzegovina",
                                 "United States, Ashburn, Virginia", "Iiiiiiiiiiiiil, WWWWWWWW",
                                 "Saint Vincent and the Grenadines", "Democratic Republic of the Congo"})
        {
            ElideLabel label(QString::fromUtf8(text));
            useFractionalFont(label);
            label.show(); // a hidden widget gets no resize events, so it would never elide
            label.resize(label.fullTextWidth(), label.sizeHint().height());
            QCOMPARE(label.text(), label.fullText());
        }
    }

    void fullTextWidth_narrowerLabel_elides()
    {
        ElideLabel label(QStringLiteral("United States, Secaucus, New Jersey"));
        label.show();
        label.resize(label.fullTextWidth() / 2, label.sizeHint().height());
        QVERIFY(label.text() != label.fullText());
    }
};

QTEST_MAIN(TstElideLabel)
#include "tst_elideLabel.moc"
