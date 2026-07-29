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
#include "MaterialTheme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
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

        m_titleLabel = new QLabel(this);
        m_subtitleLabel = new QLabel(this);
        m_subtitleLabel->hide();
        // The title column absorbs the free space, so the two labels have to be
        // free to shrink below their text width - they elide instead.
        for (auto* label : {m_titleLabel, m_subtitleLabel}) {
            label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            label->setTextInteractionFlags(Qt::NoTextInteraction);
        }

        auto* titleColumn = new QVBoxLayout;
        titleColumn->setContentsMargins(0, 0, 0, 0);
        titleColumn->setSpacing(0);
        titleColumn->addStretch();
        titleColumn->addWidget(m_titleLabel);
        titleColumn->addWidget(m_subtitleLabel);
        titleColumn->addStretch();
        layout->addLayout(titleColumn, 1);

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
        for (const auto& action : actions) {
            auto* button = new IconButton(QString::fromLatin1(action.symbol), this);
            button->setSymbolSize(ActionGlyphSize);
            button->setToolTip(action.tip);
            button->setAccessibleName(action.tip);
            connect(button, &QAbstractButton::clicked, this, action.signal);
            layout->addWidget(button);
            *action.button = button;
        }

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
        updateLabels();
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
        const int available = m_titleLabel->width();
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
