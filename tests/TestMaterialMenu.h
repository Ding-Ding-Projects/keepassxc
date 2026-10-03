#ifndef KEEPASSXC_TESTMATERIALMENU_H
#define KEEPASSXC_TESTMATERIALMENU_H
#include <QObject>
class TestMaterialMenu : public QObject
{
    Q_OBJECT
private slots:
    void everyNativeMenuGetsOneSearch();
    void plainTextIsolationAndOriginalActions();
    void invalidAndBoundedRegexFailClosed();
    void flagsOnlyRefilterAndUnicode();
    void dynamicPopulationAndClear();
    void existingSearchIsNotDuplicated();
    void keyboardRequiresExplicitVisibleSelection();
    void hiddenAndDisabledActionsRemainSafe();
    void nestedMenusAndStandardEditorMenus();
    void inlineFullBuilderPreservesExecAndLifetime();
    void localizationAndSessionHistory();
    void borrowedSearchFlagsReachExistingConsumer();
    void pendingCallbacksDoNotRefilterClosedMenu_data();
    void pendingCallbacksDoNotRefilterClosedMenu();
    void matchLimitFailureDiscardsPartialResults();
    void selectKeepsKeyboardNavigation();
    void selectKeepsClearFirstEscape();
    void selectStatusCountsItsChoices();
    void removedSharedActionRestoresVisibility();
    void dynamicRefreshReturnsFocusToOpener();
    void selectBuilderFitsAndRestoresWidth();
    void selectInvalidRegexClearsPreviousChoice();
    void selectRegexLimits_data();
    void selectRegexLimits();
};
#endif
