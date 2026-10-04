#ifndef KEEPASSXC_TESTWINDOWSELECTCOMBOBOX_H
#define KEEPASSXC_TESTWINDOWSELECTCOMBOBOX_H

#include <QObject>

class TestWindowSelectComboBox : public QObject
{
    Q_OBJECT
private slots:
    void customTextRefreshAndData();
    void searchableOriginalIndexActivation();
    void readOnlyDoesNotRefreshOrOpen();
    void reopeningRefreshesAndCancellationPreservesText();
    void builderIsAdjacentAndHasNoTitleSamples();
    void editableContractAndSizing();
    void popupDiesWithOwner();
};

#endif
