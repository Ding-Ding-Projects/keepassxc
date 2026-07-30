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

#ifndef KEEPASSXC_MATERIALDIALOG_H
#define KEEPASSXC_MATERIALDIALOG_H

#include "MaterialOverlay.h"

#include <QString>

class QHBoxLayout;
class QLabel;
class QVBoxLayout;

namespace Material
{
    class ButtonBase;

    /**
     * The confirm and message sheet.
     *
     * A ready-made Overlay sheet: an optional glyph, a headline, supporting
     * text and a right-aligned row of actions. The primary action is a filled
     * button, the rest are text buttons, and every action closes the sheet
     * before its own clicked() runs.
     *
     * Dialogs are not modal event loops. Connect to the button returned by
     * addAction(), or to accepted() / rejected(), and call openOverlay().
     */
    class Dialog : public Overlay
    {
        Q_OBJECT

    public:
        explicit Dialog(QWidget* parent = nullptr);
        ~Dialog() override;

        void setHeadline(const QString& text);
        QString headline() const;

        void setSupportingText(const QString& text);
        QString supportingText() const;

        /** Leading glyph above the headline; empty hides it. */
        void setSymbol(const QString& symbol);
        QString symbol() const;

        /**
         * Append an action. Primary actions are filled and emit accepted(),
         * the others are text buttons and emit rejected(). The dialog keeps
         * ownership of the returned button.
         */
        ButtonBase* addAction(const QString& label, bool isPrimary = false);
        void clearActions();

        /**
         * A Cancel / @p confirmLabel sheet parented to @p parent, error-tinted
         * when @p destructive. It deletes itself once closed, so connect to
         * accepted() before calling openOverlay().
         */
        static Dialog* confirm(QWidget* parent,
                               const QString& headline,
                               const QString& supportingText,
                               const QString& confirmLabel,
                               bool destructive = false);

    signals:
        void accepted();
        void rejected();

    private:
        void applyTheme();

        QWidget* m_sheet = nullptr;
        QVBoxLayout* m_sheetLayout = nullptr;
        QHBoxLayout* m_actionLayout = nullptr;
        QLabel* m_symbolLabel = nullptr;
        QLabel* m_headlineLabel = nullptr;
        QLabel* m_supportingLabel = nullptr;
        QString m_symbol;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALDIALOG_H
