#ifndef KEEPASSXC_TESTSQUIRRELLIFECYCLE_H
#define KEEPASSXC_TESTSQUIRRELLIFECYCLE_H

#include <QObject>

class TestSquirrelLifecycle : public QObject
{
    Q_OBJECT

private slots:
    void classification();
    void updatePath();
};

#endif // KEEPASSXC_TESTSQUIRRELLIFECYCLE_H
