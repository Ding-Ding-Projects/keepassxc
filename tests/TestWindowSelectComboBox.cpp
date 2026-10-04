#include "TestWindowSelectComboBox.h"
#include "autotype/WindowSelectComboBox.h"
#include "gui/material/MaterialControls.h"
#include "gui/material/MaterialRegexBuilder.h"
#include "gui/material/MaterialSearchBar.h"

#include <QAbstractProxyModel>
#include <QAccessible>
#include <QApplication>
#include <QCompleter>
#include <QDir>
#include <QLineEdit>
#include <QListView>
#include <QPointer>
#include <QRegularExpressionValidator>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

namespace {
QWidget* open(WindowSelectComboBox& combo)
{
    combo.show();
    combo.showPopup();
    return combo.findChild<QWidget*>(QStringLiteral("materialChoicePopup"));
}
Material::SearchBar* search(QWidget* popup) { return popup->findChild<Material::SearchBar*>(); }
QListView* choices(QWidget* popup) { return popup->findChild<QListView*>(QStringLiteral("materialChoiceList")); }
int rows(QListView* view) { return view->model()->rowCount(view->rootIndex()); }
void choose(QListView* view, int row)
{
    view->setCurrentIndex(view->model()->index(row, view->modelColumn(), view->rootIndex()));
    QTest::keyClick(view, Qt::Key_Return);
}
}

void TestWindowSelectComboBox::customTextRefreshAndData()
{
    int calls = 0;
    QStringList titles{QStringLiteral("Synthetic Alpha"), QStringLiteral("Synthetic Beta")};
    WindowSelectComboBox combo(nullptr, [&] { ++calls; return titles; });
    QCOMPARE(calls, 0);
    QCOMPARE(combo.count(), 1);
    combo.setItemData(0, 71);
    combo.setEditText(QStringLiteral("Custom * association"));
    auto* model = combo.model();
    auto* editor = combo.lineEdit();
    combo.refreshWindowList();
    QCOMPARE(calls, 1);
    QCOMPARE(combo.count(), 3);
    QCOMPARE(combo.itemText(0), QStringLiteral("Custom * association"));
    QCOMPARE(combo.currentText(), QStringLiteral("Custom * association"));
    QCOMPARE(combo.itemData(0).toInt(), 71);
    QCOMPARE(combo.model(), model);
    QCOMPARE(combo.lineEdit(), editor);
    combo.setCurrentIndex(2);
    titles = {QStringLiteral("Synthetic Gamma")};
    combo.refreshWindowList();
    QCOMPARE(calls, 2);
    QCOMPARE(combo.count(), 2);
    QCOMPARE(combo.itemText(0), QStringLiteral("Synthetic Beta"));
    QCOMPARE(combo.currentText(), QStringLiteral("Synthetic Beta"));
    QCOMPARE(combo.itemText(1), QStringLiteral("Synthetic Gamma"));
}

void TestWindowSelectComboBox::searchableOriginalIndexActivation()
{
    WindowSelectComboBox combo(nullptr, [] {
        return QStringList{QStringLiteral("Other"), QStringLiteral("Same"), QStringLiteral("Same")};
    });
    combo.setEditText(QStringLiteral("Custom"));
    auto* model = combo.model();
    QSignalSpy activated(&combo, &QComboBox::activated);
    auto* popup = open(combo); QVERIFY(popup);
    auto* bar = search(popup); QVERIFY(bar);
    auto* view = choices(popup); QVERIFY(view);
    QVERIFY(qobject_cast<QAbstractProxyModel*>(view->model()));
    combo.setItemData(2, 12); combo.setItemData(3, 93);
    bar->setText(QStringLiteral("Same"));
    QCOMPARE(rows(view), 2);
    QCOMPARE(combo.model(), model);
    QCOMPARE(combo.currentText(), QStringLiteral("Custom"));
    choose(view, 1);
    QCOMPARE(combo.currentIndex(), 3);
    QCOMPARE(combo.currentData().toInt(), 93);
    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.first().first().toInt(), 3);
    QCOMPARE(combo.lineEdit()->text(), QStringLiteral("Same"));
}

void TestWindowSelectComboBox::readOnlyDoesNotRefreshOrOpen()
{
    int calls = 0;
    WindowSelectComboBox combo(nullptr, [&] { ++calls; return QStringList{QStringLiteral("Synthetic")}; });
    combo.setEditText(QStringLiteral("Read only association"));
    combo.lineEdit()->setReadOnly(true);
    combo.show(); combo.showPopup();
    QCOMPARE(calls, 0);
    QCOMPARE(combo.count(), 1);
    QCOMPARE(combo.currentText(), QStringLiteral("Read only association"));
    QVERIFY(!combo.findChild<QWidget*>(QStringLiteral("materialChoicePopup")));
}

void TestWindowSelectComboBox::reopeningRefreshesAndCancellationPreservesText()
{
    int calls = 0;
    QStringList titles{QStringLiteral("Old synthetic title")};
    WindowSelectComboBox combo(nullptr, [&] { ++calls; return titles; });
    combo.setEditText(QStringLiteral("Custom literal .*"));
    QSignalSpy activated(&combo, &QComboBox::activated);
    auto* popup = open(combo); QVERIFY(popup);
    search(popup)->setText(QStringLiteral("Old"));
    QTest::keyClick(search(popup)->lineEdit(), Qt::Key_Escape);
    QVERIFY(!popup->isVisible());
    QCOMPARE(combo.currentText(), QStringLiteral("Custom literal .*"));
    QCOMPARE(activated.count(), 0);
    titles = {QStringLiteral("New synthetic title")};
    combo.showPopup();
    QCOMPARE(calls, 2);
    QCOMPARE(search(popup)->text(), QString());
    QCOMPARE(rows(choices(popup)), 2);
    QCOMPARE(combo.itemText(1), QStringLiteral("New synthetic title"));
    search(popup)->setText(QStringLiteral(".*"));
    QCOMPARE(rows(choices(popup)), 1); // Plain text by default, including the custom row.
    choose(choices(popup), 0);
    QCOMPARE(combo.currentIndex(), 0);
    QCOMPARE(combo.currentText(), QStringLiteral("Custom literal .*"));
    QCOMPARE(activated.count(), 1);
}

void TestWindowSelectComboBox::builderIsAdjacentAndHasNoTitleSamples()
{
    WindowSelectComboBox combo(nullptr, [] { return QStringList{QStringLiteral("SYNTHETIC PRIVATE TITLE")}; });
    auto* popup = open(combo); QVERIFY(popup);
    auto* bar = search(popup); QVERIFY(bar);
    bar->setText(QStringLiteral("^synthetic"));
    QVERIFY(QMetaObject::invokeMethod(bar, "builderRequested"));
    auto* builder = popup->findChild<Material::RegexBuilder*>(); QVERIFY(builder);
    QVERIFY(builder->isOpen()); QVERIFY(popup->isVisible());
    QVERIFY(builder->sampleText().isEmpty());
    QCOMPARE(builder->pattern(), bar->text());
    QCOMPARE(builder->dialect(), QStringLiteral("qt"));
    builder->setFlags(QStringLiteral("i"));
    QVERIFY(QMetaObject::invokeMethod(builder, "patternApplied", Q_ARG(QString, QStringLiteral("^synthetic"))));
    QCOMPARE(bar->regexFlags(), QStringLiteral("i")); QVERIFY(bar->isRegexEnabled());
    QCOMPARE(rows(choices(popup)), 1);
    bar->setText(QStringLiteral("[")); QCOMPARE(rows(choices(popup)), 0);
    bar->setText(QString(513, QLatin1Char('x'))); QCOMPARE(rows(choices(popup)), 0);
    QCOMPARE(combo.currentText(), QString());
    combo.hidePopup(); QVERIFY(!builder->isOpen());
}

void TestWindowSelectComboBox::editableContractAndSizing()
{
    WindowSelectComboBox combo(nullptr, [] { return QStringList{}; });
    QVERIFY(combo.isEditable()); QCOMPARE(combo.insertPolicy(), QComboBox::NoInsert);
    QCOMPARE(combo.sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    QCOMPARE(combo.sizePolicy().verticalPolicy(), QSizePolicy::Fixed);
    auto* editor = combo.lineEdit();
    auto* completer = combo.completer();
    QRegularExpressionValidator validator(QRegularExpression(QStringLiteral("[A-Z* ]*")));
    combo.setValidator(&validator);
    combo.show(); combo.setFocus();
    QTest::keyClicks(editor, "CUSTOM *");
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(combo.count(), 1); QCOMPARE(combo.currentText(), QStringLiteral("CUSTOM *"));
    QCOMPARE(combo.sizeHint().width(), editor->sizeHint().width());
    QCOMPARE(combo.minimumSizeHint().width(), editor->minimumSizeHint().width());
    Material::ComboBox reference;
    QVERIFY(combo.sizeHint().height() >= reference.sizeHint().height());
    QVERIFY(combo.minimumSizeHint().height() >= reference.minimumSizeHint().height());
    auto* popup = open(combo); QVERIFY(popup);
    combo.hidePopup();
    QCOMPARE(combo.lineEdit(), editor); QCOMPARE(combo.completer(), completer);
    QCOMPARE(combo.validator(), &validator);
    combo.activateWindow(); combo.setFocus(); QTRY_VERIFY(combo.hasFocus());
    auto* accessible = QAccessible::queryAccessibleInterface(&combo); QVERIFY(accessible);
    auto* child = accessible->focusChild(); QVERIFY(child);
    QCOMPARE(child->object(), editor); QVERIFY(child->textInterface());
}

void TestWindowSelectComboBox::popupDiesWithOwner()
{
    auto* combo = new WindowSelectComboBox(nullptr, [] { return QStringList{QStringLiteral("Synthetic")}; });
    QPointer<QWidget> popup = open(*combo); QVERIFY(popup);
    delete combo;
    QVERIFY(popup.isNull());
}

int main(int argc, char** argv)
{
    const QString identity = QStringLiteral("kpxc-window-choice-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir isolated(QDir::tempPath() + QStringLiteral("/kwc-XXXXXX"));
    if (!isolated.isValid()) return 2;
    qputenv("KPXC_CONFIG", (isolated.path() + QStringLiteral("/settings.ini")).toUtf8());
    qputenv("KPXC_CONFIG_LOCAL", (isolated.path() + QStringLiteral("/local.ini")).toUtf8());
    qputenv("USERNAME", identity.toUtf8()); qputenv("USER", identity.toUtf8());
    QCoreApplication::setOrganizationName(identity);
    QCoreApplication::setApplicationName(identity);
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(identity);
    TestWindowSelectComboBox test;
    return QTest::qExec(&test, argc, argv);
}
