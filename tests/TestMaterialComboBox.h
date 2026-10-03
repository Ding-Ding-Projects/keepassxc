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
    void openingKeepsCurrentCandidate();
    void changedLabelCannotActivateStaleResult();
    void bindingChangeCannotActivateStaleResult();
    void builderIsNotGloballyRouted();
    void builderReturnDoesNotActivateChoice();
    void boundedRegexEngineErrors_data();
    void boundedRegexEngineErrors();
    void dismissalBindingChangeCannotCommit_data();
    void dismissalBindingChangeCannotCommit();
    void editableAccessibleFocusRoutesToEditor_data();
    void editableAccessibleFocusRoutesToEditor();
    void dismissalDeletionIsSafe_data();
    void dismissalDeletionIsSafe();
    void dismissalEligibilityChangeCannotCommit_data();
    void dismissalEligibilityChangeCannotCommit();
};

#endif
