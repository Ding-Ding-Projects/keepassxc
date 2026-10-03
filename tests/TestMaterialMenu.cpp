#include "TestMaterialMenu.h"
#include "gui/material/MaterialMenu.h"
#include "gui/material/MaterialRegexBuilder.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialVoice.h"

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWidgetAction>
#include <memory>

using namespace Material;

void TestMaterialMenu::everyNativeMenuGetsOneSearch()
{
    QMenu menu;
    menu.addAction(QStringLiteral("Open"));
    menu.popup(QPoint(40, 40));
    QTRY_VERIFY(MenuSearch::attach(&menu)->searchBar());
    QCOMPARE(menu.findChildren<SearchBar*>().size(), 1);
    menu.close();
    menu.popup(QPoint(40, 40));
    QTRY_COMPARE(menu.findChildren<SearchBar*>().size(), 1);
    QVERIFY(MenuSearch::attach(&menu)->searchBar()->showRegexControls());
    menu.close();
}

void TestMaterialMenu::plainTextIsolationAndOriginalActions()
{
    QMenu first, second;
    QObject owner;
    QAction original(QStringLiteral("&Copy .*"), &owner);
    original.setCheckable(true);
    original.setChecked(true);
    original.setShortcut(QKeySequence(QStringLiteral("Ctrl+Y")));
    first.addAction(&original);
    auto* other = first.addAction(QStringLiteral("Remove"));
    second.addAction(QStringLiteral("Remove"));
    auto* a = MenuSearch::attach(&first);
    auto* b = MenuSearch::attach(&second);
    a->refresh(); b->refresh();
    a->searchBar()->setText(QStringLiteral(".*"));
    QVERIFY(!a->searchBar()->isRegexEnabled());
    QVERIFY(original.isVisible());
    QVERIFY(!other->isVisible());
    QCOMPARE(b->searchBar()->text(), QString());
    QCOMPARE(b->resultCount(), 1);
    QCOMPARE(original.parent(), &owner);
    QVERIFY(original.isChecked());
    QCOMPARE(original.shortcut(), QKeySequence(QStringLiteral("Ctrl+Y")));
    a->searchBar()->clear();
    QVERIFY(other->isVisible());
    QCOMPARE(a->resultCount(), 2);
}

void TestMaterialMenu::invalidAndBoundedRegexFailClosed()
{
    QMenu menu;
    auto* remove = menu.addAction(QStringLiteral("Remove"));
    QSignalSpy triggered(remove, &QAction::triggered);
    auto* controller = MenuSearch::attach(&menu);
    controller->refresh();
    controller->searchBar()->setRegexEnabled(true);
    for (const auto& pattern : {QStringLiteral("["), QStringLiteral("(a+)+$"), QString(513, QLatin1Char('a'))}) {
        controller->searchBar()->setText(pattern);
        QCOMPARE(controller->resultCount(), 0);
        QVERIFY(!remove->isVisible());
        QTest::keyClick(&menu, Qt::Key_Return);
        QCOMPARE(triggered.count(), 0);
    }
    controller->searchBar()->clear();
    QCOMPARE(controller->resultCount(), 1);
}

void TestMaterialMenu::flagsOnlyRefilterAndUnicode()
{
    QMenu menu;
    auto* item = menu.addAction(QStringLiteral("COPY 香港"));
    auto* controller = MenuSearch::attach(&menu);
    controller->refresh();
    auto* bar = controller->searchBar();
    bar->setRegexEnabled(true);
    bar->setText(QStringLiteral("^copy"));
    QVERIFY(item->isVisible());
    bar->setRegexFlags(QString());
    QVERIFY(!item->isVisible());
    bar->setText(QStringLiteral("\\p{Han}+"));
    QVERIFY(item->isVisible());
    bar->setText(QStringLiteral("(?=香)"));
    QVERIFY(item->isVisible());
}

void TestMaterialMenu::dynamicPopulationAndClear()
{
    QMenu menu;
    int generation = 0;
    connect(&menu, &QMenu::aboutToShow, &menu, [&] {
        menu.clear();
        menu.addAction(QStringLiteral("Generation %1").arg(++generation));
    });
    menu.popup(QPoint(40, 40));
    QTRY_COMPARE(MenuSearch::attach(&menu)->resultCount(), 1);
    QPointer<SearchBar> original = MenuSearch::attach(&menu)->searchBar();
    menu.close();
    menu.popup(QPoint(40, 40));
    QTRY_VERIFY(!original);
    QCOMPARE(menu.findChildren<SearchBar*>().size(), 1);
    QCOMPARE(MenuSearch::attach(&menu)->resultCount(), 1);
    menu.close();
}

void TestMaterialMenu::existingSearchIsNotDuplicated()
{
    QMenu menu;
    auto* search = new SearchBar(&menu);
    search->setIdentity(QStringLiteral("test.menu.existing"), QStringLiteral("Existing search"));
    auto* row = new QWidgetAction(&menu);
    row->setDefaultWidget(search);
    menu.addAction(row);
    menu.addAction(QStringLiteral("Existing action"));
    auto* controller = MenuSearch::attach(&menu);
    controller->refresh(); controller->refresh();
    QCOMPARE(controller->searchBar(), search);
    QCOMPARE(menu.findChildren<SearchBar*>().size(), 1);
}

void TestMaterialMenu::keyboardRequiresExplicitVisibleSelection()
{
    QMenu menu;
    auto* remove = menu.addAction(QStringLiteral("Remove"));
    auto* copy = menu.addAction(QStringLiteral("Copy"));
    QSignalSpy removed(remove, &QAction::triggered);
    QSignalSpy copied(copy, &QAction::triggered);
    menu.popup(QPoint(40, 40));
    auto* controller = MenuSearch::attach(&menu);
    QTRY_VERIFY(controller->searchBar());
    menu.setActiveAction(remove);
    controller->searchBar()->setText(QStringLiteral("Copy"));
    QVERIFY(!menu.activeAction());
    QTest::keyClick(controller->searchBar()->lineEdit(), Qt::Key_Return);
    QCOMPARE(removed.count(), 0); QCOMPARE(copied.count(), 0);
    QTest::keyClick(controller->searchBar()->lineEdit(), Qt::Key_Down);
    QCOMPARE(menu.activeAction(), copy);
    QTest::keyClick(&menu, Qt::Key_Return);
    QCOMPARE(removed.count(), 0); QCOMPARE(copied.count(), 1);
}

void TestMaterialMenu::hiddenAndDisabledActionsRemainSafe()
{
    QMenu menu;
    auto* hidden = menu.addAction(QStringLiteral("Hidden"));
    hidden->setVisible(false);
    auto* disabled = menu.addAction(QStringLiteral("Disabled"));
    disabled->setEnabled(false);
    auto* visible = menu.addAction(QStringLiteral("Visible"));
    menu.popup(QPoint(40, 40));
    auto* controller = MenuSearch::attach(&menu);
    QTRY_VERIFY(controller->searchBar());
    controller->searchBar()->setText(QStringLiteral("Disabled"));
    QVERIFY(!hidden->isVisible()); QVERIFY(disabled->isVisible()); QVERIFY(!disabled->isEnabled());
    QTest::keyClick(controller->searchBar()->lineEdit(), Qt::Key_Down);
    QVERIFY(!menu.activeAction());
    visible->setEnabled(false); // unrelated state change while filtered
    QCoreApplication::processEvents();
    controller->searchBar()->clear();
    QVERIFY(visible->isVisible()); QVERIFY(!visible->isEnabled());
    menu.close();
    QVERIFY(!hidden->isVisible()); QVERIFY(visible->isVisible());
}

void TestMaterialMenu::nestedMenusAndStandardEditorMenus()
{
    QMenu root;
    auto* nested = root.addMenu(QStringLiteral("Nested"));
    nested->addAction(QStringLiteral("Child"));
    root.popup(QPoint(40, 40));
    nested->popup(QPoint(100, 100));
    QTRY_VERIFY(MenuSearch::attach(nested)->searchBar());
    QVERIFY(MenuSearch::attach(&root)->searchBar() != MenuSearch::attach(nested)->searchBar());
    nested->close(); root.close();
    QLineEdit line;
    QPlainTextEdit plain;
    std::unique_ptr<QMenu> lineMenu(line.createStandardContextMenu());
    std::unique_ptr<QMenu> plainMenu(plain.createStandardContextMenu());
    lineMenu->popup(QPoint(40, 40));
    QTRY_VERIFY(MenuSearch::attach(lineMenu.get())->searchBar());
    lineMenu->close();
    plainMenu->popup(QPoint(40, 40));
    QTRY_VERIFY(MenuSearch::attach(plainMenu.get())->searchBar());
    plainMenu->close();
}

void TestMaterialMenu::inlineFullBuilderPreservesExecAndLifetime()
{
    QPointer<SearchBar> field;
    QPointer<RegexBuilder> builder;
    bool opened = false;
    bool stillInsideExec = false;
    bool returned = false;
    {
        QMenu menu;
        menu.addAction(QStringLiteral("Synthetic command"));
        QTimer::singleShot(0, &menu, [&] {
            field = MenuSearch::attach(&menu)->searchBar();
            opened = field && MenuSearch::openBuilderFor(field);
            builder = menu.findChild<RegexBuilder*>();
            stillInsideExec = !returned && menu.isVisible() && builder && builder->isOpen();
            if (builder) {
                builder->setPattern(QStringLiteral("Synthetic"));
                emit builder->patternApplied(builder->pattern());
            }
            menu.close();
        });
        menu.exec(QPoint(40, 40));
        returned = true;
        QVERIFY(opened); QVERIFY(stillInsideExec);
        QVERIFY(field); QCOMPARE(field->text(), QStringLiteral("Synthetic"));
        QVERIFY(field->isRegexEnabled());
    }
    QVERIFY(!field); QVERIFY(!builder);
}

void TestMaterialMenu::localizationAndSessionHistory()
{
    const auto language = Voice::language();
    Voice::setLanguage(Voice::Language::Bilingual);
    QMenu menu;
    menu.addAction(QStringLiteral("Open"));
    menu.popup(QPoint(40, 40));
    auto* controller = MenuSearch::attach(&menu);
    QTRY_VERIFY(controller->searchBar());
    controller->searchBar()->setText(QStringLiteral("Unknown"));
    auto* status = menu.findChild<QLabel*>(QStringLiteral("materialMenuSearchStatus"));
    QVERIFY(status); QVERIFY(status->text().contains(QStringLiteral("沒有")));
    QVERIFY(status->text().contains(QStringLiteral("No matching")));
    QCOMPARE(status->accessibleName(), status->text());
    menu.close();
    QCOMPARE(controller->history(), QStringList{QStringLiteral("Unknown")});
    Voice::setLanguage(language);
}

int main(int argc, char** argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir isolated(QDir::tempPath() + QStringLiteral("/kpxc-menu-XXXXXX"));
    if (!isolated.isValid()) return 2;
    qputenv("KPXC_CONFIG", (isolated.path() + QStringLiteral("/settings.ini")).toUtf8());
    qputenv("KPXC_CONFIG_LOCAL", (isolated.path() + QStringLiteral("/local.ini")).toUtf8());
    qputenv("USERNAME", "kpxc-menu-test");
    qputenv("USER", "kpxc-menu-test");
    QCoreApplication::setOrganizationName(QStringLiteral("KeePassXC-Menu-Tests"));
    QApplication application(argc, argv);
    MenuSearch::install(&application);
    TestMaterialMenu test;
    return QTest::qExec(&test, argc, argv);
}
