#ifndef KEEPASSXC_TESTMATERIALSEARCHREGISTRY_H
#define KEEPASSXC_TESTMATERIALSEARCHREGISTRY_H

#include <QObject>

class TestMaterialSearchRegistry : public QObject
{
    Q_OBJECT

private slots:
    void registrationAndOwnership();
    void duplicateIdentityRejected();
    void existingConsumerSurfacesRegister();
    void storedNotificationActionsCanBeReplacedSafely();
};

#endif
