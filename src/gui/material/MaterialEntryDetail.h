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

#ifndef KEEPASSXC_MATERIALENTRYDETAIL_H
#define KEEPASSXC_MATERIALENTRYDETAIL_H

#include "MaterialTheme.h"

#include <QList>
#include <QMargins>
#include <QScrollArea>
#include <QString>

class QAbstractButton;
class QLabel;
class QTimer;
class QVBoxLayout;

namespace Material
{
    class FilledButton;
    class IconButton;
    class TonalButton;

    /** One row of the attachment list: display name and an already formatted size. */
    struct EntryAttachment
    {
        QString name;
        QString size;
    };

    /**
     * Everything the detail pane draws for one entry.
     *
     * Plain values only. The pane never sees an Entry, a Database or the
     * clipboard, so the same struct can be filled from EntryPreviewWidget, from
     * a report row or from a test.
     */
    struct EntryDetailData
    {
        QString title;
        QString url;
        QString symbol; // Material Symbols name for the header tile
        QString username;
        QString password;
        QString totpCode; // empty leaves the TOTP row in its placeholder state
        int totpPeriod = 30; // seconds per step, drives the countdown ring
        QString notes; // empty hides the notes section
        Health health = Health::Unknown;
        QString strengthLabel;
        int strengthPercent = 0; // 0..100, the filled width of the strength meter
        QList<EntryAttachment> attachments; // empty hides the attachments section
        QString historySummary; // empty hides the history row
        bool favourite = false;
    };

    /**
     * The 392px detail pane on the right of the vault.
     *
     * A surfaceContainerLow column behind a hairline left border: the entry
     * header, the Auto-Type / Edit / delete actions, the credentials card with
     * its strength meter and TOTP countdown, then the notes, attachments and
     * history sections.
     *
     * The pane is purely presentational. It holds no Entry pointer and performs
     * no database work; every action leaves through a signal and every value
     * arrives through setEntryData().
     */
    class EntryDetail : public QScrollArea
    {
        Q_OBJECT

    public:
        explicit EntryDetail(QWidget* parent = nullptr);
        ~EntryDetail() override;

        const EntryDetailData& entryData() const;
        void setEntryData(const EntryDetailData& entry);

        /** Blank the pane and stop the countdown; no credentials are kept. */
        void clear();

        bool isPasswordVisible() const;

    public slots:
        void setPasswordVisible(bool visible);

    signals:
        /** @p field is one of "username", "password" or "totp". */
        void copyRequested(const QString& field);
        void autoTypeRequested();
        void editRequested();
        void deleteRequested();
        void favouriteToggled(bool favourite);
        void historyRequested();
        void attachmentActivated(const QString& name);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        // Pane internals, defined in MaterialEntryDetail.cpp.
        class CountdownRing;
        class StrengthMeter;
        class ValueLabel;

        void buildUi();
        QWidget* buildHeader();
        QWidget* buildActions();
        QWidget* buildCredentials();
        QWidget* buildNotes();
        QWidget* buildAttachments();
        QWidget* buildHistory();

        QLabel* createCaption(const QString& text);
        QLabel* createOverline(const QString& text);
        IconButton* createRowButton(const QString& symbol, const QString& tooltip);
        /** Wrap @p child in a container carrying its design margins. */
        QWidget* inset(QWidget* child, const QMargins& margins);

        void applyTheme();
        void updateContent();
        void updateAttachments();
        void updatePasswordDisplay();
        void updateFavouriteState();
        void updateTotp();
        void syncTotpTimer();

        EntryDetailData m_data;
        bool m_hasEntry = false;
        bool m_passwordVisible = false;

        QWidget* m_content = nullptr;
        QLabel* m_symbolLabel = nullptr;
        ValueLabel* m_titleLabel = nullptr;
        ValueLabel* m_urlLabel = nullptr;
        IconButton* m_favouriteButton = nullptr;
        FilledButton* m_autoTypeButton = nullptr;
        TonalButton* m_editButton = nullptr;
        QAbstractButton* m_deleteButton = nullptr;
        ValueLabel* m_usernameLabel = nullptr;
        ValueLabel* m_passwordLabel = nullptr;
        StrengthMeter* m_strengthMeter = nullptr;
        QLabel* m_strengthLabel = nullptr;
        IconButton* m_revealButton = nullptr;
        IconButton* m_copyPasswordButton = nullptr;
        IconButton* m_copyUsernameButton = nullptr;
        IconButton* m_copyTotpButton = nullptr;
        QWidget* m_totpDivider = nullptr;
        QWidget* m_totpRow = nullptr;
        CountdownRing* m_totpRing = nullptr;
        ValueLabel* m_totpLabel = nullptr;
        QWidget* m_notesSection = nullptr;
        QLabel* m_notesLabel = nullptr;
        QWidget* m_attachmentsSection = nullptr;
        QWidget* m_attachmentsList = nullptr;
        QVBoxLayout* m_attachmentsLayout = nullptr;
        QWidget* m_historySection = nullptr;
        QLabel* m_historyLabel = nullptr;
        QTimer* m_totpTimer = nullptr;

        QList<QLabel*> m_captions;
        QList<QLabel*> m_overlines;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALENTRYDETAIL_H
