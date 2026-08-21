#ifndef KEEPASSXC_TESTMATERIALREGEXSAFETY_H
#define KEEPASSXC_TESTMATERIALREGEXSAFETY_H

#include <QObject>

class TestMaterialRegexSafety : public QObject
{
    Q_OBJECT

private slots:
    void validAndInvalidPatterns();
    void boundsAndRiskShapes();
    void zeroWidthAndMatchLimit();
};

#endif
