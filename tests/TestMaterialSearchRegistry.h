#ifndef KEEPASSXC_TESTMATERIALSEARCHREGISTRY_H
#define KEEPASSXC_TESTMATERIALSEARCHREGISTRY_H

#include <QObject>

class TestMaterialSearchRegistry : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void localizedCopyPreservesBuilderOwnership();
    void registrationAndOwnership();
    void duplicateIdentityRejected();
    void existingConsumerSurfacesRegister();
    void paletteExplainsUnavailableActions();
    void storedNotificationActionsCanBeReplacedSafely();
};

#endif
