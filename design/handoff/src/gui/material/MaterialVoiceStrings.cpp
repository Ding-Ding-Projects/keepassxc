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

#include "MaterialVoiceStrings.h"

#include "core/Config.h"

#include <QHash>

namespace Material
{
    namespace Voice
    {
        namespace
        {
            // Source of truth: utils/design/voice-strings.json, generated into
            // this table the same way MaterialSheetCatalogue is generated from
            // sheets.json. Do NOT hand-edit the table - re-run the generator so
            // a transcription can be diffed rather than trusted.
            //
            // The generator FAILS the build when an entry's five levels do not
            // share the same placeholder set. That check is the only thing
            // standing between a funny level and a vaguer warning.
            const QHash<QString, Message>& table()
            {
                static QHash<QString, Message> t;
                return t;
            }
        } // namespace

        QString text(const char* key, int level, Lang lang)
        {
            const auto m = table().value(QString::fromLatin1(key));
            const int i = qBound(0, level - 1, 4);
            switch (lang) {
            case Lang::En:
                return QString::fromUtf8(m.en[i]);
            case Lang::Yue:
                return QString::fromUtf8(m.yue[i]);
            case Lang::Both:
                // Caller decides layout; this returns the primary only.
                return QString::fromUtf8(m.en[i]);
            }
            return {};
        }

        Lang langFromString(const QString& v)
        {
            if (v == QLatin1String("yue")) return Lang::Yue;
            if (v == QLatin1String("both")) return Lang::Both;
            return Lang::En;
        }

        QString langToString(Lang l)
        {
            switch (l) {
            case Lang::Yue: return QStringLiteral("yue");
            case Lang::Both: return QStringLiteral("both");
            default: return QStringLiteral("en");
            }
        }

        QPair<QString, QString> label(const char* key)
        {
            Q_UNUSED(key)
            // TODO: from the same generated table's label section.
            return {};
        }

        QString regexToken(int tokenType, const QString& t, Lang lang)
        {
            Q_UNUSED(tokenType)
            Q_UNUSED(t)
            Q_UNUSED(lang)
            return {};
        }
    } // namespace Voice
} // namespace Material
