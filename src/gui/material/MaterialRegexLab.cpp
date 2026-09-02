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

#include "MaterialRegexLab.h"

#include <QHash>
#include <QRegularExpression>

namespace Material
{
    namespace RegexLab
    {
        namespace
        {
            struct ClassName
            {
                QChar letter;
                const char* english;
                const char* cantonese;
            };

            const ClassName ClassNames[] = {
                {QLatin1Char('d'), "any digit 0-9", "數字 0-9"},
                {QLatin1Char('D'), "anything that is not a digit", "唔係數字"},
                {QLatin1Char('w'), "word character: letter, digit or _", "字母、數字或者底線"},
                {QLatin1Char('W'), "not a word character", "唔係字母數字"},
                {QLatin1Char('s'), "whitespace", "空白位"},
                {QLatin1Char('S'), "not whitespace", "唔係空白"},
                {QLatin1Char('b'), "word boundary", "字界"},
                {QLatin1Char('B'), "not a word boundary", "唔係字界"},
                {QLatin1Char('n'), "newline", "換行"},
                {QLatin1Char('t'), "tab", "Tab"},
                {QLatin1Char('r'), "carriage return", "回車"},
            };

            const ClassName* className(QChar letter)
            {
                for (const ClassName& entry : ClassNames) {
                    if (entry.letter == letter) {
                        return &entry;
                    }
                }
                return nullptr;
            }

            Token make(const QString& type,
                       const QString& text,
                       const QString& english,
                       const QString& cantonese,
                       int start,
                       bool pcreOnly = false)
            {
                Token token;
                token.type = type;
                token.text = text;
                token.english = english;
                token.cantonese = cantonese;
                token.start = start;
                token.end = start + text.size();
                token.pcreOnly = pcreOnly;
                return token;
            }

            const QString Quote = QString::fromUtf8("“");
            const QString Unquote = QString::fromUtf8("”");
        } // namespace

        QVector<Token> tokenize(const QString& src)
        {
            QVector<Token> out;
            int i = 0;
            int groupNo = 0;
            const int n = src.size();
            auto at = [&](int index) { return index < n ? src.at(index) : QChar(); };

            while (i < n) {
                const QChar c = src.at(i);

                if (c == QLatin1Char('\\')) {
                    const QChar next = at(i + 1);
                    if ((next == QLatin1Char('p') || next == QLatin1Char('P')) && at(i + 2) == QLatin1Char('{')) {
                        const int close = src.indexOf(QLatin1Char('}'), i);
                        if (close > -1) {
                            const QString body = src.mid(i + 3, close - i - 3);
                            const QString text = src.mid(i, close + 1 - i);
                            out.append(make(QStringLiteral("unicode"),
                                            text,
                                            QStringLiteral("Unicode property %1%2")
                                                .arg(body, next == QLatin1Char('P') ? QStringLiteral(" (negated)") : QString()),
                                            QString::fromUtf8("Unicode 屬性 %1").arg(body),
                                            i));
                            i = close + 1;
                            continue;
                        }
                    }
                    if (next == QLatin1Char('k') && at(i + 2) == QLatin1Char('<')) {
                        const int close = src.indexOf(QLatin1Char('>'), i);
                        if (close > -1) {
                            const QString name = src.mid(i + 3, close - i - 3);
                            out.append(make(QStringLiteral("backref"),
                                            src.mid(i, close + 1 - i),
                                            QStringLiteral("back-reference to group %1%2%3").arg(Quote, name, Unquote),
                                            QString::fromUtf8("回頭引用 %1%2%3").arg(Quote, name, Unquote),
                                            i));
                            i = close + 1;
                            continue;
                        }
                    }
                    if (next.isDigit() && next != QLatin1Char('0')) {
                        out.append(make(QStringLiteral("backref"),
                                        QStringLiteral("\\") + next,
                                        QStringLiteral("back-reference to group %1").arg(next),
                                        QString::fromUtf8("回頭引用 第%1組").arg(next),
                                        i));
                        i += 2;
                        continue;
                    }
                    if (const ClassName* entry = className(next)) {
                        const bool anchor = next == QLatin1Char('b') || next == QLatin1Char('B');
                        out.append(make(anchor ? QStringLiteral("anchor") : QStringLiteral("class"),
                                        QStringLiteral("\\") + next,
                                        QString::fromUtf8(entry->english),
                                        QString::fromUtf8(entry->cantonese),
                                        i));
                        i += 2;
                        continue;
                    }
                    out.append(make(QStringLiteral("escape"),
                                    QStringLiteral("\\") + next,
                                    QStringLiteral("a literal %1").arg(next),
                                    QString::fromUtf8("真係個 %1 字").arg(next),
                                    i));
                    i += next.isNull() ? 1 : 2;
                    continue;
                }

                if (c == QLatin1Char('[')) {
                    int j = i + 1;
                    int depth = 1;
                    if (at(j) == QLatin1Char('^')) {
                        ++j;
                    }
                    if (at(j) == QLatin1Char(']')) {
                        ++j;
                    }
                    while (j < n && depth > 0) {
                        if (src.at(j) == QLatin1Char('\\')) {
                            j += 2;
                        } else if (src.at(j) == QLatin1Char(']')) {
                            --depth;
                            ++j;
                        } else {
                            ++j;
                        }
                    }
                    j = qMin(j, n);
                    const QString text = src.mid(i, j - i);
                    const bool negated = text.size() > 1 && text.at(1) == QLatin1Char('^');
                    out.append(make(QStringLiteral("charclass"),
                                    text,
                                    negated ? QStringLiteral("any character NOT in this set")
                                            : QStringLiteral("any one character from this set"),
                                    negated ? QString::fromUtf8("唔喺呢個範圍嘅字") : QString::fromUtf8("呢個範圍入面任何一個字"),
                                    i));
                    i = j;
                    continue;
                }

                if (c == QLatin1Char('(')) {
                    const QString head3 = src.mid(i, 3);
                    const QString head4 = src.mid(i, 4);
                    struct Head
                    {
                        const char* text;
                        const char* type;
                        const char* english;
                        const char* cantonese;
                        bool pcreOnly;
                    };
                    static const Head heads[] = {
                        {"(?:", "group", "group, not captured", "分組但唔捕捉", false},
                        {"(?=", "look", "lookahead: what follows must match", "向前望：後面要係咁", false},
                        {"(?!", "look", "negative lookahead: what follows must NOT match", "向前望：後面唔可以係咁", false},
                        {"(?<=", "look", "lookbehind: what precedes must match", "向後望：前面要係咁", false},
                        {"(?<!", "look", "negative lookbehind: what precedes must NOT match", "向後望：前面唔可以係咁", false},
                        {"(?>", "group", "atomic group - PCRE2 only, not ECMAScript", "原子組 - 淨係 Qt 有", true},
                    };
                    bool matched = false;
                    for (const Head& head : heads) {
                        const QString text = QString::fromLatin1(head.text);
                        if ((text.size() == 3 ? head3 : head4) == text) {
                            out.append(make(QString::fromLatin1(head.type),
                                            text,
                                            QString::fromUtf8(head.english),
                                            QString::fromUtf8(head.cantonese),
                                            i,
                                            head.pcreOnly));
                            i += text.size();
                            matched = true;
                            break;
                        }
                    }
                    if (matched) {
                        continue;
                    }
                    if (head3 == QLatin1String("(?<")) {
                        const int close = src.indexOf(QLatin1Char('>'), i);
                        if (close > -1) {
                            const QString name = src.mid(i + 3, close - i - 3);
                            ++groupNo;
                            Token token = make(QStringLiteral("group"),
                                               src.mid(i, close + 1 - i),
                                               QStringLiteral("capture group %1, named %2%3%4").arg(groupNo).arg(Quote, name, Unquote),
                                               QString::fromUtf8("第%1組，叫 %2%3%4").arg(groupNo).arg(Quote, name, Unquote),
                                               i);
                            token.group = groupNo;
                            token.name = name;
                            out.append(token);
                            i = close + 1;
                            continue;
                        }
                    }
                    ++groupNo;
                    Token token = make(QStringLiteral("group"),
                                       QStringLiteral("("),
                                       QStringLiteral("capture group %1").arg(groupNo),
                                       QString::fromUtf8("第%1組").arg(groupNo),
                                       i);
                    token.group = groupNo;
                    out.append(token);
                    ++i;
                    continue;
                }

                if (c == QLatin1Char(')')) {
                    out.append(make(QStringLiteral("group"), QStringLiteral(")"), QStringLiteral("end of group"), QString::fromUtf8("組完"), i));
                    ++i;
                    continue;
                }
                if (c == QLatin1Char('|')) {
                    out.append(make(QStringLiteral("alt"), QStringLiteral("|"), QStringLiteral("or - either side may match"), QString::fromUtf8("或者"), i));
                    ++i;
                    continue;
                }
                if (c == QLatin1Char('^')) {
                    out.append(make(QStringLiteral("anchor"), QStringLiteral("^"), QStringLiteral("start of the string (or line with m)"), QString::fromUtf8("開頭"), i));
                    ++i;
                    continue;
                }
                if (c == QLatin1Char('$')) {
                    out.append(make(QStringLiteral("anchor"), QStringLiteral("$"), QStringLiteral("end of the string (or line with m)"), QString::fromUtf8("結尾"), i));
                    ++i;
                    continue;
                }
                if (c == QLatin1Char('.')) {
                    out.append(make(QStringLiteral("dot"), QStringLiteral("."), QStringLiteral("any character except newline (unless s)"), QString::fromUtf8("任何一個字"), i));
                    ++i;
                    continue;
                }

                if (c == QLatin1Char('*') || c == QLatin1Char('+') || c == QLatin1Char('?')) {
                    const bool lazy = at(i + 1) == QLatin1Char('?');
                    const bool possessive = at(i + 1) == QLatin1Char('+');
                    const QString text = src.mid(i, lazy || possessive ? 2 : 1);
                    const QString base = c == QLatin1Char('*')   ? QStringLiteral("zero or more")
                                         : c == QLatin1Char('+') ? QStringLiteral("one or more")
                                                                 : QStringLiteral("optional - zero or one");
                    const QString baseYue = c == QLatin1Char('*')   ? QString::fromUtf8("零次或者更多")
                                            : c == QLatin1Char('+') ? QString::fromUtf8("一次或者更多")
                                                                    : QString::fromUtf8("有冇都得");
                    out.append(make(QStringLiteral("quant"),
                                    text,
                                    base
                                        + (lazy         ? QStringLiteral(", lazy (as few as possible)")
                                           : possessive ? QStringLiteral(", possessive - PCRE2 only")
                                                        : QStringLiteral(", greedy")),
                                    baseYue
                                        + (lazy         ? QString::fromUtf8("，懶（要幾少有幾少）")
                                           : possessive ? QString::fromUtf8("，霸住唔放（淨係 Qt 有）")
                                                        : QString::fromUtf8("，貪心")),
                                    i,
                                    possessive));
                    i += text.size();
                    continue;
                }

                if (c == QLatin1Char('{')) {
                    const int close = src.indexOf(QLatin1Char('}'), i);
                    static const QRegularExpression braces(QStringLiteral("^\\{\\d+(,\\d*)?\\}$"));
                    if (close > -1 && braces.match(src.mid(i, close + 1 - i)).hasMatch()) {
                        const bool lazy = at(close + 1) == QLatin1Char('?');
                        const QString text = src.mid(i, close + 1 - i + (lazy ? 1 : 0));
                        QString body = src.mid(i + 1, close - i - 1);
                        QString spoken = body;
                        spoken.replace(QLatin1Char(','), QLatin1String(" to "));
                        if (spoken.endsWith(QLatin1String(" to "))) {
                            spoken.chop(4);
                            spoken.append(QLatin1String(" or more"));
                        }
                        out.append(make(QStringLiteral("quant"),
                                        text,
                                        QStringLiteral("repeated %1 times%2").arg(spoken, lazy ? QStringLiteral(", lazy") : QString()),
                                        QString::fromUtf8("重複 %1 次%2").arg(body, lazy ? QString::fromUtf8("，懶") : QString()),
                                        i));
                        i += text.size();
                        continue;
                    }
                }

                int j = i;
                static const QString special = QStringLiteral("\\[](){}|^$.*+?");
                while (j < n && !special.contains(src.at(j))) {
                    ++j;
                }
                if (j == i) {
                    j = i + 1;
                }
                const QString text = src.mid(i, j - i);
                out.append(make(QStringLiteral("literal"),
                                text,
                                QStringLiteral("the text %1%2%3").arg(Quote, text, Unquote),
                                QString::fromUtf8("%1%2%3 呢啲字").arg(Quote, text, Unquote),
                                i));
                i = j;
            }
            return out;
        }

        QVector<Dialect> dialects()
        {
            return {
                {QStringLiteral("js"),
                 QStringLiteral("ECMAScript (RegExp)"),
                 QString::fromUtf8("JS 引擎"),
                 {{QStringLiteral("i"), QStringLiteral("ignore case"), QString::fromUtf8("唔理大細楷")},
                  {QStringLiteral("m"), QStringLiteral("multiline ^ $"), QString::fromUtf8("逐行錨點")},
                  {QStringLiteral("s"), QStringLiteral("dot matches newline"), QString::fromUtf8("點號食換行")},
                  {QStringLiteral("u"), QStringLiteral("unicode mode"), QString::fromUtf8("Unicode 模式")},
                  {QStringLiteral("g"), QStringLiteral("global"), QString::fromUtf8("全部")},
                  {QStringLiteral("y"), QStringLiteral("sticky"), QString::fromUtf8("黐住")}},
                 {QStringLiteral("Lookbehind is supported in modern engines but is not in the ES2018 baseline everywhere."),
                  QStringLiteral("\\p{...} requires the u or v flag."),
                  QStringLiteral("No possessive quantifiers, no atomic groups, no recursion.")}},
                {QStringLiteral("qt"),
                 QStringLiteral("QRegularExpression (PCRE2)"),
                 QString::fromUtf8("Qt 引擎"),
                 {{QStringLiteral("CaseInsensitiveOption"), QStringLiteral("ignore case"), QString::fromUtf8("唔理大細楷")},
                  {QStringLiteral("MultilineOption"), QStringLiteral("multiline ^ $"), QString::fromUtf8("逐行錨點")},
                  {QStringLiteral("DotMatchesEverythingOption"), QStringLiteral("dot matches newline"), QString::fromUtf8("點號食換行")},
                  {QStringLiteral("UseUnicodePropertiesOption"), QStringLiteral("unicode properties"), QString::fromUtf8("Unicode 屬性")},
                  {QStringLiteral("ExtendedPatternSyntaxOption"), QStringLiteral("extended / free-spacing"), QString::fromUtf8("自由排版")},
                  {QStringLiteral("InvertedGreedinessOption"), QStringLiteral("inverted greediness"), QString::fromUtf8("反貪心")}},
                 {QStringLiteral("Supports atomic groups (?>...), possessive quantifiers a++ and recursion (?R)."),
                  QStringLiteral("Unicode properties need UseUnicodePropertiesOption explicitly."),
                  QStringLiteral("Backslashes double inside a C++ string literal: \\d becomes \"\\\\d\".")}},
            };
        }

        QVector<Preset> presets()
        {
            return {
                {QStringLiteral("url-host"), QStringLiteral("URL host"), QString::fromUtf8("網址主機"), QStringLiteral("^https?://(?<host>[^/:?#]+)"), QStringLiteral("i"), QStringLiteral("https://service.example/console\nhttp://host.example:5432/status")},
                {QStringLiteral("email"), QStringLiteral("Email address"), QString::fromUtf8("電郵地址"), QStringLiteral("(?<local>[\\w.+-]+)@(?<domain>[\\w-]+\\.[\\w.-]+)"), QStringLiteral("gi"), QStringLiteral("ops@acme.example\nme@fastmail.example\nnot-an-email@")},
                {QStringLiteral("masked"), QStringLiteral("Masked account tail"), QString::fromUtf8("尾四位"), QString::fromUtf8("•{2,}(?<tail>\\d{3,4})\\b"), QStringLiteral("g"), QString::fromUtf8("••••3391\n••••7742\n••••1180")},
                {QStringLiteral("ipv4"), QStringLiteral("IPv4 address"), QStringLiteral("IPv4"), QStringLiteral("\\b(?:(?:25[0-5]|2[0-4]\\d|1?\\d?\\d)\\.){3}(?:25[0-5]|2[0-4]\\d|1?\\d?\\d)\\b"), QStringLiteral("g"), QStringLiteral("192.0.2.10\n198.51.100.42\n999.1.1.1")},
                {QStringLiteral("uuid"), QStringLiteral("UUID v4"), QStringLiteral("UUID"), QStringLiteral("\\b[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}\\b"), QStringLiteral("gi"), QStringLiteral("9f2c1ab0-3d17-4b7b-9c33-2bf39c001188")},
                {QStringLiteral("kpxc-ref"), QStringLiteral("KeePassXC field reference"), QString::fromUtf8("欄位引用"), QStringLiteral("\\{REF:(?<want>[TUPAN])@(?<by>[TUPANIO]):(?<query>[^}]+)\\}"), QStringLiteral("g"), QString::fromUtf8("{REF:P@T:AWS — production root}\n{REF:U@I:9f2c1ab0}")},
                {QStringLiteral("cjk"), QStringLiteral("Any CJK character"), QString::fromUtf8("中文字"), QStringLiteral("\\p{Han}+"), QStringLiteral("gu"), QString::fromUtf8("帶子蝦餃 scallop har gow 筍尖蝦餃")},
                {QStringLiteral("dupe-word"), QStringLiteral("Doubled word (backreference)"), QString::fromUtf8("重複字"), QStringLiteral("\\b(\\w+)\\s+\\1\\b"), QStringLiteral("gi"), QStringLiteral("the the vault is is fine")},
                {QStringLiteral("not-prod"), QStringLiteral("Host that is not prod (lookahead)"), QString::fromUtf8("唔係 prod"), QStringLiteral("^(?!.*prod)(?<name>[\\w.-]+)$"), QStringLiteral("m"), QStringLiteral("db.staging.internal\ndb.prod.internal\ncache.dev.internal")},
                {QStringLiteral("after-colon"), QStringLiteral("Value after a key (lookbehind)"), QString::fromUtf8("冒號之後"), QStringLiteral("(?<=password:\\s)(?<value>\\S+)"), QStringLiteral("gi"), QString::fromUtf8("username: ops\npassword: •••••••••")},
            };
        }

        QVector<Export> translate(const QString& pattern, const QString& flags)
        {
            QString cpp = pattern;
            cpp.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
            cpp.replace(QLatin1Char('"'), QLatin1String("\\\""));
            QStringList qtOptions;
            if (flags.contains(QLatin1Char('i'))) {
                qtOptions << QStringLiteral("QRegularExpression::CaseInsensitiveOption");
            }
            if (flags.contains(QLatin1Char('m'))) {
                qtOptions << QStringLiteral("QRegularExpression::MultilineOption");
            }
            if (flags.contains(QLatin1Char('s'))) {
                qtOptions << QStringLiteral("QRegularExpression::DotMatchesEverythingOption");
            }
            if (flags.contains(QLatin1Char('u'))) {
                qtOptions << QStringLiteral("QRegularExpression::UseUnicodePropertiesOption");
            }
            QStringList pyFlags;
            if (flags.contains(QLatin1Char('i'))) {
                pyFlags << QStringLiteral("re.IGNORECASE");
            }
            if (flags.contains(QLatin1Char('m'))) {
                pyFlags << QStringLiteral("re.MULTILINE");
            }
            if (flags.contains(QLatin1Char('s'))) {
                pyFlags << QStringLiteral("re.DOTALL");
            }
            QString shell = pattern;
            shell.replace(QLatin1Char('\''), QLatin1String("'\\''"));
            const QString qt = QStringLiteral("QRegularExpression re(QStringLiteral(\"%1\")%2);")
                                   .arg(cpp,
                                        qtOptions.isEmpty()
                                            ? QString()
                                            : QStringLiteral(",\n                       ") + qtOptions.join(QStringLiteral(" |\n                       ")));
            return {
                {QStringLiteral("qt"), QStringLiteral("Qt / C++"), qt},
                {QStringLiteral("js"), QStringLiteral("JavaScript"), QStringLiteral("/%1/%2").arg(pattern, flags)},
                {QStringLiteral("python"), QStringLiteral("Python"), QStringLiteral("re.compile(r\"%1\"%2)").arg(pattern, pyFlags.isEmpty() ? QString() : QStringLiteral(", ") + pyFlags.join(QStringLiteral(" | ")))},
                {QStringLiteral("grep"), QStringLiteral("grep"), QStringLiteral("grep -P%1 '%2'").arg(flags.contains(QLatin1Char('i')) ? QStringLiteral("i") : QString(), shell)},
            };
        }

        QStringList groupNames(const QString& pattern)
        {
            QStringList names;
            for (const Token& token : tokenize(pattern)) {
                if (token.group > 0) {
                    names.append(token.name);
                }
            }
            return names;
        }

        QString qtReplacement(const QString& pattern, const QString& replacement)
        {
            const QStringList names = groupNames(pattern);
            QString out;
            for (int i = 0; i < replacement.size(); ++i) {
                const QChar c = replacement.at(i);
                if (c != QLatin1Char('$') || i + 1 >= replacement.size()) {
                    if (c == QLatin1Char('\\')) {
                        out.append(QLatin1String("\\\\"));
                    } else {
                        out.append(c);
                    }
                    continue;
                }
                const QChar next = replacement.at(i + 1);
                if (next == QLatin1Char('&')) {
                    out.append(QLatin1String("\\0"));
                    ++i;
                } else if (next == QLatin1Char('$')) {
                    out.append(QLatin1Char('$'));
                    ++i;
                } else if (next.isDigit()) {
                    int j = i + 1;
                    while (j < replacement.size() && replacement.at(j).isDigit()) {
                        ++j;
                    }
                    out.append(QLatin1Char('\\') + replacement.mid(i + 1, j - i - 1));
                    i = j - 1;
                } else if (next == QLatin1Char('<')) {
                    const int close = replacement.indexOf(QLatin1Char('>'), i);
                    const QString name = close > -1 ? replacement.mid(i + 2, close - i - 2) : QString();
                    const int index = names.indexOf(name);
                    if (close > -1 && index >= 0) {
                        out.append(QStringLiteral("\\%1").arg(index + 1));
                        i = close;
                    } else {
                        out.append(c);
                    }
                } else {
                    out.append(c);
                }
            }
            return out;
        }

        QVector<CheatEntry> cheatSheet()
        {
            QVector<CheatEntry> rows;
            for (const ClassName& entry : ClassNames) {
                rows.append({QStringLiteral("\\") + entry.letter, QString::fromUtf8(entry.english), QString::fromUtf8(entry.cantonese), false});
            }
            rows.append({QStringLiteral("."), QStringLiteral("any character except newline (unless s)"), QString::fromUtf8("任何一個字"), false});
            rows.append({QStringLiteral("[abc]"), QStringLiteral("any one character from this set"), QString::fromUtf8("呢個範圍入面任何一個字"), false});
            rows.append({QStringLiteral("[^abc]"), QStringLiteral("any character NOT in this set"), QString::fromUtf8("唔喺呢個範圍嘅字"), false});
            rows.append({QStringLiteral("^"), QStringLiteral("start of the string (or line with m)"), QString::fromUtf8("開頭"), false});
            rows.append({QStringLiteral("$"), QStringLiteral("end of the string (or line with m)"), QString::fromUtf8("結尾"), false});
            rows.append({QStringLiteral("a*"), QStringLiteral("zero or more, greedy"), QString::fromUtf8("零次或者更多，貪心"), false});
            rows.append({QStringLiteral("a+"), QStringLiteral("one or more, greedy"), QString::fromUtf8("一次或者更多，貪心"), false});
            rows.append({QStringLiteral("a?"), QStringLiteral("optional - zero or one"), QString::fromUtf8("有冇都得"), false});
            rows.append({QStringLiteral("a{2,5}"), QStringLiteral("repeated 2 to 5 times"), QString::fromUtf8("重複 2 到 5 次"), false});
            rows.append({QStringLiteral("a*?"), QStringLiteral("lazy: as few as possible"), QString::fromUtf8("懶：要幾少有幾少"), false});
            rows.append({QStringLiteral("a++"), QStringLiteral("possessive: never gives back"), QString::fromUtf8("霸住唔放"), true});
            rows.append({QStringLiteral("(...)"), QStringLiteral("capture group"), QString::fromUtf8("捕捉組"), false});
            rows.append({QStringLiteral("(?<name>...)"), QStringLiteral("named capture group"), QString::fromUtf8("有名捕捉組"), false});
            rows.append({QStringLiteral("(?:...)"), QStringLiteral("group, not captured"), QString::fromUtf8("分組但唔捕捉"), false});
            rows.append({QStringLiteral("(?>...)"), QStringLiteral("atomic group"), QString::fromUtf8("原子組"), true});
            rows.append({QStringLiteral("a|b"), QStringLiteral("or - either side may match"), QString::fromUtf8("或者"), false});
            rows.append({QStringLiteral("(?=...)"), QStringLiteral("lookahead"), QString::fromUtf8("向前望"), false});
            rows.append({QStringLiteral("(?!...)"), QStringLiteral("negative lookahead"), QString::fromUtf8("向前望，唔可以係"), false});
            rows.append({QStringLiteral("(?<=...)"), QStringLiteral("lookbehind"), QString::fromUtf8("向後望"), false});
            rows.append({QStringLiteral("(?<!...)"), QStringLiteral("negative lookbehind"), QString::fromUtf8("向後望，唔可以係"), false});
            rows.append({QStringLiteral("\\1"), QStringLiteral("back-reference to group 1"), QString::fromUtf8("回頭引用第 1 組"), false});
            rows.append({QStringLiteral("\\k<name>"), QStringLiteral("back-reference to a named group"), QString::fromUtf8("回頭引用有名嘅組"), false});
            rows.append({QStringLiteral("\\p{Han}"), QStringLiteral("Unicode property (needs u)"), QString::fromUtf8("Unicode 屬性（要 u）"), false});
            rows.append({QStringLiteral("(?R)"), QStringLiteral("recursion"), QString::fromUtf8("遞歸"), true});
            return rows;
        }
    } // namespace RegexLab
} // namespace Material
