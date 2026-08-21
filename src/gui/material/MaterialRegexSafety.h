#ifndef KEEPASSXC_MATERIALREGEXSAFETY_H
#define KEEPASSXC_MATERIALREGEXSAFETY_H

#include <QList>
#include <QRegularExpression>
#include <QString>

namespace Material
{
    namespace RegexLimits
    {
        constexpr int PatternChars = 512;
        constexpr int SampleChars = 20000;
        constexpr int BudgetMs = 120;
        constexpr int MaxMatches = 500;
    }

    struct RegexRun
    {
        bool compiled = false;
        bool blocked = false;
        QString error;
        QList<QRegularExpressionMatch> matches;
        bool timedOut = false;
        bool truncated = false;
        bool sampleTruncated = false;
        qint64 elapsedMs = 0;
    };

    QStringList riskReport(const QString& pattern);
    QRegularExpression::PatternOptions optionsForFlags(const QString& flags);
    RegexRun runBounded(const QString& pattern,
                        QRegularExpression::PatternOptions options,
                        const QString& sample);
} // namespace Material

#endif
