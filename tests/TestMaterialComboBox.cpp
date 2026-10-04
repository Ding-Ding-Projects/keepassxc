#include "TestMaterialComboBox.h"
#include "gui/material/MaterialControls.h"
#include "gui/material/MaterialChoicePopup.h"
#include "gui/material/MaterialRegexBuilder.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSearchRegistry.h"

#include <QAbstractProxyModel>
#include <QAccessible>
#include <QApplication>
#include <QCompleter>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPointer>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <QValidator>

using namespace Material;

namespace {
QWidget* popup(ComboBox& combo)
{
    combo.show();
    combo.showPopup();
    return combo.findChild<QWidget*>(QStringLiteral("materialChoicePopup"));
}
SearchBar* search(QWidget* widget) { return widget->findChild<SearchBar*>(); }
QListView* choices(QWidget* widget) { return widget->findChild<QListView*>(QStringLiteral("materialChoiceList")); }
int rows(QListView* view) { return view->model()->rowCount(view->rootIndex()); }
void choose(QListView* view, int row)
{
    view->setCurrentIndex(view->model()->index(row, view->modelColumn(), view->rootIndex()));
    QTest::keyClick(view, Qt::Key_Return);
}
}

void TestMaterialComboBox::localSearchPreservesSharedModel()
{
    QObject owner;
    QStandardItemModel model(&owner);
    model.appendRow(new QStandardItem(QStringLiteral("literal .*")));
    model.appendRow(new QStandardItem(QStringLiteral("Other")));
    ComboBox combo, sibling;
    combo.setModel(&model); sibling.setModel(&model);
    combo.setCurrentIndex(1);
    auto* window = popup(combo); QVERIFY(window);
    auto* bar = search(window); QVERIFY(bar);
    auto* view = choices(window); QVERIFY(view);
    QVERIFY(qobject_cast<QAbstractProxyModel*>(view->model()));
    bar->setText(QStringLiteral(".*"));
    QCOMPARE(rows(view), 1);
    QCOMPARE(combo.model(), &model); QCOMPARE(sibling.count(), 2);
    QCOMPARE(combo.currentIndex(), 1); QCOMPARE(model.parent(), &owner);
    QVERIFY(!bar->isRegexEnabled());
}

void TestMaterialComboBox::duplicateLabelsActivateOriginalIndex()
{
    ComboBox combo;
    combo.addItem(QStringLiteral("Other"), 7);
    combo.addItem(QStringLiteral("Same"), 11);
    combo.addItem(QStringLiteral("Same"), 19);
    QSignalSpy activated(&combo, &QComboBox::activated);
    QSignalSpy textActivated(&combo, &QComboBox::textActivated);
    auto* window = popup(combo); QVERIFY(window);
    search(window)->setText(QStringLiteral("Same"));
    choose(choices(window), 1);
    QCOMPARE(combo.currentIndex(), 2); QCOMPARE(combo.currentData().toInt(), 19);
    QCOMPARE(activated.count(), 1); QCOMPARE(activated.first().first().toInt(), 2);
    QCOMPARE(textActivated.count(), 1); QVERIFY(!window->isVisible());
}

void TestMaterialComboBox::nonzeroRootAndColumn()
{
    QStandardItemModel model;
    auto* parent = new QStandardItem(QStringLiteral("Parent"));
    model.appendRow(parent);
    parent->appendRow({new QStandardItem(QStringLiteral("ignored")), new QStandardItem(QStringLiteral("Alpha"))});
    parent->appendRow({new QStandardItem(QStringLiteral("ignored")), new QStandardItem(QStringLiteral("Beta"))});
    parent->child(1, 1)->setData(QStringLiteral("original-role"), Qt::UserRole);
    ComboBox combo;
    combo.setModel(&model); combo.setRootModelIndex(parent->index()); combo.setModelColumn(1);
    combo.setCurrentIndex(0);
    auto* window = popup(combo); QVERIFY(window);
    search(window)->setText(QStringLiteral("Beta"));
    QCOMPARE(rows(choices(window)), 1);
    choose(choices(window), 0);
    QCOMPARE(combo.currentIndex(), 1); QCOMPARE(combo.currentText(), QStringLiteral("Beta"));
    QCOMPARE(combo.currentData().toString(), QStringLiteral("original-role"));
    QCOMPARE(combo.rootModelIndex(), parent->index()); QCOMPARE(combo.modelColumn(), 1);
}

void TestMaterialComboBox::mutationsFollowOriginalIndices()
{
    QStandardItemModel model;
    model.appendRow(new QStandardItem(QStringLiteral("Other")));
    model.appendRow(new QStandardItem(QStringLiteral("Target")));
    ComboBox combo; combo.setModel(&model);
    auto* window = popup(combo); QVERIFY(window);
    search(window)->setText(QStringLiteral("Target"));
    model.insertRow(0, new QStandardItem(QStringLiteral("Inserted")));
    QTRY_COMPARE(rows(choices(window)), 1);
    choose(choices(window), 0); QCOMPARE(combo.currentIndex(), 2);
    window = popup(combo); QVERIFY(window);
    search(window)->setText(QStringLiteral("Target"));
    model.item(2)->setText(QStringLiteral("Renamed"));
    QTRY_COMPARE(rows(choices(window)), 0);
    model.removeRow(2); model.clear();
    QTRY_COMPARE(rows(choices(window)), 0);
}

void TestMaterialComboBox::replacementThroughBasePointerCancels()
{
    QStandardItemModel first, second;
    first.appendRow(new QStandardItem(QStringLiteral("First")));
    second.appendRow(new QStandardItem(QStringLiteral("Second")));
    ComboBox combo; combo.setModel(&first);
    auto* window = popup(combo); QVERIFY(window);
    QComboBox* base = &combo; base->setModel(&second);
    QTRY_VERIFY(!window->isVisible());
    QCOMPARE(combo.model(), &second);
    window = popup(combo); QVERIFY(window);
    QCOMPARE(choices(window)->model()->index(0, 0).data().toString(), QStringLiteral("Second"));
}

void TestMaterialComboBox::sourceDestructionCancels()
{
    auto* model = new QStandardItemModel;
    model->appendRow(new QStandardItem(QStringLiteral("Choice")));
    ComboBox combo; combo.setModel(model);
    QSignalSpy activated(&combo, &QComboBox::activated);
    auto* window = popup(combo); QVERIFY(window);
    delete model;
    QTRY_VERIFY(!window->isVisible()); QCOMPARE(activated.count(), 0);
}

void TestMaterialComboBox::disabledRowsAndSameItemActivation()
{
    QStandardItemModel model;
    auto* disabled = new QStandardItem(QStringLiteral("Disabled")); disabled->setEnabled(false);
    model.appendRow(disabled); model.appendRow(new QStandardItem(QStringLiteral("Enabled")));
    ComboBox combo; combo.setModel(&model); combo.setCurrentIndex(1);
    QSignalSpy activated(&combo, &QComboBox::activated), changed(&combo, &QComboBox::currentIndexChanged);
    auto* window = popup(combo); QVERIFY(window);
    auto* view = choices(window);
    const auto disabledIndex = view->model()->index(0, view->modelColumn(), view->rootIndex());
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->visualRect(disabledIndex).center());
    QCOMPARE(activated.count(), 0); QVERIFY(window->isVisible());
    choose(choices(window), 1); QCOMPARE(activated.count(), 1); QCOMPARE(changed.count(), 0);
}

void TestMaterialComboBox::editableFieldAndInsertionRemainNative()
{
    ComboBox combo; combo.setEditable(true); combo.addItems({QStringLiteral("Alpha"), QStringLiteral("Beta")});
    auto* edit = new QLineEdit(&combo);
    auto* validator = new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[A-Za-z]+")), edit);
    edit->setValidator(validator); combo.setLineEdit(edit);
    auto* completer = new QCompleter(combo.model(), &combo); combo.setCompleter(completer);
    combo.setInsertPolicy(QComboBox::InsertAtBottom); combo.setEditText(QStringLiteral("Draft"));
    auto* model = combo.model();
    auto* window = popup(combo); QVERIFY(window);
    search(window)->setText(QStringLiteral("Beta"));
    QCOMPARE(combo.currentText(), QStringLiteral("Draft"));
    combo.hidePopup();
    QCOMPARE(combo.lineEdit(), edit); QCOMPARE(combo.completer(), completer);
    QCOMPARE(combo.validator(), validator); QCOMPARE(combo.model(), model);
    QCOMPARE(combo.insertPolicy(), QComboBox::InsertAtBottom);
    QTest::keyClick(edit, Qt::Key_Return);
    QCOMPARE(combo.count(), 3); QCOMPARE(combo.itemText(2), QStringLiteral("Draft"));
}

void TestMaterialComboBox::invalidRegexFailsClosed()
{
    ComboBox combo; combo.addItem(QStringLiteral("Choice"));
    auto* window = popup(combo); QVERIFY(window);
    auto* bar = search(window); bar->setRegexEnabled(true); bar->setText(QStringLiteral("["));
    QCOMPARE(rows(choices(window)), 0);
    auto* status = window->findChild<QLabel*>(QStringLiteral("materialChoiceStatus")); QVERIFY(status);
    QVERIFY(!status->text().isEmpty());
    QSignalSpy activated(&combo, &QComboBox::activated);
    QTest::keyClick(bar->lineEdit(), Qt::Key_Return); QCOMPARE(activated.count(), 0);
}

void TestMaterialComboBox::flagsOnlyChangeAndUnicode()
{
    ComboBox combo; combo.addItem(QStringLiteral("COPY 香港"));
    auto* window = popup(combo); QVERIFY(window);
    auto* bar = search(window); bar->setRegexEnabled(true); bar->setText(QStringLiteral("^copy"));
    QCOMPARE(rows(choices(window)), 1);
    bar->setRegexFlags(QString()); QCOMPARE(rows(choices(window)), 0);
    bar->setText(QStringLiteral("\\p{Han}+")); QCOMPARE(rows(choices(window)), 1);
    bar->setText(QStringLiteral("(?=香)")); QCOMPARE(rows(choices(window)), 1);
}

void TestMaterialComboBox::limitsFailClosed()
{
    ComboBox combo; combo.addItem(QStringLiteral("OK")); combo.addItem(QString(2049, QLatin1Char('a')));
    auto* window = popup(combo); QVERIFY(window);
    auto* bar = search(window); bar->setRegexEnabled(true); bar->setText(QStringLiteral(".*"));
    QCOMPARE(rows(choices(window)), 0);
    combo.clear(); combo.addItem(QStringLiteral("OK"));
    bar->setText(QStringLiteral("(a+)+$")); QCOMPARE(rows(choices(window)), 0);
    bar->setText(QString(513, QLatin1Char('a'))); QCOMPARE(rows(choices(window)), 0);
}

void TestMaterialComboBox::builderStaysLocalAndHasNoChoiceSamples()
{
    ComboBox combo; combo.addItem(QStringLiteral("SYNTHETIC_PRIVATE_CHOICE"));
    auto* window = popup(combo); QVERIFY(window);
    auto* bar = search(window); bar->setText(QStringLiteral("^Syn"));
    QVERIFY(QMetaObject::invokeMethod(bar, "builderRequested"));
    auto* builder = window->findChild<RegexBuilder*>(); QVERIFY(builder);
    QVERIFY(window->isVisible()); QVERIFY(builder->isOpen());
    QVERIFY(builder->sampleText().isEmpty()); QCOMPARE(builder->pattern(), bar->text());
    QCOMPARE(builder->dialect(), QStringLiteral("qt"));
    builder->setFlags(QStringLiteral("m"));
    QVERIFY(QMetaObject::invokeMethod(builder, "patternApplied", Q_ARG(QString, QStringLiteral("PRIVATE"))));
    QCOMPARE(bar->text(), QStringLiteral("PRIVATE")); QCOMPARE(bar->regexFlags(), QStringLiteral("m"));
    QVERIFY(bar->isRegexEnabled());
    combo.hidePopup(); QVERIFY(!builder->isOpen());
}

void TestMaterialComboBox::cancellationDoesNotCommit()
{
    ComboBox combo; combo.addItems({QStringLiteral("A"), QStringLiteral("B")}); combo.setCurrentIndex(1);
    auto* window = popup(combo); QVERIFY(window);
    QSignalSpy activated(&combo, &QComboBox::activated);
    search(window)->setText(QStringLiteral("A")); QTest::keyClick(search(window)->lineEdit(), Qt::Key_Escape);
    QVERIFY(!window->isVisible()); QCOMPARE(combo.currentIndex(), 1); QCOMPARE(activated.count(), 0);
    combo.showPopup(); QCOMPARE(search(window)->text(), QString());
}

void TestMaterialComboBox::hideAndOwnerLifetime()
{
    auto* combo = new ComboBox; combo->addItem(QStringLiteral("Choice"));
    QPointer<QWidget> window = popup(*combo); QVERIFY(window);
    combo->hide(); QVERIFY(!window->isVisible());
    combo->show(); combo->showPopup(); QVERIFY(window->isVisible());
    delete combo; QVERIFY(window.isNull());
}

void TestMaterialComboBox::activationMayDestroyOwner()
{
    auto* combo = new ComboBox; combo->addItems({QStringLiteral("A"), QStringLiteral("B")});
    QPointer<ComboBox> owner(combo);
    auto* window = popup(*combo); QVERIFY(window);
    connect(combo, &QComboBox::activated, combo, [combo] { delete combo; });
    choose(choices(window), 1); QVERIFY(owner.isNull());

    // Native QComboBox continues using itself after currentIndexChanged. As
    // with a stock combo, destruction from that signal must be deferred.
    combo = new ComboBox; combo->addItems({QStringLiteral("A"), QStringLiteral("B")});
    owner = combo; window = popup(*combo); QVERIFY(window);
    connect(combo, &QComboBox::currentIndexChanged, combo, &QObject::deleteLater);
    choose(choices(window), 1);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(owner.isNull());
}

void TestMaterialComboBox::accessibilityStateAndAssociation()
{
    ComboBox combo; combo.setAccessibleName(QStringLiteral("Synthetic choice")); combo.addItem(QStringLiteral("A"));
    auto* window = popup(combo); QVERIFY(window);
    auto* accessible = QAccessible::queryAccessibleInterface(&combo); QVERIFY(accessible);
    QCOMPARE(accessible->role(), QAccessible::ComboBox); QVERIFY(accessible->state().expanded);
    bool associated = false;
    for (const auto& relation : accessible->relations())
        if (relation.first && relation.first->object() == window) associated = true;
    QVERIFY(associated);
    QVERIFY(!search(window)->lineEdit()->accessibleName().isEmpty());
    auto* list = QAccessible::queryAccessibleInterface(choices(window)); QVERIFY(list);
    QCOMPARE(list->role(), QAccessible::List);
    combo.hidePopup(); QVERIFY(!accessible->state().expanded); QVERIFY(accessible->state().collapsed);
}

void TestMaterialComboBox::openingKeepsCurrentCandidate()
{
    ComboBox combo; combo.addItems({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});
    combo.setCurrentIndex(2);
    auto* window = popup(combo); QVERIFY(window);
    QSignalSpy activated(&combo, &QComboBox::activated), changed(&combo, &QComboBox::currentIndexChanged);
    QTest::keyClick(search(window)->lineEdit(), Qt::Key_Return);
    QCOMPARE(combo.currentIndex(), 2); QCOMPARE(changed.count(), 0);
    QCOMPARE(activated.count(), 1); QCOMPARE(activated.first().first().toInt(), 2);
}

void TestMaterialComboBox::changedLabelCannotActivateStaleResult()
{
    QStandardItemModel model;
    model.appendRow(new QStandardItem(QStringLiteral("Other")));
    model.appendRow(new QStandardItem(QStringLiteral("Target")));
    ComboBox combo; combo.setModel(&model);
    auto* window = popup(combo); QVERIFY(window);
    search(window)->setText(QStringLiteral("Target"));
    QSignalSpy activated(&combo, &QComboBox::activated);
    model.item(1)->setText(QStringLiteral("Renamed"));
    // Deliberately do not drain the queued dataChanged refresh before Return.
    QTest::keyClick(search(window)->lineEdit(), Qt::Key_Return);
    QCOMPARE(activated.count(), 0); QCOMPARE(combo.currentIndex(), 0);
}

void TestMaterialComboBox::bindingChangeCannotActivateStaleResult()
{
    QStandardItemModel first, second;
    first.appendRow(new QStandardItem(QStringLiteral("Old")));
    second.appendRow(new QStandardItem(QStringLiteral("New")));
    ComboBox combo; combo.setModel(&first);
    auto* window = popup(combo); QVERIFY(window);
    QSignalSpy activated(&combo, &QComboBox::activated);
    static_cast<QComboBox*>(&combo)->setModel(&second);
    QTest::keyClick(search(window)->lineEdit(), Qt::Key_Return);
    QCOMPARE(activated.count(), 0); QVERIFY(!window->isVisible());
}

void TestMaterialComboBox::builderIsNotGloballyRouted()
{
    ComboBox combo; combo.addItem(QStringLiteral("Private choice"));
    auto* window = popup(combo); QVERIFY(window);
    QSignalSpy routed(SearchRegistry::instance(), &SearchRegistry::builderRequested);
    QVERIFY(QMetaObject::invokeMethod(search(window), "builderRequested"));
    QCOMPARE(routed.count(), 0);
    QVERIFY(!SearchRegistry::instance()->bars().contains(search(window)));
    auto* builder = window->findChild<RegexBuilder*>(); QVERIFY(builder);
    QCOMPARE(builder->sampleText(), QString());
    builder->closeOverlay();
    QTRY_VERIFY(!builder->isVisible());
    QVERIFY(window->isVisible());
}

void TestMaterialComboBox::builderReturnDoesNotActivateChoice()
{
    ComboBox combo; combo.addItems({QStringLiteral("A"), QStringLiteral("B")});
    auto* window = popup(combo); QVERIFY(window);
    QVERIFY(QMetaObject::invokeMethod(search(window), "builderRequested"));
    auto* builder = window->findChild<RegexBuilder*>(); QVERIFY(builder);
    auto* patternEdit = qobject_cast<QLineEdit*>(builder->sheetWidget()->focusProxy()); QVERIFY(patternEdit);
    QSignalSpy activated(&combo, &QComboBox::activated);
    QTest::keyClick(patternEdit, Qt::Key_Return);
    QCOMPARE(activated.count(), 0);
    QVERIFY(window->isVisible()); QVERIFY(builder->isOpen());
    QTest::keyClick(patternEdit, Qt::Key_Escape);
    QTRY_VERIFY(!builder->isOpen());
    QVERIFY(window->isVisible()); QCOMPARE(activated.count(), 0);
}

void TestMaterialComboBox::boundedRegexEngineErrors_data()
{
    QTest::addColumn<QString>("pattern");
    QTest::addColumn<QStringList>("labels");
    QTest::newRow("match-limit") << QStringLiteral("(*NO_JIT)(*LIMIT_MATCH=10000)^(?:OK|(?:a?){30}a{30})$")
        << QStringList{QStringLiteral("OK"), QString(30, QLatin1Char('a'))};
    QTest::newRow("depth-limit") << QStringLiteral("(*NO_JIT)(*LIMIT_DEPTH=1)^(?:OK|a)$")
        << QStringList{QStringLiteral("a")};
    QStringList many;
    for (int row = 0; row < 1025; ++row) many.append(QStringLiteral("Choice"));
    QTest::newRow("row-count") << QStringLiteral(".*") << many;
}

void TestMaterialComboBox::boundedRegexEngineErrors()
{
    QFETCH(QString, pattern); QFETCH(QStringList, labels);
    ComboBox combo; combo.addItems(labels);
    auto* window = popup(combo); QVERIFY(window);
    search(window)->setRegexEnabled(true); search(window)->setText(pattern);
    QCOMPARE(rows(choices(window)), 0);
    QSignalSpy activated(&combo, &QComboBox::activated);
    QTest::keyClick(search(window)->lineEdit(), Qt::Key_Return);
    QCOMPARE(activated.count(), 0); QVERIFY(window->isVisible());
}

void TestMaterialComboBox::dismissalBindingChangeCannotCommit_data()
{
    QTest::addColumn<bool>("changeRoot");
    QTest::newRow("root-changed-during-dismissal") << true;
    QTest::newRow("column-changed-during-dismissal") << false;
}

void TestMaterialComboBox::dismissalBindingChangeCannotCommit()
{
    QFETCH(bool, changeRoot);
    QStandardItemModel model;
    auto* originalRoot = new QStandardItem(QStringLiteral("Original"));
    auto* replacementRoot = new QStandardItem(QStringLiteral("Replacement"));
    model.appendRow(originalRoot); model.appendRow(replacementRoot);
    for (int row = 0; row < 3; ++row) {
        originalRoot->appendRow({new QStandardItem(QStringLiteral("Original %1").arg(row)),
                                 new QStandardItem(QStringLiteral("Other column %1").arg(row))});
        replacementRoot->appendRow({new QStandardItem(QStringLiteral("Replacement %1").arg(row)),
                                    new QStandardItem(QStringLiteral("Replacement column %1").arg(row))});
    }
    ComboBox combo; combo.setModel(&model); combo.setRootModelIndex(originalRoot->index());
    combo.setCurrentIndex(0);
    auto* window = popup(combo); QVERIFY(window);
    auto* choicePopup = qobject_cast<ChoicePopup*>(window); QVERIFY(choicePopup);
    QSignalSpy activated(&combo, &QComboBox::activated), changed(&combo, &QComboBox::currentIndexChanged);
    bool dismissed = false;
    int indexAfterDismissal = -1;
    QString textAfterDismissal;
    connect(choicePopup, &ChoicePopup::dismissed, &combo, [&] {
        dismissed = true;
        if (changeRoot) combo.setRootModelIndex(replacementRoot->index());
        else combo.setModelColumn(1);
        indexAfterDismissal = combo.currentIndex();
        textAfterDismissal = combo.currentText();
        changed.clear(); // Only subsequent writes belong to stale activation.
    });
    choose(choices(window), 2);
    QVERIFY(dismissed); QCOMPARE(combo.model(), &model);
    QCOMPARE(activated.count(), 0);
    QCOMPARE(combo.currentIndex(), indexAfterDismissal);
    QCOMPARE(combo.currentText(), textAfterDismissal);
    QCOMPARE(changed.count(), 0);
}

void TestMaterialComboBox::editableAccessibleFocusRoutesToEditor_data()
{
    QTest::addColumn<bool>("material");
    QTest::newRow("native-qt-control") << false;
    QTest::newRow("material-control") << true;
}

void TestMaterialComboBox::editableAccessibleFocusRoutesToEditor()
{
    QFETCH(bool, material);
    QScopedPointer<QComboBox> combo(material ? static_cast<QComboBox*>(new ComboBox) : new QComboBox);
    combo->setEditable(true); combo->addItem(QStringLiteral("Synthetic editable choice"));
    combo->show(); combo->activateWindow(); combo->setFocus();
    QTRY_VERIFY(combo->hasFocus());
    combo->lineEdit()->setCursorPosition(4);
    auto* accessible = QAccessible::queryAccessibleInterface(combo.data()); QVERIFY(accessible);
    auto* focus = accessible->focusChild(); QVERIFY(focus);
    QCOMPARE(focus->object(), combo->lineEdit());
    auto* text = focus->textInterface(); QVERIFY(text);
    QCOMPARE(text->cursorPosition(), 4);
    QCOMPARE(text->text(0, text->characterCount()), combo->lineEdit()->text());
}

void TestMaterialComboBox::dismissalDeletionIsSafe_data()
{
    QTest::addColumn<bool>("deleteOwner");
    QTest::addColumn<bool>("activate");
    QTest::newRow("owner-direct") << true << false;
    QTest::newRow("owner-selection") << true << true;
    QTest::newRow("popup-direct") << false << false;
    QTest::newRow("popup-selection") << false << true;
}

void TestMaterialComboBox::dismissalDeletionIsSafe()
{
    QFETCH(bool, deleteOwner);
    QFETCH(bool, activate);
    QPointer<ComboBox> owner = new ComboBox;
    owner->addItems({QStringLiteral("A"), QStringLiteral("B")});
    QPointer<ChoicePopup> window = qobject_cast<ChoicePopup*>(popup(*owner));
    QVERIFY(window);
    QPointer<SearchBar> bar = search(window);
    QVERIFY(bar);
    bool dismissed = false;
    int activations = 0;
    connect(owner, &QComboBox::activated, this, [&] { ++activations; });
    connect(window, &ChoicePopup::dismissed, this, [&] {
        dismissed = true;
        if (deleteOwner) delete owner.data();
        else delete window.data();
        // The search control must already be gone while hideEvent unwinds.
        // A signal blocker surviving dismissed would retain its deleted target.
        QVERIFY(bar.isNull());
    });
    if (activate) {
        auto* view = choices(window);
        view->setCurrentIndex(view->model()->index(1, view->modelColumn(), view->rootIndex()));
        // Only send the press: the callback deliberately deletes its receiver.
        QTest::keyPress(view, Qt::Key_Return);
    } else {
        owner->hidePopup();
    }
    QVERIFY(dismissed);
    QVERIFY(window.isNull());
    QVERIFY(bar.isNull());
    QCOMPARE(activations, 0);
    if (deleteOwner) {
        QVERIFY(owner.isNull());
    } else {
        QVERIFY(owner);
        QCOMPARE(owner->currentIndex(), 0);
        auto* replacement = popup(*owner);
        QVERIFY(replacement);
        QVERIFY(replacement->isVisible());
        owner->hidePopup();
        delete owner.data();
    }
}

void TestMaterialComboBox::dismissalEligibilityChangeCannotCommit_data()
{
    QTest::addColumn<bool>("disable");
    QTest::newRow("enabled-removed") << true;
    QTest::newRow("selectable-removed") << false;
}

void TestMaterialComboBox::dismissalEligibilityChangeCannotCommit()
{
    QFETCH(bool, disable);
    QStandardItemModel model;
    model.appendRow(new QStandardItem(QStringLiteral("A")));
    auto* selected = new QStandardItem(QStringLiteral("B"));
    model.appendRow(selected);
    ComboBox combo;
    combo.setModel(&model);
    combo.setCurrentIndex(0);
    auto* window = qobject_cast<ChoicePopup*>(popup(combo));
    QVERIFY(window);
    QSignalSpy activated(&combo, &QComboBox::activated);
    QSignalSpy changed(&combo, &QComboBox::currentIndexChanged);
    bool dismissed = false;
    connect(window, &ChoicePopup::dismissed, &combo, [&] {
        dismissed = true;
        if (disable) selected->setEnabled(false);
        else selected->setSelectable(false);
    });
    choose(choices(window), 1);
    QVERIFY(dismissed);
    QCOMPARE(activated.count(), 0);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(combo.currentIndex(), 0);
}

void TestMaterialComboBox::activationTextFollowsSelection_data()
{
    QTest::addColumn<bool>("editable");
    QTest::addColumn<int>("callbackStage");
    QTest::newRow("fixed-dismissal") << false << 0;
    QTest::newRow("editable-dismissal") << true << 0;
    QTest::newRow("fixed-index-callback") << false << 1;
    QTest::newRow("editable-index-callback") << true << 1;
    QTest::newRow("fixed-activation-callback") << false << 2;
    QTest::newRow("editable-activation-callback") << true << 2;
}

void TestMaterialComboBox::activationTextFollowsSelection()
{
    QFETCH(bool, editable);
    QFETCH(int, callbackStage);
    ComboBox combo;
    combo.setEditable(editable);
    combo.addItems({QStringLiteral("Initial"), QStringLiteral("Old label")});
    combo.setCurrentIndex(0);
    auto* window = qobject_cast<ChoicePopup*>(popup(combo));
    QVERIFY(window);
    QSignalSpy activated(&combo, &QComboBox::activated);
    QSignalSpy textActivated(&combo, &QComboBox::textActivated);
    bool dismissed = false;
    QString selectedTextAtActivation;
    connect(window, &ChoicePopup::dismissed, &combo, [&] {
        dismissed = true;
        combo.setItemText(1, QStringLiteral("Dismissed label"));
    });
    connect(&combo, &QComboBox::currentIndexChanged, &combo, [&](int index) {
        if (index == 1 && callbackStage >= 1) {
            combo.setItemText(1, QStringLiteral("Index callback label"));
        }
    });
    connect(&combo, &QComboBox::activated, &combo, [&](int index) {
        QCOMPARE(index, 1);
        selectedTextAtActivation = combo.currentText();
        if (callbackStage == 2) combo.setItemText(1, QStringLiteral("Activation callback label"));
    });
    choose(choices(window), 1);
    QVERIFY(dismissed);
    QCOMPARE(combo.currentIndex(), 1);
    QCOMPARE(activated.count(), 1);
    QCOMPARE(textActivated.count(), 1);
    QCOMPARE(selectedTextAtActivation,
             callbackStage >= 1 ? QStringLiteral("Index callback label") : QStringLiteral("Dismissed label"));
    QCOMPARE(combo.currentText(),
             callbackStage == 2 ? QStringLiteral("Activation callback label") : selectedTextAtActivation);
    QCOMPARE(textActivated.at(0).at(0).toString(), selectedTextAtActivation);
}

int main(int argc, char** argv)
{
    const QString identity = QStringLiteral("kpxc-choice-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir isolated(QDir::tempPath() + QStringLiteral("/kcc-XXXXXX"));
    if (!isolated.isValid()) return 2;
    qputenv("KPXC_CONFIG", (isolated.path() + QStringLiteral("/settings.ini")).toUtf8());
    qputenv("KPXC_CONFIG_LOCAL", (isolated.path() + QStringLiteral("/local.ini")).toUtf8());
    qputenv("USERNAME", identity.toUtf8()); qputenv("USER", identity.toUtf8());
    QCoreApplication::setOrganizationName(identity);
    QCoreApplication::setApplicationName(identity);
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(identity);
    TestMaterialComboBox test;
    return QTest::qExec(&test, argc, argv);
}
