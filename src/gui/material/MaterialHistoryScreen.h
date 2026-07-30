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

#ifndef KEEPASSXC_MATERIALHISTORYSCREEN_H
#define KEEPASSXC_MATERIALHISTORYSCREEN_H

#include "MaterialChip.h"
#include "MaterialScreen.h"

#include <QString>
#include <QVector>

class QVBoxLayout;

namespace Material
{
    /**
     * One entry of the version history.
     *
     * @p tint picks the family the 40px glyph circle is drawn in, so a deletion
     * can read as an error and a restore as healthy without the screen knowing
     * anything about what a revision means.
     */
    struct Revision
    {
        QString id;
        QString symbol;
        QString label;
        QString meta;
        PillKind tint = PillKind::Value;
    };

    /**
     * The version history destination: a blurb, a search bar with filter chips
     * and a column of revision rows, each offering a diff and a restore.
     */
    class HistoryScreen : public Screen
    {
        Q_OBJECT

    public:
        explicit HistoryScreen(QWidget* parent = nullptr);
        ~HistoryScreen() override;

        void setRevisions(const QVector<Revision>& revisions);

    signals:
        void diffRequested(const QString& id);
        void restoreRequested(const QString& id);

    private:
        void rebuild();

        QVBoxLayout* m_revisionLayout = nullptr;
        QVector<Revision> m_revisions;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALHISTORYSCREEN_H
