#include "MaterialTabOverflow.h"

#include "MaterialButtons.h"
#include "MaterialRegexSafety.h"
#include "MaterialSearchBar.h"
#include "MaterialVoice.h"
#include "MaterialTheme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QScrollArea>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace Material
{
    TabOverflow::TabOverflow(QWidget* parent)
        : Overlay(parent)
    {
        m_sheet = new QWidget;
        m_sheet->setObjectName(QStringLiteral("materialTabOverflowSheet"));
        auto* root = new QVBoxLayout(m_sheet);
        root->setContentsMargins(20, 20, 20, 20);
        root->setSpacing(12);

        auto* title = new QLabel(tr("Open database tabs"), m_sheet);
        title->setText(Voice::say(QStringLiteral("search.tabs")));
        connect(Voice::notifier(), &Voice::Notifier::changed, title, [title] {
            title->setText(Voice::say(QStringLiteral("search.tabs")));
        });
        title->setFont(theme()->font(TypeRole::HeadlineSmall));
        root->addWidget(title);

        m_search = new SearchBar(SearchBar::Variant::Surface, m_sheet);
        m_search->setIdentity(QStringLiteral("tabs.open"), tr("Open database tabs search"));
        m_search->setCopyKeys(QStringLiteral("search.tabs"), QStringLiteral("search.tabs"));
        m_search->setPlaceholder(tr("Search every open database tab"));
        connect(m_search, &SearchBar::textChanged, this, [this] { rebuild(); });
        connect(m_search, &SearchBar::regexToggled, this, [this] { rebuild(); });
        connect(m_search, &SearchBar::regexFlagsChanged, this, [this] { rebuild(); });
        connect(Voice::notifier(), &Voice::Notifier::changed, this, [this] { if (isOpen()) rebuild(); });
        root->addWidget(m_search);

        auto* list = new QWidget;
        m_rows = new QVBoxLayout(list);
        m_rows->setContentsMargins(0, 0, 0, 0);
        m_rows->setSpacing(6);
        m_rows->addStretch(1);
        m_scroll = new QScrollArea;
        m_scroll->setWidgetResizable(true);
        m_scroll->setFrameShape(QFrame::NoFrame);
        m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scroll->setWidget(list);
        m_scroll->setMinimumHeight(260);
        root->addWidget(m_scroll, 1);

        m_empty = new QLabel(Voice::say(QStringLiteral("search.no-tabs")), m_sheet);
        m_empty->setAlignment(Qt::AlignCenter);
        m_empty->setWordWrap(true);
        m_empty->hide();
        root->addWidget(m_empty);

        connect(this, &Overlay::closed, this, &TabOverflow::clearSearch);
        setSheetWidth(560);
        setSheetTopMargin(72);
        setSheetWidget(m_sheet);
    }

    void TabOverflow::setTabs(const QList<TabDescriptor>& tabs,
                              const QString& currentRuntimeId,
                              const QSet<QString>& hiddenRuntimeIds)
    {
        m_tabs = tabs;
        m_current = currentRuntimeId;
        m_hidden = hiddenRuntimeIds;
        if (isOpen()) rebuild();
    }

    void TabOverflow::aboutToOpen()
    {
        rebuild();
        m_search->lineEdit()->setFocus(Qt::PopupFocusReason);
    }

    void TabOverflow::clearSearch()
    {
        m_search->clear();
        m_search->setRegexEnabled(false);
        m_search->setRegexFlags(QStringLiteral("i"));
        m_tabs.clear();
        clearRows();
    }

    void TabOverflow::clearRows()
    {
        while (m_rows->count() > 0) {
            auto* item = m_rows->takeAt(0);
            if (auto* widget = item->widget()) widget->deleteLater();
            delete item;
        }
    }

    void TabOverflow::rebuild()
    {
        const QString query = m_search->text();
        if (m_search->isRegexEnabled() && !query.isEmpty()) {
            const auto validation = runBounded(query, optionsForFlags(m_search->regexFlags()), QString());
            if (!validation.compiled || validation.blocked || validation.timedOut) {
                m_empty->setText(Voice::say(QStringLiteral("search.unsafe")));
                m_empty->show();
                return;
            }
        }
        m_empty->setText(Voice::say(QStringLiteral("search.no-tabs")));
        QList<TabDescriptor> matchingTabs;
        for (const auto& tab : m_tabs) {
            bool matched = query.isEmpty() || tab.label.contains(query, Qt::CaseInsensitive);
            if (m_search->isRegexEnabled() && !query.isEmpty()) {
                const auto run = runBounded(query, optionsForFlags(m_search->regexFlags()), tab.label);
                if (!run.compiled || run.blocked || run.timedOut) {
                    m_empty->setText(Voice::say(QStringLiteral("search.unsafe")));
                    m_empty->show();
                    return;
                }
                matched = !run.matches.isEmpty();
            }
            if (matched) matchingTabs.append(tab);
        }
        clearRows();
        int matches = 0;
        for (const auto& tab : matchingTabs) {

            auto* row = new QWidget;
            row->setObjectName(QStringLiteral("materialTabResult"));
            row->setProperty("runtimeId", tab.runtimeId);
            auto* layout = new QHBoxLayout(row);
            layout->setContentsMargins(8, 4, 8, 4);
            layout->setSpacing(8);
            QString state;
            if (tab.runtimeId == m_current) state += tr("Current");
            if (tab.pinned) state += (state.isEmpty() ? QString() : QStringLiteral(" · ")) + tr("Pinned");
            if (m_hidden.contains(tab.runtimeId)) state += (state.isEmpty() ? QString() : QStringLiteral(" · ")) + tr("In overflow");
            auto* activate = new TextButton(tab.symbol, state.isEmpty() ? tab.label : tr("%1 — %2").arg(tab.label, state));
            activate->setAccessibleName(tab.label);
            connect(activate, &QAbstractButton::clicked, this, [this, id = tab.runtimeId] {
                emit tabActivated(id);
                closeOverlay();
            });
            layout->addWidget(activate, 1);
            auto* pin = new IconButton(tab.pinned ? QStringLiteral("keep_off") : QStringLiteral("keep"));
            pin->setToolTip(tab.pinned ? tr("Unpin tab") : tr("Pin tab"));
            pin->setAccessibleName(pin->toolTip());
            connect(pin, &QAbstractButton::clicked, this, [this, id = tab.runtimeId, pinned = tab.pinned] {
                emit tabPinRequested(id, !pinned);
            });
            layout->addWidget(pin);
            m_rows->addWidget(row);
            ++matches;
        }
        m_rows->addStretch(1);
        m_scroll->setVisible(matches > 0);
        m_empty->setVisible(matches == 0);
    }
} // namespace Material
