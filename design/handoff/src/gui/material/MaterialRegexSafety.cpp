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

#include "MaterialRegexSafety.h"

#include <QElapsedTimer>

namespace Material
{
    namespace
    {
        struct Shape
        {
            const char* re;
            const char* en;
            const char* yue;
        };

        constexpr Shape RiskShapes[] = {
            {R"(\([^)]*[+*][^)]*\)\s*[+*])",
             "a quantified group whose body is itself quantified - (a+)+",
             "組入面有 +，組外面又有 +"},
            {R"(\([^)]*\|[^)]*\)\s*[+*])",
             "a quantified alternation - (a|a)*",
             "有 | 嘅組再加 *"},
            {R"(\.\*\.\*)", "two greedy .* in sequence", "兩個 .* 排埋一齊"}};
    } // namespace

    QList<RegexRisk> riskReport(const QString& pattern)
    {
        QList<RegexRisk> out;
        for (const auto& s : RiskShapes) {
            const QRegularExpression probe(QString::fromLatin1(s.re));
            if (probe.match(pattern).hasMatch()) {
                out.append({QString::fromUtf8(s.en), QString::fromUtf8(s.yue)});
            }
        }
        return out;
    }

    RegexRun runBounded(const QString& pattern,
                        QRegularExpression::PatternOptions options,
                        const QString& sample)
    {
        RegexRun run;
        if (pattern.size() > RegexLimits::PatternChars) {
            run.error = QObject::tr("Pattern is longer than %1 characters.").arg(RegexLimits::PatternChars);
            return run;
        }

        QRegularExpression re(pattern, options);
        if (!re.isValid()) {
            // Verbatim. Never paraphrase the engine's error - the offset it
            // names is what makes it actionable.
            run.error = re.errorString();
            return run;
        }
        run.compiled = true;

        const QString text = sample.left(RegexLimits::SampleChars);
        QElapsedTimer timer;
        timer.start();

        int offset = 0;
        while (offset <= text.size()) {
            const auto m = re.match(text, offset);
            if (!m.hasMatch()) {
                break;
            }
            run.matches.append(m);
            offset = m.capturedEnd() == m.capturedStart() ? m.capturedEnd() + 1 : m.capturedEnd();
            if (run.matches.size() >= RegexLimits::MaxMatches) {
                run.truncated = true;
                break;
            }
            if (timer.elapsed() > RegexLimits::BudgetMs) {
                run.timedOut = true;
                break;
            }
        }
        run.elapsedMs = timer.elapsed();
        return run;
    }
} // namespace Material
