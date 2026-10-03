#include "MaterialMenu.h"

#include "MaterialRegexBuilder.h"
#include "MaterialRegexSafety.h"
#include "MaterialSearchBar.h"
#include "MaterialSelect.h"
#include "MaterialTheme.h"
#include "MaterialVoice.h"
#include "gui/Clipboard.h"

#include <QActionEvent>
#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QScreen>
#include <QScopedValueRollback>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidgetAction>

namespace Material
{
    namespace
    {
        QString commandLabel(QAction* action)
        {
            // Search command copy only, never document, field or clipboard contents.
            QString label = action->text().section(QLatin1Char('\t'), 0, 0);
            label.replace(QStringLiteral("&&"), QString(QChar(0x1f)));
            label.remove(QLatin1Char('&'));
            label.replace(QChar(0x1f), QLatin1Char('&'));
            return label;
        }

        SearchBar* directSearch(QMenu* menu)
        {
            for (auto* action : menu->actions()) {
                auto* widgetAction = qobject_cast<QWidgetAction*>(action);
                if (!widgetAction || !widgetAction->defaultWidget()) continue;
                QWidget* widget = widgetAction->defaultWidget();
                if (auto* bar = qobject_cast<SearchBar*>(widget)) return bar;
                // Do not borrow a search field from a nested picker or submenu.
                for (auto* bar : widget->findChildren<SearchBar*>()) {
                    QWidget* parent = bar->parentWidget();
                    while (parent && parent != menu && !qobject_cast<QMenu*>(parent)) parent = parent->parentWidget();
                    if (parent == menu) return bar;
                }
            }
            return nullptr;
        }

        class MenuInstaller final : public QObject
        {
        public:
            explicit MenuInstaller(QApplication* application) : QObject(application) {}
            bool eventFilter(QObject* watched, QEvent* event) override
            {
                // Show follows aboutToShow, including dynamic clear/repopulate callbacks.
                if (event->type() == QEvent::Show) {
                    if (auto* menu = qobject_cast<QMenu*>(watched)) MenuSearch::attach(menu)->refresh();
                }
                return false;
            }
        };
    }

    void MenuSearch::install(QApplication* application)
    {
        if (!application || application->property("materialMenuSearchInstalled").toBool()) return;
        application->setProperty("materialMenuSearchInstalled", true);
        application->installEventFilter(new MenuInstaller(application));
    }

    MenuSearch* MenuSearch::attach(QMenu* menu)
    {
        if (!menu) return nullptr;
        auto* controller = menu->findChild<MenuSearch*>(QStringLiteral("materialMenuSearchController"),
                                                       Qt::FindDirectChildrenOnly);
        if (!controller) controller = new MenuSearch(menu);
        return controller;
    }

    MenuSearch::MenuSearch(QMenu* menu) : QObject(menu), m_menu(menu)
    {
        setObjectName(QStringLiteral("materialMenuSearchController"));
        menu->installEventFilter(this);
        connect(menu, &QMenu::aboutToHide, this, [this] {
            ++m_popupGeneration;
            if (m_builderOpen) {
                m_builderOpen = false;
                if (m_builder) m_builder->closeOverlay();
                if (m_builderAction) m_builderAction->setVisible(false);
                restoreBuilderWidth();
            }
            remember();
            restore();
            const QPointer<QWidget> opener = m_opener;
            QTimer::singleShot(0, qApp, [opener] {
                if (opener && opener->isVisible() && !QApplication::activePopupWidget())
                    opener->setFocus(Qt::PopupFocusReason);
            });
        });
    }

    SearchBar* MenuSearch::searchBar() const { return m_search; }
    QStringList MenuSearch::history() const { return m_history; }
    int MenuSearch::resultCount() const { return m_resultCount; }

    void MenuSearch::prepare()
    {
        if (!m_menu) return;
        QScopedValueRollback<bool> changing(m_changing, true);
        // clear() may have destroyed our QWidgetActions since the previous popup.
        if (!m_search) {
            m_search = directSearch(m_menu);
            m_ownsSearch = !m_search;
            if (m_ownsSearch) {
                m_search = new SearchBar(SearchBar::Variant::Prominent, m_menu);
                m_search->setObjectName(QStringLiteral("materialMenuSearch"));
                m_search->setMinimumWidth(280);
                m_search->lineEdit()->setMaxLength(RegexLimits::PatternChars);
                m_search->setIdentity(QStringLiteral("menu.") + QUuid::createUuid().toString(QUuid::WithoutBraces),
                                      Voice::say(QStringLiteral("menu.search")));
                m_searchAction = new QWidgetAction(m_menu);
                m_searchAction->setDefaultWidget(m_search);
                m_menu->insertAction(m_menu->actions().value(0), m_searchAction);
            }
            m_search->lineEdit()->installEventFilter(this);
            connect(m_search, &SearchBar::textChanged, this, [this] { filter(); });
            connect(m_search, &SearchBar::regexToggled, this, [this] { filter(); });
            connect(m_search, &SearchBar::regexFlagsChanged, this, [this] { filter(); });
            // A Select owns a list inside its QWidgetAction, including keyboard
            // behavior. Observe its real results instead of treating it as commands.
            m_select = qobject_cast<Select*>(m_menu->parentWidget());
            if (m_select && m_select->searchBar() != m_search) m_select.clear();
            if (m_select) {
                connect(m_select, &Select::filteredChoicesChanged, this, [this] { filter(); });
            }
        }
        if (!m_statusAction) {
            m_status = new QLabel(m_menu);
            m_status->setObjectName(QStringLiteral("materialMenuSearchStatus"));
            m_status->setWordWrap(true);
            m_status->setMargin(8);
            m_status->setTextInteractionFlags(Qt::NoTextInteraction);
            m_status->setFont(theme()->font(TypeRole::BodySmall));
            m_statusAction = new QWidgetAction(m_menu);
            m_statusAction->setDefaultWidget(m_status);
            m_statusAction->setEnabled(false);
            m_menu->addAction(m_statusAction);
        }
        if (m_ownsSearch) {
            m_search->setPlaceholder(Voice::say(QStringLiteral("menu.search")));
            m_search->lineEdit()->setAccessibleName(Voice::say(QStringLiteral("menu.search")));
        }
        m_prepared = true;
    }

    void MenuSearch::refresh()
    {
        if (!m_menu || m_changing) return;
        if (!m_builderOpen) m_opener = QApplication::focusWidget();
        prepare();
        if (m_ownsSearch) {
            for (auto* action : m_menu->actions()) {
                if (action == m_searchAction || action == m_statusAction || action == m_builderAction
                    || m_originalVisibility.contains(action)) continue;
                m_originalVisibility.insert(action, action->isVisible());
                connect(action, &QObject::destroyed, this, [this, action] {
                    m_originalVisibility.remove(action);
                    m_matches.remove(action);
                });
            }
        }
        filter();
        m_menu->adjustSize();
        QTimer::singleShot(0, this, [this] {
            if (m_menu && m_menu->isVisible() && m_search && !m_builderOpen) {
                m_menu->setActiveAction(nullptr);
                m_search->lineEdit()->setFocus(Qt::PopupFocusReason);
            }
        });
    }

    bool MenuSearch::isCommand(QAction* action) const
    {
        return action && action != m_searchAction && action != m_statusAction && action != m_builderAction && !action->isSeparator()
               && !qobject_cast<QWidgetAction*>(action);
    }

    bool MenuSearch::canActivate(QAction* action) const
    {
        return m_menu && m_menu->actions().contains(action) && isCommand(action) && action->isVisible()
               && action->isEnabled() && (!m_ownsSearch || m_matches.contains(action));
    }

    void MenuSearch::filter()
    {
        if (!m_menu || !m_search || !m_prepared || m_changing) return;
        QScopedValueRollback<bool> changing(m_changing, true);
        // No default/stale selection survives an edit, including a flags-only edit.
        m_menu->setActiveAction(nullptr);
        m_matches.clear();
        m_resultCount = 0;
        if (m_select) {
            auto* list = m_select->listWidget();
            for (int row = 0; row < list->count(); ++row) {
                if (!list->isRowHidden(row)) ++m_resultCount;
            }
            updateStatus(m_select->filterError());
            return;
        }
        QString error;
        const QString query = m_search->text();
        QRegularExpression expression;
        if (m_ownsSearch && m_search->isRegexEnabled() && !query.isEmpty()) {
            if (query.size() > RegexLimits::PatternChars || !riskReport(query).isEmpty()) {
                error = Voice::say(QStringLiteral("menu.pattern-limit"), Voice::Category::Error);
            } else {
                // PCRE2 enforces these limits inside a match, unlike a timer checked afterwards.
                expression = QRegularExpression(QStringLiteral("(*LIMIT_MATCH=10000)(*LIMIT_DEPTH=128)") + query,
                                                optionsForFlags(m_search->regexFlags()));
                if (!expression.isValid())
                    error = Voice::say(QStringLiteral("menu.invalid-pattern"), Voice::Category::Error);
            }
        }
        QElapsedTimer timer;
        timer.start();
        int examined = 0;
        for (auto* action : m_menu->actions()) {
            if (!isCommand(action)) continue;
            bool visible = action->isVisible();
            if (m_ownsSearch) {
                visible = m_originalVisibility.value(action, action->isVisible());
                if (visible && !query.isEmpty()) {
                    const QString label = commandLabel(action);
                    if (++examined > 1024 || label.size() > 2048 || timer.elapsed() > RegexLimits::BudgetMs) {
                        error = Voice::say(QStringLiteral("menu.result-limit"), Voice::Category::Error);
                    }
                    visible = false;
                    if (error.isEmpty()) {
                        if (m_search->isRegexEnabled()) {
                            const auto match = expression.match(label);
                            if (!match.isValid())
                                error = Voice::say(QStringLiteral("menu.result-limit"), Voice::Category::Error);
                            else
                                visible = match.hasMatch();
                        } else {
                            visible = label.contains(query, Qt::CaseInsensitive);
                        }
                    }
                }
                action->setVisible(visible);
            }
            if (visible) { m_matches.insert(action); ++m_resultCount; }
        }
        if (m_ownsSearch && !error.isEmpty()) {
            // A partial result is not safe to activate after a resource-limit failure.
            for (auto* action : m_menu->actions()) if (isCommand(action)) action->setVisible(false);
            m_matches.clear();
            m_resultCount = 0;
        }
        if (m_ownsSearch) {
            // Keep separators only between visible groups, without moving any commands.
            QAction* pending = nullptr;
            bool haveCommand = false;
            for (auto* action : m_menu->actions()) {
                if (action->isSeparator()) {
                    action->setVisible(false);
                    if (haveCommand && m_originalVisibility.value(action)) pending = action;
                } else if (isCommand(action) && action->isVisible()) {
                    if (pending) pending->setVisible(true);
                    pending = nullptr;
                    haveCommand = true;
                }
            }
        }
        updateStatus(error);
    }

    void MenuSearch::updateStatus(const QString& error)
    {
        if (!m_status) return;
        const QString message = !error.isEmpty() ? error : m_resultCount == 0
            ? Voice::say(QStringLiteral("menu.no-matches"))
            : Voice::say(QStringLiteral("menu.match-count"), {{QStringLiteral("count"), m_resultCount}}, Voice::Category::Info);
        m_status->setText(message);
        m_status->setAccessibleName(message);
        if (m_search) m_search->lineEdit()->setAccessibleDescription(message);
    }

    void MenuSearch::restore()
    {
        if (!m_menu || m_changing) return;
        QScopedValueRollback<bool> changing(m_changing, true);
        // Removed actions can still belong to another menu or toolbar. Keep weak
        // pointers while restoring, since action callbacks may destroy siblings.
        QList<QPair<QPointer<QAction>, bool>> originals;
        for (auto it = m_originalVisibility.cbegin(); it != m_originalVisibility.cend(); ++it)
            originals.append({it.key(), it.value()});
        m_originalVisibility.clear();
        m_matches.clear();
        m_prepared = false;
        for (const auto& original : originals)
            if (original.first) original.first->setVisible(original.second);
    }

    void MenuSearch::remember()
    {
        if (!m_search || m_search->text().isEmpty()) return;
        // Bounded, in-memory only. No settings, database, logs or history repository writes.
        m_history.removeAll(m_search->text());
        m_history.prepend(m_search->text());
        while (m_history.size() > 20) m_history.removeLast();
    }

    void MenuSearch::moveSelection(int direction)
    {
        QList<QAction*> actions;
        for (auto* action : m_menu->actions()) if (canActivate(action)) actions.append(action);
        if (actions.isEmpty()) { m_menu->setActiveAction(nullptr); return; }
        const int current = actions.indexOf(m_menu->activeAction());
        const int next = current < 0 ? (direction > 0 ? 0 : actions.size() - 1)
                                    : (current + direction + actions.size()) % actions.size();
        m_menu->setActiveAction(actions[next]);
        m_menu->setFocus(Qt::PopupFocusReason);
    }

    bool MenuSearch::eventFilter(QObject* watched, QEvent* event)
    {
        if (!m_menu || m_changing) return false;
        if (watched == m_menu && event->type() == QEvent::ActionChanged && m_prepared && m_ownsSearch) {
            auto* action = static_cast<QActionEvent*>(event)->action();
            // An enabled/checkable/text change must not make a filtered action
            // permanently hidden. Only an actual visibility transition updates it.
            const bool expected = action->isSeparator() ? action->isVisible() : m_matches.contains(action);
            if (m_originalVisibility.contains(action) && action->isVisible() != expected)
                m_originalVisibility[action] = action->isVisible();
            const auto generation = m_popupGeneration;
            QTimer::singleShot(0, this, [this, generation] {
                if (generation == m_popupGeneration && m_menu && m_menu->isVisible()) filter();
            });
        }
        if (watched == m_menu && event->type() == QEvent::ActionRemoved) {
            auto* action = static_cast<QActionEvent*>(event)->action();
            if (m_originalVisibility.contains(action)) {
                const bool visible = m_originalVisibility.take(action);
                m_matches.remove(action);
                QScopedValueRollback<bool> changing(m_changing, true);
                action->setVisible(visible);
            }
        }
        if (watched == m_menu && (event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved)
            && m_menu->isVisible()) {
            const auto generation = m_popupGeneration;
            QTimer::singleShot(0, this, [this, generation] {
                if (generation == m_popupGeneration && m_menu && m_menu->isVisible()) refresh();
            });
        }
        if (event->type() != QEvent::KeyPress || !m_search) return false;
        auto* key = static_cast<QKeyEvent*>(event);
        if (m_builderOpen) {
            if (watched == m_menu && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) return true;
            return false;
        }
        if (m_select) return false; // The borrowed list owns arrows, Enter and clear-first Escape.
        if (key->key() == Qt::Key_Escape) { m_menu->close(); return true; }
        if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) {
            moveSelection(key->key() == Qt::Key_Down ? 1 : -1);
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            // The field itself never executes a command. Explicit arrow selection is required.
            if (watched != m_menu || !canActivate(m_menu->activeAction())) return true;
            return false; // QMenu owns triggering and the exec() return value.
        }
        if (watched == m_menu && !key->text().isEmpty()
            && !(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
            m_search->lineEdit()->setFocus(Qt::OtherFocusReason);
            QApplication::sendEvent(m_search->lineEdit(), key);
            return true;
        }
        return false;
    }

    bool MenuSearch::openBuilderFor(SearchBar* bar)
    {
        if (!bar) return false;
        for (QWidget* parent = bar->parentWidget(); parent; parent = parent->parentWidget()) {
            if (auto* menu = qobject_cast<QMenu*>(parent)) {
                auto* controller = attach(menu);
                controller->prepare();
                if (controller->searchBar() != bar) return false;
                controller->showBuilder();
                return true;
            }
        }
        return false;
    }

    void MenuSearch::showBuilder()
    {
        if (!m_menu || !m_search || m_builderOpen) return;
        m_popupMinimumWidth = m_menu->minimumWidth();
        m_popupMaximumWidth = m_menu->maximumWidth();
        const QSize baseSize = m_menu->sizeHint();
        m_builderOpen = true;
        if (!m_builderContainer) {
            m_builderContainer = new QWidget(m_menu);
            m_builderContainer->setObjectName(QStringLiteral("materialMenuRegexPanel"));
            m_builderContainer->setAccessibleName(Voice::say(QStringLiteral("menu.builder")));
            m_builder = new RegexBuilder(m_builderContainer);
            m_builder->setDialect(QStringLiteral("qt"));
            m_builder->setSheetTopMargin(0);
            m_builderAction = new QWidgetAction(m_menu);
            m_builderAction->setDefaultWidget(m_builderContainer);
            // Publish the row only after its final size is known, so QMenu
            // invalidates its cached action rectangles when it becomes visible.
            m_builderAction->setVisible(false);
            const auto actions = m_menu->actions();
            QAction* searchAction = m_searchAction;
            if (!searchAction) {
                for (auto* action : actions) {
                    auto* widget = qobject_cast<QWidgetAction*>(action);
                    if (widget && widget->defaultWidget() && widget->defaultWidget()->isAncestorOf(m_search))
                        searchAction = action;
                    if (widget && widget->defaultWidget() == m_search) searchAction = action;
                }
            }
            m_menu->insertAction(actions.value(actions.indexOf(searchAction) + 1), m_builderAction);
            connect(m_builder, &Overlay::closed, this, [this] {
                if (!m_builderOpen || !m_menu) return;
                m_builderOpen = false;
                m_builderAction->setVisible(false);
                restoreBuilderWidth();
                filter();
                m_menu->adjustSize();
                if (m_search) m_search->lineEdit()->setFocus(Qt::PopupFocusReason);
            });
            connect(m_builder, &RegexBuilder::patternApplied, this, [this](const QString& pattern) {
                if (!m_search || !m_builder) return;
                m_search->setRegexFlags(m_builder->flags());
                m_search->setRegexEnabled(true);
                m_search->setText(pattern);
                remember();
                filter();
            });
            connect(m_builder, &RegexBuilder::patternCopied, this, [](const QString& pattern) {
                clipboard()->setText(pattern);
            });
        }
        m_builder->setPattern(m_search->text());
        m_builder->setFlags(m_search->regexFlags());
        const QRect available = m_menu->screen()->availableGeometry();
        // Select fixes the ordinary popup width. Release that constraint only
        // while its inline workbench is visible, and reserve the other rows.
        m_menu->setMinimumWidth(m_popupMinimumWidth);
        m_menu->setMaximumWidth(qMax(m_popupMinimumWidth, available.width()));
        const QSize size(qMin(1064, qMax(1, available.width() - 32)),
                         qMin(740, qMax(1, available.height() - baseSize.height() - 32)));
        m_builderContainer->setFixedSize(size);
        m_builder->setGeometry(m_builderContainer->rect());
        m_builderAction->setVisible(true);
        m_builderContainer->show();
        m_builder->openOverlay();
        // The workbench is an adjacent child row in the SAME popup. A separate
        // top-level dialog would close QMenu::exec and destroy stack-local menus.
        m_menu->adjustSize();
        m_menu->move(qBound(available.left(), m_menu->x(), qMax(available.left(), available.right() - m_menu->width() + 1)),
                     qBound(available.top(), m_menu->y(), qMax(available.top(), available.bottom() - m_menu->height() + 1)));
    }

    void MenuSearch::restoreBuilderWidth()
    {
        if (!m_menu) return;
        m_menu->setMinimumWidth(m_popupMinimumWidth);
        m_menu->setMaximumWidth(m_popupMaximumWidth);
    }
}
