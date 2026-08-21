#ifndef KEEPASSXC_TESTMATERIALREPORTS_H
#define KEEPASSXC_TESTMATERIALREPORTS_H
#include <QObject>
class TestMaterialReports : public QObject
{
    Q_OBJECT
private slots:
    void statesSelectionAndAccessibility();
    void searchRegistrationAndResponsiveLayout();
};
#endif
