#include "MaterialRegexSafety.h"

#include <QElapsedTimer>

namespace Material
{
    QStringList riskReport(const QString& pattern)
    {
        struct Risk { const char* expression; const char* message; };
        static constexpr Risk risks[] = {
            {R"(\([^)]*[+*][^)]*\)\s*[+*])", "Nested quantified group"},
            {R"(\([^)]*\|[^)]*\)\s*[+*])", "Quantified alternation"},
            {R"(\.\*[^\n]*\.\*)", "Repeated greedy wildcard"},
        };
        QStringList result;
        for (const auto& risk : risks) {
            if (QRegularExpression(QString::fromLatin1(risk.expression)).match(pattern).hasMatch()) {
                result.append(QString::fromLatin1(risk.message));
            }
        }
        return result;
    }

    QRegularExpression::PatternOptions optionsForFlags(const QString& flags)
    {
        QRegularExpression::PatternOptions options = QRegularExpression::UseUnicodePropertiesOption;
        if (flags.contains(QLatin1Char('i'))) options |= QRegularExpression::CaseInsensitiveOption;
        if (flags.contains(QLatin1Char('m'))) options |= QRegularExpression::MultilineOption;
        if (flags.contains(QLatin1Char('s'))) options |= QRegularExpression::DotMatchesEverythingOption;
        return options;
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
        const QStringList risks = riskReport(pattern);
        if (!risks.isEmpty()) {
            run.blocked = true;
            run.error = QObject::tr("Pattern was blocked because it contains a high-risk backtracking shape: %1")
                            .arg(risks.join(QStringLiteral(", ")));
            return run;
        }
        QRegularExpression expression(pattern, options);
        if (!expression.isValid()) {
            run.error = expression.errorString();
            return run;
        }
        run.compiled = true;
        run.sampleTruncated = sample.size() > RegexLimits::SampleChars;
        const QString text = sample.left(RegexLimits::SampleChars);
        QElapsedTimer timer;
        timer.start();
        int offset = 0;
        while (offset <= text.size()) {
            const auto match = expression.match(text, offset);
            if (!match.hasMatch()) break;
            run.matches.append(match);
            offset = match.capturedEnd() == match.capturedStart() ? match.capturedEnd() + 1 : match.capturedEnd();
            if (run.matches.size() >= RegexLimits::MaxMatches) { run.truncated = true; break; }
            if (timer.elapsed() > RegexLimits::BudgetMs) { run.timedOut = true; break; }
        }
        run.elapsedMs = timer.elapsed();
        return run;
    }
} // namespace Material
