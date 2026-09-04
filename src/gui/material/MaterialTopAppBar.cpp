/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MaterialTopAppBar.h"

#include "MaterialButtons.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QAction>
#include <QMenu>

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QFontMetrics>
#include <QPainter>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
        constexpr int TitleMaxWidth = 230;
        constexpr int SearchMaxWidth = 720;
        constexpr int LeftPadding = 20;
        constexpr int RightPadding = 12;
        constexpr int Spacing = 8;
        constexpr int SaveGlyphSize = 20;
        constexpr int ActionGlyphSize = 22;
    } // namespace

    TopAppBar::TopAppBar(QWidget* parent)
        : QWidget(parent)
    {
        setFixedHeight(Layout::AppBarHeight);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(LeftPadding, 0, RightPadding, 0);
        layout->setSpacing(Spacing);
        m_layout = layout;

        m_titleLabel = new QLabel(this);
        m_subtitleLabel = new QLabel(this);
        m_subtitleLabel->hide();
        // The title column absorbs the free space, so the two labels have to be
        // free to shrink below their text width - they elide instead.
        for (auto* label : {m_titleLabel, m_subtitleLabel}) {
            label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            label->setTextInteractionFlags(Qt::NoTextInteraction);
        }

        // The title column takes what it needs up to the reference's 230 px
        // when a search field shares the bar, and the free space otherwise.
        m_titleColumn = new QWidget(this);
        auto* titleColumn = new QVBoxLayout(m_titleColumn);
        titleColumn->setContentsMargins(0, 0, 0, 0);
        titleColumn->setSpacing(0);
        titleColumn->addStretch();
        titleColumn->addWidget(m_titleLabel);
        titleColumn->addWidget(m_subtitleLabel);
        titleColumn->addStretch();
        layout->addWidget(m_titleColumn, 1);
        layout->addStretch(0);

        m_saveButton = new TonalButton(QStringLiteral("save"), tr("Save"), this);
        m_saveButton->setFixedHeight(Layout::ButtonHeight);
        m_saveButton->setSymbolSize(SaveGlyphSize);
        m_saveButton->setToolTip(tr("Save the active database"));
        connect(m_saveButton, &QAbstractButton::clicked, this, &TopAppBar::saveRequested);
        layout->addWidget(m_saveButton);

        struct Action
        {
            IconButton** button;
            const char* symbol;
            QString tip;
            void (TopAppBar::*signal)();
        };
        const Action actions[] = {
            {&m_paletteButton, "bolt", tr("All actions"), &TopAppBar::paletteRequested},
            {&m_generatorButton, "casino", tr("Password generator"), &TopAppBar::generatorRequested},
            {&m_regexButton, "regular_expression", tr("Regular expression builder"), &TopAppBar::regexRequested},
            {&m_notificationsButton, "notifications", tr("Notifications"), &TopAppBar::notificationsRequested},
        };
        // The overflow menu holds whichever trailing actions the bar cannot
        // fit; its button is the last thing in the row and only shows then.
        m_overflowMenu = new QMenu(this);
        m_overflowMenu->setObjectName(QStringLiteral("topAppBarOverflowMenu"));
        m_overflowButton = new IconButton(QStringLiteral("more_vert"), this);
        m_overflowButton->setObjectName(QStringLiteral("topAppBarOverflow"));
        m_overflowButton->setFixedSize(Layout::IconButtonSize, Layout::IconButtonSize);
        m_overflowButton->setSymbolSize(ActionGlyphSize);
        m_overflowButton->setToolTip(tr("More actions"));
        m_overflowButton->setAccessibleName(tr("More actions"));
        m_overflowButton->hide();
        connect(m_overflowButton, &QAbstractButton::clicked, this, [this] {
            m_overflowMenu->popup(m_overflowButton->mapToGlobal(QPoint(0, m_overflowButton->height())));
        });

        for (const auto& action : actions) {
            auto* button = new IconButton(QString::fromLatin1(action.symbol), this);
            // ButtonBase fixes a minimum width from its label metrics on
            // construction, which the layout would otherwise honour over the
            // icon button's square size hint.
            button->setFixedSize(Layout::IconButtonSize, Layout::IconButtonSize);
            button->setSymbolSize(ActionGlyphSize);
            button->setToolTip(action.tip);
            button->setAccessibleName(action.tip);
            connect(button, &QAbstractButton::clicked, this, action.signal);
            layout->addWidget(button);
            *action.button = button;

            // The same command, as a menu entry, for when the button is folded.
            auto* menuAction = new QAction(Icons::symbol(QString::fromLatin1(action.symbol)), action.tip, this);
            menuAction->setObjectName(QStringLiteral("topAppBarOverflow_") + QString::fromLatin1(action.symbol));
            connect(menuAction, &QAction::triggered, this, action.signal);
            m_actions.append({button, menuAction, action.tip});
        }
        layout->addWidget(m_overflowButton);

        applyTheme();
        connect(theme(), &Theme::changed, this, [this] { applyTheme(); });
    }

    TopAppBar::~TopAppBar() = default;

    QString TopAppBar::title() const
    {
        return m_title;
    }

    void TopAppBar::setTitle(const QString& title)
    {
        if (title == m_title) {
            return;
        }
        m_title = title;
        setAccessibleName(title);
        updateLabels();
    }

    QString TopAppBar::subtitle() const
    {
        return m_subtitle;
    }

    void TopAppBar::setSubtitle(const QString& subtitle)
    {
        if (subtitle == m_subtitle) {
            return;
        }
        m_subtitle = subtitle;
        updateLabels();
    }

    bool TopAppBar::isSaveEnabled() const
    {
        return m_saveButton->isEnabled();
    }

    void TopAppBar::setSaveEnabled(bool enabled)
    {
        m_saveButton->setEnabled(enabled);
    }

    int TopAppBar::notificationCount() const
    {
        return m_notificationsButton->badgeCount();
    }

    void TopAppBar::setNotificationCount(int count)
    {
        count = qMax(0, count);
        if (count == m_notificationsButton->badgeCount()) {
            return;
        }
        m_notificationsButton->setBadgeCount(count);
        m_notificationsButton->setAccessibleName(count > 0 ? tr("Notifications (%1 unread)").arg(count)
                                                           : tr("Notifications"));
    }

    void TopAppBar::setSearchWidget(QWidget* search)
    {
        if (m_search == search) {
            return;
        }
        if (m_search) {
            m_layout->removeWidget(m_search);
            m_search->setParent(nullptr);
        }
        m_search = search;
        if (m_search) {
            m_search->setParent(this);
            m_search->setMaximumWidth(SearchMaxWidth);
            m_search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            // After the title column, before the stretch and the actions.
            m_layout->insertWidget(1, m_search, 2);
            m_search->setVisible(m_searchVisible);
        }
        m_titleColumn->setMaximumWidth(m_search && m_searchVisible ? TitleMaxWidth : QWIDGETSIZE_MAX);
        m_layout->setStretch(0, m_search && m_searchVisible ? 0 : 1);
        updateLabels();
    }

    QWidget* TopAppBar::searchWidget() const
    {
        return m_search;
    }

    void TopAppBar::setSearchVisible(bool visible)
    {
        m_searchVisible = visible;
        if (m_search) {
            m_search->setVisible(visible);
        }
        m_titleColumn->setMaximumWidth(m_search && visible ? TitleMaxWidth : QWIDGETSIZE_MAX);
        m_layout->setStretch(0, m_search && visible ? 0 : 1);
        relayoutActions();
        updateLabels();
    }

    QSize TopAppBar::sizeHint() const
    {
        return {QWidget::sizeHint().width(), Layout::AppBarHeight};
    }

    QSize TopAppBar::minimumSizeHint() const
    {
        return {QWidget::minimumSizeHint().width(), Layout::AppBarHeight};
    }

    void TopAppBar::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.fillRect(rect(), theme()->color(Role::SurfaceContainer));
        painter.fillRect(QRect(0, height() - 1, width(), 1), theme()->color(Role::OutlineVariant));
    }

    void TopAppBar::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        relayoutActions();
        updateLabels();
    }

    QStringList TopAppBar::overflowedActions() const
    {
        QStringList names;
        for (int i = m_actions.size() - m_folded; i < m_actions.size(); ++i) {
            names.append(m_actions.at(i).name);
        }
        return names;
    }

    QAbstractButton* TopAppBar::overflowButton() const
    {
        return m_overflowButton;
    }

    QMenu* TopAppBar::overflowMenu() const
    {
        return m_overflowMenu;
    }

    void TopAppBar::relayoutActions()
    {
        if (m_relayouting || m_actions.isEmpty()) {
            return;
        }
        m_relayouting = true;

        // What the row needs besides the action buttons, at their minimums:
        // the paddings, the title column, the search field and Save.
        const QMargins margins = m_layout->contentsMargins();
        int fixed = margins.left() + margins.right();
        int items = 0;
        auto count = [&](QWidget* widget, int minimum) {
            if (widget && !widget->isHidden()) {
                fixed += minimum;
                ++items;
            }
        };
        count(m_titleColumn, m_search && m_searchVisible ? qMin(TitleMaxWidth, m_titleColumn->minimumWidth()) : 120);
        if (m_search && m_searchVisible) {
            count(m_search, qMax(m_search->minimumWidth(), m_search->minimumSizeHint().width()));
        }
        count(m_saveButton, m_saveButton->sizeHint().width());

        const int total = m_actions.size();
        int folded = 0;
        for (; folded < total; ++folded) {
            const int shown = total - folded;
            const int overflow = folded > 0 ? 1 : 0;
            const int buttons = shown + overflow;
            const int need = fixed + buttons * Layout::IconButtonSize + (items + buttons - 1) * Spacing;
            if (need <= width()) {
                break;
            }
        }
        // Folding a single action gains nothing: the overflow button takes
        // its place. Fold two or none.
        if (folded == 1 && total >= 2) {
            folded = 2;
        }

        if (folded != m_folded) {
            m_folded = folded;
            m_overflowMenu->clear();
            for (int i = 0; i < total; ++i) {
                const bool inMenu = i >= total - folded;
                m_actions.at(i).button->setVisible(!inMenu);
                if (inMenu) {
                    m_overflowMenu->addAction(m_actions.at(i).action);
                }
            }
            m_overflowButton->setVisible(folded > 0);
        }
        m_relayouting = false;
    }

    void TopAppBar::applyTheme()
    {
        m_titleLabel->setFont(theme()->font(TypeRole::TitleLarge));

        QFont subtitleFont = theme()->font(TypeRole::LabelSmall);
        subtitleFont.setWeight(QFont::Normal);
        m_subtitleLabel->setFont(subtitleFont);

        QPalette titlePalette = m_titleLabel->palette();
        titlePalette.setColor(QPalette::WindowText, theme()->color(Role::OnSurface));
        m_titleLabel->setPalette(titlePalette);

        QPalette subtitlePalette = m_subtitleLabel->palette();
        subtitlePalette.setColor(QPalette::WindowText, theme()->color(Role::OnSurfaceVariant));
        m_subtitleLabel->setPalette(subtitlePalette);

        updateLabels();
        update();
    }

    void TopAppBar::updateLabels()
    {
        // With a search field beside it the column is sized to its own text,
        // capped at the reference's 230 px; alone it takes the free space.
        if (m_titleColumn) {
            if (m_search && m_searchVisible) {
                const int wanted = qMax(QFontMetrics(m_titleLabel->font()).horizontalAdvance(m_title),
                                        QFontMetrics(m_subtitleLabel->font()).horizontalAdvance(m_subtitle));
                m_titleColumn->setMinimumWidth(qMin(TitleMaxWidth, wanted));
                m_titleColumn->setMaximumWidth(TitleMaxWidth);
            } else {
                m_titleColumn->setMinimumWidth(0);
                m_titleColumn->setMaximumWidth(QWIDGETSIZE_MAX);
            }
        }
        const int available = m_titleColumn ? m_titleColumn->width() : m_titleLabel->width();
        if (available > 0) {
            m_titleLabel->setText(m_titleLabel->fontMetrics().elidedText(m_title, Qt::ElideRight, available));
            m_subtitleLabel->setText(m_subtitleLabel->fontMetrics().elidedText(m_subtitle, Qt::ElideRight, available));
        } else {
            m_titleLabel->setText(m_title);
            m_subtitleLabel->setText(m_subtitle);
        }
        m_subtitleLabel->setVisible(!m_subtitle.isEmpty());
    }

} // namespace Material
