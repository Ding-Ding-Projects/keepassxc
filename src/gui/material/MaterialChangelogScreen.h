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

#ifndef KEEPASSXC_MATERIALCHANGELOGSCREEN_H
#define KEEPASSXC_MATERIALCHANGELOGSCREEN_H

#include "MaterialChip.h"
#include "MaterialScreen.h"

#include <QString>
#include <QVector>

class QLabel;
class QVBoxLayout;

namespace Material
{
    /**
     * One line of a release. @p tag is the fixed / added / changed label of the
     * 74px pill, @p tint the family it is drawn in - Good, Value or Warn.
     */
    struct ChangeItem
    {
        QString tag;
        QString text;
        PillKind tint = PillKind::Value;
    };

    /** One release card: version, status pill, date and the change items. */
    struct Release
    {
        QString version;
        QString date;
        QString status;
        PillKind statusTint = PillKind::Value;
        QVector<ChangeItem> items;
    };

    /**
     * The changelog destination.
     *
     * Release cards filtered live by the search bar: a release stays when its
     * version matches, otherwise only the items whose tag or text match are
     * kept. The date range chip and the count line follow whatever is shown.
     */
    class ChangelogScreen : public Screen
    {
        Q_OBJECT

    public:
        explicit ChangelogScreen(QWidget* parent = nullptr);
        ~ChangelogScreen() override;

        void setReleases(const QVector<Release>& releases);

    signals:
        void exportRequested();

    private:
        void rebuild();
        void updateDateRange();

        QVBoxLayout* m_releaseLayout = nullptr;
        Chip* m_dateChip = nullptr;
        QLabel* m_countLabel = nullptr;
        QVector<Release> m_releases;
        QString m_query;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALCHANGELOGSCREEN_H
