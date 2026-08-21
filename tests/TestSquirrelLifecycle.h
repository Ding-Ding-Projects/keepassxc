#ifndef KEEPASSXC_TESTSQUIRRELLIFECYCLE_H
#define KEEPASSXC_TESTSQUIRRELLIFECYCLE_H

#include <QObject>

class TestSquirrelLifecycle : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void classification();
    void firstRunConsumption();
    void layoutValidation();
    void processResultContract();
    void shortHelperEvidence();
    void handleUsesExactOwnedSeams();
};

#endif // KEEPASSXC_TESTSQUIRRELLIFECYCLE_H
