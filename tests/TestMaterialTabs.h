#ifndef KEEPASSXC_TESTMATERIALTABS_H
#define KEEPASSXC_TESTMATERIALTABS_H

#include <QObject>

class TestMaterialTabs : public QObject
{
    Q_OBJECT

private slots:
    void persistenceIdentity();
    void atomicReconciliation();
};

#endif
