#ifndef KEEPASSXC_TESTMATERIALSHELLRESPONSIVE_H
#define KEEPASSXC_TESTMATERIALSHELLRESPONSIVE_H

#include <QObject>

class TestMaterialShellResponsive : public QObject
{
    Q_OBJECT

private slots:
    void preservesDestinationAccessAcrossBreakpoints();
    void emitsOnlyOnBreakpointTransitions();
    void appliesVaultPaneContract();
};

#endif // KEEPASSXC_TESTMATERIALSHELLRESPONSIVE_H
