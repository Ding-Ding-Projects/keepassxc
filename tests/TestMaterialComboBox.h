#ifndef KEEPASSXC_TESTMATERIALCOMBOBOX_H
#define KEEPASSXC_TESTMATERIALCOMBOBOX_H

#include <QObject>

class TestMaterialComboBox : public QObject
{
    Q_OBJECT
private slots:
    void localSearchPreservesSharedModel();
    void duplicateLabelsActivateOriginalIndex();
    void nonzeroRootAndColumn();
    void mutationsFollowOriginalIndices();
    void replacementThroughBasePointerCancels();
    void sourceDestructionCancels();
    void disabledRowsAndSameItemActivation();
    void editableFieldAndInsertionRemainNative();
    void invalidRegexFailsClosed();
    void flagsOnlyChangeAndUnicode();
    void limitsFailClosed();
    void builderStaysLocalAndHasNoChoiceSamples();
    void cancellationDoesNotCommit();
    void hideAndOwnerLifetime();
    void activationMayDestroyOwner();
    void accessibilityStateAndAssociation();
};

#endif
