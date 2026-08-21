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

#ifndef KEEPASSXC_MATERIALREGEXSAFETY_H
#define KEEPASSXC_MATERIALREGEXSAFETY_H

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace Material
{
    /**
     * Bounds around every regex evaluation the application performs.
     *
     * A password manager must not be able to hang on a pattern its own user
     * typed. Two independent mechanisms, because neither is sufficient alone:
     *
     * 1. A static shape check, run before evaluation. It flags the three shapes
     *    that cause catastrophic backtracking in practice - a quantified group
     *    whose body is itself quantified, a quantified alternation, and two
     *    greedy .* in sequence. This is a warning, not a refusal: the pattern
     *    may still be exactly what the user wants, and a heuristic that blocked
     *    it would be worse than one that mentions it.
     *
     * 2. A wall-clock budget around the match loop. PCRE2's own match limit is
     *    set as well, but it counts steps rather than time and a loaded machine
     *    can still stall inside the limit. When the budget expires the results
     *    gathered so far are returned and the surface says so - never silently
     *    truncated, and never presented as a complete answer.
     *
     * The sizes are deliberately small enough that a worst case is a stutter,
     * not a freeze. They are constants rather than settings because a user who
     * could raise them would be given a way to hang their own vault.
     */
    namespace RegexLimits
    {
        constexpr int PatternChars = 512;
        constexpr int SampleChars = 20000;
        constexpr int BudgetMs = 120;
        constexpr int MaxMatches = 500;
        constexpr int Pcre2MatchLimit = 100000;
    } // namespace RegexLimits

    struct RegexRisk
    {
        QString en;
        QString yue;
    };

    /** Shapes in @p pattern known to backtrack catastrophically. May be empty. */
    QList<RegexRisk> riskReport(const QString& pattern);

    struct RegexRun
    {
        bool compiled = false;
        QString error; // QRegularExpression::errorString(), verbatim
        QList<QRegularExpressionMatch> matches;
        bool timedOut = false;
        bool truncated = false;
        qint64 elapsedMs = 0;
    };

    /**
     * Evaluate @p pattern over @p sample within the limits above.
     *
     * Never returns a partially-true story: timedOut and truncated are separate
     * because they mean different things to a user deciding whether to trust
     * the result.
     */
    RegexRun runBounded(const QString& pattern,
                        QRegularExpression::PatternOptions options,
                        const QString& sample);
} // namespace Material

#endif // KEEPASSXC_MATERIALREGEXSAFETY_H
