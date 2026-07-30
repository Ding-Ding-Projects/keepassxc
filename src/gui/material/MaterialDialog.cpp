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

#include "MaterialDialog.h"

#include "MaterialButtons.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
        constexpr int DialogWidth = 460;
        constexpr int SheetPaddingSide = 26;
        constexpr int SheetPaddingTop = 24;
        constexpr int SheetPaddingBottom = 20;
        constexpr int BadgeSize = 48;
        constexpr int BadgeSymbolSize = 24;
        // The design leaves 14px under the badge; the sheet layout's own 8px
        // spacing supplies the rest, so the badge reserves the remainder.
        constexpr int BadgeGap = 6;

        /** The rounded-28 panel the dialog content sits on. */
        class DialogPanel : public QWidget
        {
        public:
            explicit DialogPanel(QWidget* parent = nullptr)
                : QWidget(parent)
            {
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                paintSurface(&painter, rect(), Shape::ExtraLarge, theme()->color(Role::SurfaceContainerLowest));
            }
        };

        void styleLabel(QLabel* label, TypeRole type, Role color)
        {
            label->setFont(theme()->font(type));
            label->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(theme()->hex(color)));
        }
    } // namespace

    /**
     * The 48px round badge the symbol sits in: a container fill with the
     * matching on-container glyph, error-tinted on a destructive sheet.
     */
    class SymbolBadge : public QWidget
    {
    public:
        explicit SymbolBadge(QWidget* parent = nullptr)
            : QWidget(parent)
        {
        }

        void setSymbol(const QString& symbol, Role container, Role content)
        {
            m_symbol = symbol;
            m_container = container;
            m_content = content;
            update();
        }

        QSize sizeHint() const override
        {
            return {BadgeSize, BadgeSize + BadgeGap};
        }

        QSize minimumSizeHint() const override
        {
            return sizeHint();
        }

    protected:
        void paintEvent(QPaintEvent* event) override
        {
            Q_UNUSED(event)
            if (m_symbol.isEmpty()) {
                return;
            }

            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            paintSurface(&painter, QRect(0, 0, BadgeSize, BadgeSize), Shape::Full, theme()->color(m_container));

            const int inset = (BadgeSize - BadgeSymbolSize) / 2;
            painter.drawPixmap(QRect(inset, inset, BadgeSymbolSize, BadgeSymbolSize),
                               Icons::pixmap(m_symbol, BadgeSymbolSize, theme()->color(m_content)));
        }

    private:
        QString m_symbol;
        Role m_container = Role::PrimaryContainer;
        Role m_content = Role::OnPrimaryContainer;
    };

    Dialog::Dialog(QWidget* parent)
        : Overlay(parent)
    {
        m_sheet = new DialogPanel;

        m_sheetLayout = new QVBoxLayout(m_sheet);
        m_sheetLayout->setContentsMargins(SheetPaddingSide, SheetPaddingTop, SheetPaddingSide, SheetPaddingBottom);
        m_sheetLayout->setSpacing(8);

        m_symbolBadge = new SymbolBadge(m_sheet);
        m_symbolBadge->hide();
        m_sheetLayout->addWidget(m_symbolBadge, 0, Qt::AlignLeft);

        // The text rows appear as soon as they are given something to show.
        m_headlineLabel = new QLabel(m_sheet);
        m_headlineLabel->setWordWrap(true);
        m_headlineLabel->hide();
        m_sheetLayout->addWidget(m_headlineLabel);

        m_supportingLabel = new QLabel(m_sheet);
        m_supportingLabel->setWordWrap(true);
        m_supportingLabel->hide();
        m_sheetLayout->addWidget(m_supportingLabel);

        m_actionLayout = new QHBoxLayout;
        m_actionLayout->setContentsMargins(0, 14, 0, 0);
        m_actionLayout->setSpacing(8);
        m_actionLayout->addStretch(1);
        m_sheetLayout->addLayout(m_actionLayout);

        setSheetWidth(DialogWidth);
        setSheetWidget(m_sheet);

        applyTheme();
        connect(theme(), &Theme::changed, this, &Dialog::applyTheme);
    }

    Dialog::~Dialog() = default;

    void Dialog::setHeadline(const QString& text)
    {
        m_headlineLabel->setText(text);
        m_headlineLabel->setVisible(!text.isEmpty());
    }

    QString Dialog::headline() const
    {
        return m_headlineLabel->text();
    }

    void Dialog::setSupportingText(const QString& text)
    {
        m_supportingLabel->setText(text);
        m_supportingLabel->setVisible(!text.isEmpty());
    }

    QString Dialog::supportingText() const
    {
        return m_supportingLabel->text();
    }

    void Dialog::setSymbol(const QString& symbol)
    {
        m_symbol = symbol;
        m_symbolBadge->setVisible(!symbol.isEmpty());
        applyTheme();
    }

    QString Dialog::symbol() const
    {
        return m_symbol;
    }

    ButtonBase* Dialog::addAction(const QString& label, bool isPrimary)
    {
        ButtonBase* button;
        if (isPrimary) {
            button = new FilledButton(QString(), label, m_sheet);
        } else {
            button = new TextButton(QString(), label, m_sheet);
        }
        m_actionLayout->addWidget(button);

        // Connected first, so the sheet is already closing when the caller's
        // own clicked() handler runs.
        connect(button, &ButtonBase::clicked, this, [this, isPrimary] {
            closeOverlay();
            if (isPrimary) {
                emit accepted();
            } else {
                emit rejected();
            }
        });
        return button;
    }

    void Dialog::clearActions()
    {
        // Index 0 is the stretch that keeps the row right-aligned.
        while (m_actionLayout->count() > 1) {
            QLayoutItem* item = m_actionLayout->takeAt(1);
            delete item->widget();
            delete item;
        }
    }

    Dialog* Dialog::confirm(QWidget* parent,
                            const QString& headline,
                            const QString& supportingText,
                            const QString& confirmLabel,
                            bool destructive)
    {
        auto* dialog = new Dialog(parent);
        dialog->setProperty("destructive", destructive);
        dialog->setSymbol(destructive ? QStringLiteral("delete_forever") : QStringLiteral("help"));
        dialog->setHeadline(headline);
        dialog->setSupportingText(supportingText);

        dialog->addAction(tr("Cancel"));
        ButtonBase* confirmButton = dialog->addAction(confirmLabel, true);
        if (destructive) {
            confirmButton->setRoles(Role::Error, Role::OnError);
        }

        connect(dialog, &Overlay::closed, dialog, &QObject::deleteLater);
        return dialog;
    }

    void Dialog::applyTheme()
    {
        // confirm() marks destructive sheets with a dynamic property so the
        // accent survives a theme change.
        const bool destructive = property("destructive").toBool();
        m_symbolBadge->setSymbol(m_symbol,
                                 destructive ? Role::ErrorContainer : Role::PrimaryContainer,
                                 destructive ? Role::OnErrorContainer : Role::OnPrimaryContainer);

        styleLabel(m_headlineLabel, TypeRole::TitleLarge, Role::OnSurface);
        styleLabel(m_supportingLabel, TypeRole::BodyMedium, Role::OnSurfaceVariant);
        m_sheet->update();
    }

} // namespace Material
