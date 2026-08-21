#ifndef KEEPASSXC_TESTMATERIALCHANGELOG_H
#define KEEPASSXC_TESTMATERIALCHANGELOG_H
#include <QObject>
class TestMaterialChangelog : public QObject
{
    Q_OBJECT
private slots:
    void parsesAuthoritativeFieldsWithoutInventingCommits();
    void routeActionsDatesAndFiltering();
};
#endif
