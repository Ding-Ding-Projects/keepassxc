#include "MaterialChoicePopup.h"

#include "MaterialControls.h"
#include "MaterialRegexBuilder.h"
#include "MaterialRegexSafety.h"
#include "MaterialSearchBar.h"
#include "MaterialTheme.h"
#include "MaterialVoice.h"
#include "gui/Clipboard.h"

#include <QAccessibleWidget>
#include <QApplication>
#include <QElapsedTimer>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QScreen>
#include <QScopedValueRollback>
#include <QSet>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

namespace Material
{
    class ChoiceProxy final : public QSortFilterProxyModel
    {
    public:
        explicit ChoiceProxy(QObject* parent) : QSortFilterProxyModel(parent) { setDynamicSortFilter(false); }
        void apply(const QModelIndex& root, int column, const QSet<QPersistentModelIndex>& matches)
        {
            m_root = root;
            m_column = column;
            m_matches = matches;
            invalidateFilter();
        }
    protected:
        bool filterAcceptsRow(int row, const QModelIndex& parent) const override
        {
            // Keep ancestors so a nonzero combo root remains addressable. Only
            // the immediate children of that root are presented by the view.
            return parent != m_root || m_matches.contains(sourceModel()->index(row, m_column, parent));
        }
    private:
        QPersistentModelIndex m_root;
        int m_column = 0;
        QSet<QPersistentModelIndex> m_matches;
    };

    namespace
    {
        class ChoiceDelegate final : public QStyledItemDelegate
        {
        public:
            explicit ChoiceDelegate(QObject* parent) : QStyledItemDelegate(parent) {}
            QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
            {
                auto size = QStyledItemDelegate::sizeHint(option, index);
                size.setHeight(qMax(48, size.height() + 16));
                return size;
            }
            void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
            {
                QStyleOptionViewItem styled(option);
                initStyleOption(&styled, index);
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                const bool selected = option.state.testFlag(QStyle::State_Selected);
                const QRect row = option.rect.adjusted(4, 2, -4, -2);
                painter->setPen(Qt::NoPen);
                painter->setBrush(theme()->color(selected ? Role::SecondaryContainer : Role::SurfaceContainer));
                painter->drawRoundedRect(row, Shape::Medium, Shape::Medium);
                if (option.state.testFlag(QStyle::State_HasFocus)) {
                    painter->setPen(QPen(theme()->color(Role::Primary), 2));
                    painter->setBrush(Qt::NoBrush);
                    painter->drawRoundedRect(row.adjusted(1, 1, -1, -1), Shape::Medium, Shape::Medium);
                }
                painter->restore();
                styled.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);
                styled.backgroundBrush = Qt::NoBrush;
                styled.rect = option.rect.adjusted(12, 0, -12, 0);
                styled.palette.setColor(QPalette::Text, theme()->color(selected ? Role::OnSecondaryContainer : Role::OnSurface));
                QStyledItemDelegate::paint(painter, styled, index);
            }
        };

        class AccessibleCombo final : public QAccessibleWidget
        {
        public:
            explicit AccessibleCombo(ComboBox* combo) : QAccessibleWidget(combo, QAccessible::ComboBox) {}
            ComboBox* combo() const { return qobject_cast<ComboBox*>(object()); }
            QAccessible::State state() const override
            {
                auto result = QAccessibleWidget::state();
                if (auto* owner = combo()) {
                    auto* popup = owner->findChild<ChoicePopup*>();
                    result.hasPopup = result.expandable = true;
                    result.expanded = popup && popup->isVisible();
                    result.collapsed = !result.expanded;
                    result.editable = owner->isEditable();
                }
                return result;
            }
            QString text(QAccessible::Text kind) const override
            {
                if (kind == QAccessible::Value && combo()) return combo()->currentText();
                return QAccessibleWidget::text(kind);
            }
            QList<std::pair<QAccessibleInterface*, QAccessible::Relation>> relations(QAccessible::Relation match) const override
            {
                auto result = QAccessibleWidget::relations(match);
                if ((match & QAccessible::Controlled) && combo()) {
                    if (auto* popup = combo()->findChild<ChoicePopup*>())
                        result.append({QAccessible::queryAccessibleInterface(popup), QAccessible::Controlled});
                }
                return result;
            }
            QStringList actionNames() const override
            {
                return {QAccessibleActionInterface::showMenuAction(), QAccessibleActionInterface::setFocusAction()};
            }
            void doAction(const QString& action) override
            {
                if (!combo() || !combo()->isEnabled()) return;
                if (action == QAccessibleActionInterface::showMenuAction()) {
                    if (state().expanded) combo()->hidePopup(); else combo()->showPopup();
                } else QAccessibleWidget::doAction(action);
            }
        };

        class AccessiblePopup final : public QAccessibleWidget
        {
        public:
            explicit AccessiblePopup(ChoicePopup* popup) : QAccessibleWidget(popup, QAccessible::Pane) {}
            QList<std::pair<QAccessibleInterface*, QAccessible::Relation>> relations(QAccessible::Relation match) const override
            {
                auto result = QAccessibleWidget::relations(match);
                if ((match & QAccessible::Controller) && widget()->parentWidget())
                    result.append({QAccessible::queryAccessibleInterface(widget()->parentWidget()), QAccessible::Controller});
                return result;
            }
        };

        QAccessibleInterface* accessibleFactory(const QString&, QObject* object)
        {
            if (auto* combo = qobject_cast<ComboBox*>(object)) return new AccessibleCombo(combo);
            if (auto* popup = qobject_cast<ChoicePopup*>(object)) return new AccessiblePopup(popup);
            return nullptr;
        }
    }

    void ChoicePopup::installAccessibility()
    {
        static const bool installed = [] { QAccessible::installFactory(accessibleFactory); return true; }();
        Q_UNUSED(installed)
    }

    ChoicePopup::ChoicePopup(ComboBox* owner)
        : QWidget(owner, Qt::Popup | Qt::FramelessWindowHint), m_owner(owner)
    {
        setObjectName(QStringLiteral("materialChoicePopup"));
        setFocusPolicy(Qt::StrongFocus);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(8);
        layout->setSizeConstraint(QLayout::SetNoConstraint);
        m_search = new SearchBar(SearchBar::Variant::Prominent, this);
        m_search->setObjectName(QStringLiteral("materialChoiceSearch"));
        // This transient search intentionally does not register with the global
        // builder router. The adjacent workbench belongs only to this popup.
        layout->addWidget(m_search);
        m_status = new QLabel(this);
        m_status->setObjectName(QStringLiteral("materialChoiceStatus"));
        m_status->setWordWrap(true);
        m_status->setTextInteractionFlags(Qt::NoTextInteraction);
        layout->addWidget(m_status);
        m_proxy = new ChoiceProxy(this);
        m_list = new QListView(this);
        m_list->setObjectName(QStringLiteral("materialChoiceList"));
        m_list->setModel(m_proxy);
        m_list->setItemDelegate(new ChoiceDelegate(m_list));
        m_list->setSelectionMode(QAbstractItemView::SingleSelection);
        m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_list->setFrameShape(QFrame::NoFrame);
        m_list->setUniformItemSizes(false);
        layout->addWidget(m_list, 1);
        m_search->lineEdit()->installEventFilter(this);
        m_list->installEventFilter(this);
        owner->installEventFilter(this);
        installEventFilter(this);
        connect(m_search, &SearchBar::textChanged, this, &ChoicePopup::refresh);
        connect(m_search, &SearchBar::regexToggled, this, &ChoicePopup::refresh);
        connect(m_search, &SearchBar::regexFlagsChanged, this, &ChoicePopup::refresh);
        connect(m_search, &SearchBar::builderRequested, this, &ChoicePopup::showBuilder);
        connect(m_list, &QListView::clicked, this, &ChoicePopup::activateIndex);
        m_bindingTimer = new QTimer(this);
        m_bindingTimer->setInterval(30);
        connect(m_bindingTimer, &QTimer::timeout, this, [this] {
            // QComboBox::setModel/root/column are not virtual. Observe changes
            // even when callers use QComboBox*, and recheck synchronously before
            // any activation so no stale source index can be committed.
            if (!bindingIsCurrent()) hide();
        });
        connect(theme(), &Theme::changed, this, &ChoicePopup::updateTheme);
        connect(Voice::notifier(), &Voice::Notifier::changed, this, &ChoicePopup::updateCopy);
        updateTheme(); updateCopy();
    }

    ChoicePopup::~ChoicePopup()
    {
        for (const auto& connection : m_connections) disconnect(connection);
    }

    bool ChoicePopup::bindingIsCurrent() const
    {
        return m_owner && m_source && m_owner->isVisible() && m_owner->isEnabled()
            && m_owner->model() == m_source && m_owner->modelColumn() == m_column
            && (!m_hadRoot || m_root.isValid()) && m_owner->rootModelIndex() == m_root;
    }

    void ChoicePopup::open()
    {
        if (isVisible() || !m_owner || !m_owner->isVisible() || !m_owner->isEnabled()) return;
        m_source = m_owner->model();
        if (!m_source) return;
        m_root = m_owner->rootModelIndex(); m_hadRoot = m_root.isValid(); m_column = m_owner->modelColumn();
        m_proxy->setSourceModel(m_source);
        m_list->setModelColumn(m_column);
        for (const auto& connection : m_connections) disconnect(connection);
        m_connections.clear();
        m_connections << connect(m_source, &QObject::destroyed, this, [this] { hide(); });
        m_connections << connect(m_source, &QAbstractItemModel::dataChanged, this, &ChoicePopup::refresh, Qt::QueuedConnection);
        m_connections << connect(m_source, &QAbstractItemModel::rowsInserted, this, &ChoicePopup::refresh, Qt::QueuedConnection);
        m_connections << connect(m_source, &QAbstractItemModel::rowsRemoved, this, &ChoicePopup::refresh, Qt::QueuedConnection);
        m_connections << connect(m_source, &QAbstractItemModel::rowsMoved, this, &ChoicePopup::refresh, Qt::QueuedConnection);
        m_connections << connect(m_source, &QAbstractItemModel::columnsInserted, this, &ChoicePopup::refresh, Qt::QueuedConnection);
        m_connections << connect(m_source, &QAbstractItemModel::columnsRemoved, this, &ChoicePopup::refresh, Qt::QueuedConnection);
        m_connections << connect(m_source, &QAbstractItemModel::columnsMoved, this, &ChoicePopup::refresh, Qt::QueuedConnection);
        m_connections << connect(m_source, &QAbstractItemModel::layoutChanged, this, &ChoicePopup::refresh, Qt::QueuedConnection);
        m_connections << connect(m_source, &QAbstractItemModel::modelReset, this, &ChoicePopup::refresh, Qt::QueuedConnection);
        refresh(); positionPopup(); show(); raise();
        m_search->lineEdit()->setFocus(Qt::PopupFocusReason);
        m_bindingTimer->start(); announceState();
    }

    void ChoicePopup::refresh()
    {
        if (m_refreshing || m_closing || !m_source) return;
        if (!bindingIsCurrent()) { hide(); return; }
        QScopedValueRollback<bool> refreshing(m_refreshing, true);
        QPersistentModelIndex candidate(m_proxy->mapToSource(m_list->currentIndex()));
        if (!candidate.isValid()) candidate = m_source->index(m_owner->currentIndex(), m_column, m_root);
        m_error.clear();
        QSet<QPersistentModelIndex> matches;
        const QString query = m_search->text();
        QRegularExpression expression;
        const bool regex = m_search->isRegexEnabled() && !query.isEmpty();
        if (query.size() > RegexLimits::PatternChars || (regex && !riskReport(query).isEmpty()))
            m_error = Voice::say(QStringLiteral("choice.pattern-limit"), Voice::Category::Error);
        if (regex && m_error.isEmpty()) {
            expression = QRegularExpression(QStringLiteral("(*LIMIT_MATCH=10000)(*LIMIT_DEPTH=128)") + query,
                                            optionsForFlags(m_search->regexFlags()));
            if (!expression.isValid()) m_error = Voice::say(QStringLiteral("choice.invalid-pattern"), Voice::Category::Error);
        }
        QElapsedTimer timer; timer.start();
        const int count = m_source->rowCount(m_root);
        if (count > 1024) m_error = Voice::say(QStringLiteral("choice.result-limit"), Voice::Category::Error);
        for (int row = 0; row < count && m_error.isEmpty(); ++row) {
            const QModelIndex index = m_source->index(row, m_column, m_root);
            const QString label = index.data(Qt::DisplayRole).toString();
            if (label.size() > 2048 || timer.elapsed() > RegexLimits::BudgetMs) {
                m_error = Voice::say(QStringLiteral("choice.result-limit"), Voice::Category::Error); break;
            }
            bool accepted = query.isEmpty();
            if (!accepted && regex) {
                const auto match = expression.match(label);
                if (!match.isValid()) m_error = Voice::say(QStringLiteral("choice.result-limit"), Voice::Category::Error);
                accepted = match.hasMatch();
            } else if (!accepted) accepted = label.contains(query, Qt::CaseInsensitive);
            if (accepted) matches.insert(index);
        }
        if (!m_error.isEmpty()) matches.clear();
        m_proxy->apply(m_root, m_column, matches);
        m_list->setRootIndex(m_proxy->mapFromSource(m_root));
        // QListView clamps its column against the current root. A child table
        // can have more columns than the top-level model, so set this last.
        m_list->setModelColumn(m_column);
        m_list->setCurrentIndex({});
        m_list->clearSelection();
        // Keep the current choice on opening and preserve a still-matching
        // keyboard candidate. Filtering never changes the combo's value.
        const QModelIndex mapped = m_proxy->mapFromSource(candidate);
        if (mapped.isValid() && mapped.flags().testFlag(Qt::ItemIsEnabled) && mapped.flags().testFlag(Qt::ItemIsSelectable))
            m_list->setCurrentIndex(mapped);
        else moveSelection(1);
        const QString status = !m_error.isEmpty() ? m_error : matches.isEmpty()
            ? Voice::say(QStringLiteral("choice.no-matches"))
            : Voice::say(QStringLiteral("choice.match-count"), {{QStringLiteral("count"), matches.size()}}, Voice::Category::Info);
        m_status->setText(status); m_status->setAccessibleName(status);
        m_search->lineEdit()->setAccessibleDescription(status);
        QAccessibleEvent changed(m_status, QAccessible::NameChanged);
        QAccessible::updateAccessibility(&changed);
    }

    void ChoicePopup::moveSelection(int direction)
    {
        const int count = m_proxy->rowCount(m_list->rootIndex());
        const int start = m_list->currentIndex().isValid() ? m_list->currentIndex().row() : direction > 0 ? -1 : 0;
        for (int offset = 1; offset <= count; ++offset) {
            const int row = (start + direction * offset + count) % count;
            const auto index = m_proxy->index(row, m_column, m_list->rootIndex());
            if (index.flags().testFlag(Qt::ItemIsEnabled) && index.flags().testFlag(Qt::ItemIsSelectable)) {
                m_list->setCurrentIndex(index); m_list->scrollTo(index); return;
            }
        }
        m_list->setCurrentIndex({});
    }

    void ChoicePopup::activateCurrent()
    {
        activateIndex(m_list->currentIndex());
    }

    void ChoicePopup::activateIndex(const QModelIndex& index)
    {
        if (!bindingIsCurrent()) { hide(); return; }
        const QPersistentModelIndex selected(m_proxy->mapToSource(index));
        if (!selected.isValid() || selected.parent() != m_root || selected.column() != m_column
            || !selected.flags().testFlag(Qt::ItemIsEnabled) || !selected.flags().testFlag(Qt::ItemIsSelectable)) return;
        // dataChanged can be queued behind this key/click. Re-evaluate before
        // committing the captured index, never fall back to another current row.
        refresh();
        if (!m_error.isEmpty() || !selected.isValid() || !m_proxy->mapFromSource(selected).isValid()) return;
        // The receiver may synchronously delete the combo and this popup.
        emit choiceActivated(selected);
    }

    bool ChoicePopup::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == m_owner && (event->type() == QEvent::Hide || event->type() == QEvent::EnabledChange)) {
            if (!m_owner || !m_owner->isVisible() || !m_owner->isEnabled()) hide();
        }
        if (event->type() == QEvent::KeyPress && watched != m_owner) {
            auto* key = static_cast<QKeyEvent*>(event);
            // Unhandled editor keys propagate through the inline workbench.
            // They must not also commit or navigate the underlying choices.
            if (m_builder && m_builder->isOpen()) {
                if (key->key() == Qt::Key_Escape) { m_builder->closeOverlay(); return true; }
                if (watched == this && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter
                                       || key->key() == Qt::Key_Down || key->key() == Qt::Key_Up)) return true;
            }
            if (key->key() == Qt::Key_Escape) { hide(); return true; }
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) { activateCurrent(); return true; }
            if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) {
                moveSelection(key->key() == Qt::Key_Down ? 1 : -1); return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void ChoicePopup::showBuilder()
    {
        if (!bindingIsCurrent() || !isVisible()) return;
        if (!m_builder) {
            m_builderContainer = new QWidget(this);
            m_builderContainer->setMinimumSize(1, 1);
            static_cast<QVBoxLayout*>(layout())->addWidget(m_builderContainer, 3);
            m_builder = new RegexBuilder(m_builderContainer);
            m_builder->setDialect(QStringLiteral("qt"));
            m_builder->setSheetTopMargin(0);
            connect(m_builder, &Overlay::closed, this, [this] {
                if (m_closing) return;
                m_builderContainer->hide();
                if (isVisible()) { positionPopup(); m_search->lineEdit()->setFocus(Qt::PopupFocusReason); }
            });
            connect(m_builder, &RegexBuilder::patternApplied, this, [this](const QString& pattern) {
                const QSignalBlocker blocker(m_search);
                m_search->setRegexFlags(m_builder->flags());
                m_search->setRegexEnabled(true); m_search->setText(pattern);
                refresh();
            });
            connect(m_builder, &RegexBuilder::patternCopied, this, [](const QString& pattern) { clipboard()->setText(pattern); });
        }
        m_builder->setSampleText(QString());
        m_builder->setPattern(m_search->text()); m_builder->setFlags(m_search->regexFlags());
        m_builderContainer->show(); positionPopup();
        layout()->activate(); m_builder->setGeometry(m_builderContainer->rect()); m_builder->openOverlay();
    }

    void ChoicePopup::positionPopup()
    {
        if (!m_owner) return;
        const QRect available = m_owner->screen()->availableGeometry().adjusted(8, 8, -8, -8);
        const bool builder = m_builderContainer && !m_builderContainer->isHidden();
        const QSize desired(builder ? 1080 : qMax(440, m_owner->width()), builder ? 880 : 420);
        resize(desired.boundedTo(available.size()));
        const QPoint anchor = m_owner->mapToGlobal(QPoint(0, m_owner->height()));
        move(qBound(available.left(), anchor.x(), available.right() - width() + 1),
             qBound(available.top(), anchor.y(), available.bottom() - height() + 1));
    }

    void ChoicePopup::hideEvent(QHideEvent* event)
    {
        m_closing = true;
        m_bindingTimer->stop();
        for (const auto& connection : m_connections) disconnect(connection);
        m_connections.clear();
        if (m_builder) { m_builder->closeOverlay(); m_builder->hide(); m_builder->setSampleText(QString()); }
        if (m_builderContainer) m_builderContainer->hide();
        const QSignalBlocker blocker(m_search);
        m_search->clear(); m_search->setRegexEnabled(false); m_search->setRegexFlags(QStringLiteral("i"));
        m_proxy->setSourceModel(nullptr); m_source.clear(); m_root = QPersistentModelIndex();
        QWidget::hideEvent(event); announceState();
        const QPointer<ComboBox> owner = m_owner;
        QTimer::singleShot(0, qApp, [owner] {
            if (owner && owner->isVisible() && !QApplication::activePopupWidget() && !QApplication::focusWidget())
                owner->setFocus(Qt::PopupFocusReason);
        });
        m_closing = false;
        emit dismissed();
    }

    void ChoicePopup::announceState()
    {
        if (!m_owner) return;
        QAccessible::State changed; changed.expanded = changed.collapsed = true;
        QAccessibleStateChangeEvent event(m_owner, changed); QAccessible::updateAccessibility(&event);
    }

    void ChoicePopup::updateCopy()
    {
        setAccessibleName(Voice::say(QStringLiteral("choice.popup")));
        m_search->setPlaceholder(Voice::say(QStringLiteral("choice.search")));
        m_search->lineEdit()->setAccessibleName(Voice::say(QStringLiteral("choice.search")));
        m_list->setAccessibleName(Voice::say(QStringLiteral("choice.results")));
        m_search->setToolTip(Voice::say(QStringLiteral("choice.flags")));
        m_list->setAccessibleDescription(Voice::say(QStringLiteral("choice.flags")));
        refresh();
    }

    void ChoicePopup::updateTheme()
    {
        auto colors = palette();
        colors.setColor(QPalette::Base, theme()->color(Role::SurfaceContainer));
        colors.setColor(QPalette::Text, theme()->color(Role::OnSurface));
        colors.setColor(QPalette::WindowText, theme()->color(Role::OnSurface));
        colors.setColor(QPalette::Highlight, theme()->color(Role::SecondaryContainer));
        colors.setColor(QPalette::HighlightedText, theme()->color(Role::OnSecondaryContainer));
        setPalette(colors); m_list->setPalette(colors);
        m_list->setFont(theme()->font(TypeRole::BodyLarge));
        m_status->setFont(theme()->font(TypeRole::BodySmall));
        update(); m_list->viewport()->update();
    }

    void ChoicePopup::paintEvent(QPaintEvent*)
    {
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(theme()->color(Role::OutlineVariant), 1));
        painter.setBrush(theme()->color(Role::SurfaceContainer));
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), Shape::Medium, Shape::Medium);
    }
}
