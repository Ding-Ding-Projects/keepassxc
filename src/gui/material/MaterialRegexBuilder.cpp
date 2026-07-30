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

#include "MaterialRegexBuilder.h"

#include "MaterialButtons.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QAbstractButton>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>
#include <QVariant>

#include <utility>

namespace Material
{
    namespace
    {
        constexpr int SheetWidth = 1000;
        constexpr int SheetPadding = 26;
        constexpr int PaletteWidth = 300;
        constexpr int TokenChipHeight = 32;
        constexpr int FlagChipSize = 30;
        constexpr int PatternHeight = 52;
        constexpr int SampleHeight = 132;
        constexpr int MatchMinHeight = 96;
        constexpr int MatchMaxHeight = 150;
        constexpr int MatchPosWidth = 52;
        constexpr int ChipGap = 8;
        // Width of the right hand column before the sheet has been laid out.
        constexpr int DefaultEditorWidth = SheetWidth - PaletteWidth - 2 * SheetPadding - 18;
        // Enough to see what a pattern does without rebuilding thousands of rows.
        constexpr int MaxMatches = 200;

        // Labels remember their type and colour role so a theme change can
        // restyle the whole sheet without a rebuild.
        constexpr const char* TypeProperty = "materialType";
        constexpr const char* ColorProperty = "materialRole";
        constexpr const char* OverlineProperty = "materialOverline";

        void styleLabel(QLabel* label, TypeRole type, Role color, bool overline = false)
        {
            label->setProperty(TypeProperty, static_cast<int>(type));
            label->setProperty(ColorProperty, static_cast<int>(color));
            label->setProperty(OverlineProperty, overline);

            QFont font = theme()->font(type);
            if (overline) {
                font.setCapitalization(QFont::AllUppercase);
                font.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
            }
            label->setFont(font);
            label->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(theme()->hex(color)));
        }

        void restyleLabels(QWidget* root)
        {
            for (QLabel* label : root->findChildren<QLabel*>()) {
                const QVariant type = label->property(TypeProperty);
                if (!type.isValid()) {
                    continue;
                }
                styleLabel(label,
                           static_cast<TypeRole>(type.toInt()),
                           static_cast<Role>(label->property(ColorProperty).toInt()),
                           label->property(OverlineProperty).toBool());
            }
        }

        QLabel* makeLabel(const QString& text, TypeRole type, Role color, bool overline = false)
        {
            auto* label = new QLabel(text);
            styleLabel(label, type, color, overline);
            return label;
        }

        /** The 12px monospace face the palette, the pattern field and the matches use. */
        QFont monoFont(int delta = 0)
        {
            QFont font = theme()->font(TypeRole::Mono);
            font.setPointSize(qMax(1, font.pointSize() + delta));
            return font;
        }

        /** Pack @p items into left-aligned rows no wider than @p width. */
        void wrapIntoRows(QVBoxLayout* column, const QList<QWidget*>& items, int width, int spacing)
        {
            QList<QHBoxLayout*> rows;
            int used = 0;
            for (QWidget* item : items) {
                const int itemWidth = item->sizeHint().width();
                if (rows.isEmpty() || used + spacing + itemWidth > width) {
                    auto* row = new QHBoxLayout;
                    row->setContentsMargins(0, 0, 0, 0);
                    row->setSpacing(spacing);
                    column->addLayout(row);
                    rows.append(row);
                    used = 0;
                }
                rows.last()->addWidget(item);
                used += (used > 0 ? spacing : 0) + itemWidth;
            }
            for (QHBoxLayout* row : rows) {
                row->addStretch(1);
            }
        }

        struct Token
        {
            QString label;
            QString insert;
            int caretBack; // where the caret lands inside a wrapper token
            QString hint;
        };

        struct TokenGroup
        {
            QString title;
            QList<Token> tokens;
        };
    } // namespace

    /** A rounded panel: a role fill plus an optional border of any width. */
    class RegexPanel : public QWidget
    {
    public:
        RegexPanel(int radius, Role fill, QWidget* parent = nullptr)
            : QWidget(parent)
            , m_radius(radius)
            , m_fill(fill)
        {
        }

        void setBorder(const QColor& color, int width)
        {
            m_border = color;
            m_borderWidth = width;
            update();
        }

    protected:
        void paintEvent(QPaintEvent* event) override
        {
            Q_UNUSED(event)
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);

            const qreal inset = m_borderWidth / 2.0;
            const QPainterPath path = roundedPath(QRectF(rect()).adjusted(inset, inset, -inset, -inset), m_radius);
            painter.fillPath(path, theme()->color(m_fill));
            if (m_borderWidth > 0 && m_border.isValid()) {
                painter.strokePath(path, QPen(m_border, m_borderWidth));
            }
        }

    private:
        int m_radius;
        Role m_fill;
        QColor m_border;
        int m_borderWidth = 0;
    };

    /**
     * A monospace chip: the palette tokens insert themselves, the flag chips
     * are checkable and fill with primary when on.
     */
    class RegexTokenChip : public QAbstractButton
    {
    public:
        RegexTokenChip(const QString& text, bool checkable, QWidget* parent = nullptr)
            : QAbstractButton(parent)
        {
            setText(text);
            setCheckable(checkable);
            setCursor(Qt::PointingHandCursor);
            setFont(monoFont(-1));
        }

        QSize sizeHint() const override
        {
            return {fontMetrics().horizontalAdvance(text()) + 24, TokenChipHeight};
        }

        QSize minimumSizeHint() const override
        {
            return sizeHint();
        }

    protected:
        void paintEvent(QPaintEvent* event) override
        {
            Q_UNUSED(event)
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);

            QColor fill;
            QColor border;
            QColor content;
            if (isCheckable() && isChecked()) {
                fill = theme()->color(Role::Primary);
                content = theme()->color(Role::OnPrimary);
            } else {
                border = theme()->color(Role::Outline);
                content = theme()->color(isCheckable() ? Role::OnSurfaceVariant : Role::OnSurface);
                if (m_hovered || isDown()) {
                    fill = theme()->color(Role::SurfaceContainerHigh);
                }
            }

            paintSurface(&painter, rect(), Shape::Small, fill, border);
            painter.setPen(content);
            painter.setFont(font());
            painter.drawText(rect(), Qt::AlignCenter, text());
        }

        void enterEvent(QEnterEvent* event) override
        {
            m_hovered = true;
            update();
            QAbstractButton::enterEvent(event);
        }

        void leaveEvent(QEvent* event) override
        {
            m_hovered = false;
            update();
            QAbstractButton::leaveEvent(event);
        }

    private:
        bool m_hovered = false;
    };

    // ---------------------------------------------------------------- builder

    RegexBuilder::RegexBuilder(QWidget* parent)
        : Overlay(parent)
    {
        m_sheet = new RegexPanel(Shape::ExtraLarge, Role::SurfaceContainerLowest);

        auto* layout = new QVBoxLayout(m_sheet);
        layout->setContentsMargins(SheetPadding, 22, SheetPadding, 22);
        layout->setSpacing(14);
        layout->addWidget(buildHeader());

        auto* body = new QHBoxLayout;
        body->setContentsMargins(0, 0, 0, 0);
        body->setSpacing(18);
        body->addWidget(buildPalette());
        body->addWidget(buildEditor(), 1);
        layout->addLayout(body, 1);

        layout->addWidget(buildFooter());

        m_sheet->setFocusProxy(m_patternEdit);
        setSheetWidth(SheetWidth);
        setSheetWidget(m_sheet);

        setPattern(QStringLiteral("^(admin|root)@"));
        setSampleText(QStringLiteral("admin@jump.internal\nroot@jump.internal\nops@example.edge\nme@example.org"));

        connect(theme(), &Theme::changed, this, &RegexBuilder::applyTheme);
        applyTheme();
    }

    RegexBuilder::~RegexBuilder() = default;

    QString RegexBuilder::pattern() const
    {
        return m_patternEdit->text();
    }

    void RegexBuilder::setPattern(const QString& pattern)
    {
        m_patternEdit->setText(pattern);
    }

    QString RegexBuilder::sampleText() const
    {
        return m_sampleEdit->toPlainText();
    }

    void RegexBuilder::setSampleText(const QString& text)
    {
        m_sampleEdit->setPlainText(text);
    }

    QString RegexBuilder::flags() const
    {
        QString active;
        for (const QString& flag : {QStringLiteral("g"),
                                    QStringLiteral("i"),
                                    QStringLiteral("m"),
                                    QStringLiteral("s"),
                                    QStringLiteral("u")}) {
            RegexTokenChip* chip = m_flagChips.value(flag);
            if (chip && chip->isChecked()) {
                active.append(flag);
            }
        }
        return active;
    }

    void RegexBuilder::setFlags(const QString& flags)
    {
        for (auto it = m_flagChips.cbegin(); it != m_flagChips.cend(); ++it) {
            it.value()->setChecked(flags.contains(it.key()));
        }
        evaluate();
    }

    bool RegexBuilder::isValid() const
    {
        return m_valid;
    }

    void RegexBuilder::aboutToOpen()
    {
        evaluate();
        m_patternEdit->selectAll();
    }

    QWidget* RegexBuilder::buildHeader()
    {
        auto* header = new QWidget;
        auto* layout = new QHBoxLayout(header);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);

        auto* symbol = new QLabel;
        symbol->setObjectName(QStringLiteral("regexSymbol"));
        symbol->setPixmap(Icons::pixmap(QStringLiteral("regular_expression"), 26, theme()->color(Role::Primary)));
        layout->addWidget(symbol);

        auto* titles = new QVBoxLayout;
        titles->setContentsMargins(0, 0, 0, 0);
        titles->setSpacing(2);
        titles->addWidget(makeLabel(tr("Regex builder"), TypeRole::TitleLarge, Role::OnSurface));
        titles->addWidget(makeLabel(tr("ECMAScript (RE2-safe subset) · same engine as every search bar"),
                                    TypeRole::LabelMedium,
                                    Role::OnSurfaceVariant));
        layout->addLayout(titles, 1);

        auto* close = new IconButton(QStringLiteral("close"));
        close->setToolTip(tr("Close"));
        connect(close, &IconButton::clicked, this, &Overlay::closeOverlay);
        layout->addWidget(close);

        return header;
    }

    QWidget* RegexBuilder::buildPalette()
    {
        const QList<TokenGroup> groups = {
            {tr("Character classes"),
             {{QStringLiteral("\\d"), QStringLiteral("\\d"), 0, tr("digit")},
              {QStringLiteral("\\w"), QStringLiteral("\\w"), 0, tr("word char")},
              {QStringLiteral("\\s"), QStringLiteral("\\s"), 0, tr("whitespace")},
              {QStringLiteral("[a-z]"), QStringLiteral("[a-z]"), 0, tr("range")},
              {QString::fromUtf8("[^…]"), QStringLiteral("[^]"), 1, tr("negated set")},
              {QStringLiteral("."), QStringLiteral("."), 0, tr("any char")}}},
            {tr("Anchors"),
             {{QStringLiteral("^"), QStringLiteral("^"), 0, tr("start")},
              {QStringLiteral("$"), QStringLiteral("$"), 0, tr("end")},
              {QStringLiteral("\\b"), QStringLiteral("\\b"), 0, tr("word boundary")}}},
            {tr("Quantifiers"),
             {{QStringLiteral("*"), QStringLiteral("*"), 0, tr("0 or more")},
              {QStringLiteral("+"), QStringLiteral("+"), 0, tr("1 or more")},
              {QStringLiteral("?"), QStringLiteral("?"), 0, tr("optional")},
              {QStringLiteral("{2,4}"), QStringLiteral("{2,4}"), 0, tr("range")},
              {QStringLiteral("+?"), QStringLiteral("+?"), 0, tr("lazy")}}},
            {tr("Groups & alternation"),
             {{QStringLiteral("( )"), QStringLiteral("()"), 1, tr("capture")},
              {QStringLiteral("(?: )"), QStringLiteral("(?:)"), 1, tr("non-capturing")},
              {QStringLiteral("(?<n> )"), QStringLiteral("(?<n>)"), 1, tr("named")},
              {QStringLiteral("|"), QStringLiteral("|"), 0, tr("alternation")}}}};

        auto* palette = new QWidget;
        palette->setFixedWidth(PaletteWidth);

        auto* layout = new QVBoxLayout(palette);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);
        layout->addWidget(makeLabel(tr("Guided tokens"), TypeRole::LabelSmall, Role::OnSurfaceVariant, true));

        for (const TokenGroup& group : groups) {
            auto* block = new QWidget;
            auto* blockLayout = new QVBoxLayout(block);
            blockLayout->setContentsMargins(0, 0, 0, 0);
            blockLayout->setSpacing(ChipGap);
            blockLayout->addWidget(makeLabel(group.title, TypeRole::LabelMedium, Role::OnSurface));

            QList<QWidget*> chips;
            for (const Token& token : group.tokens) {
                auto* chip = new RegexTokenChip(token.label, false);
                chip->setToolTip(token.hint);
                const QString insert = token.insert;
                const int caretBack = token.caretBack;
                connect(chip, &QAbstractButton::clicked, this, [this, insert, caretBack] {
                    insertToken(insert, caretBack);
                });
                chips.append(chip);
            }
            wrapIntoRows(blockLayout, chips, PaletteWidth, ChipGap);
            layout->addWidget(block);
        }

        layout->addStretch(1);
        return palette;
    }

    QWidget* RegexBuilder::buildEditor()
    {
        auto* editor = new QWidget;
        auto* layout = new QVBoxLayout(editor);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);

        // The /pattern/ field with its flag chips.
        auto* patternBlock = new QWidget;
        auto* patternLayout = new QVBoxLayout(patternBlock);
        patternLayout->setContentsMargins(0, 0, 0, 0);
        patternLayout->setSpacing(6);
        patternLayout->addWidget(makeLabel(tr("Pattern"), TypeRole::LabelSmall, Role::OnSurfaceVariant, true));

        m_patternBox = new RegexPanel(Shape::Large, Role::SurfaceContainerLowest);
        m_patternBox->setFixedHeight(PatternHeight);
        auto* boxLayout = new QHBoxLayout(m_patternBox);
        boxLayout->setContentsMargins(16, 0, 11, 0);
        boxLayout->setSpacing(10);
        boxLayout->addWidget(makeLabel(QStringLiteral("/"), TypeRole::Mono, Role::OnSurfaceVariant));

        m_patternEdit = new QLineEdit;
        m_patternEdit->setFrame(false);
        m_patternEdit->setClearButtonEnabled(false);
        m_patternEdit->setPlaceholderText(tr("pattern"));
        connect(m_patternEdit, &QLineEdit::textChanged, this, &RegexBuilder::evaluate);
        boxLayout->addWidget(m_patternEdit, 1);

        boxLayout->addWidget(makeLabel(QStringLiteral("/"), TypeRole::Mono, Role::OnSurfaceVariant));
        addFlagChip(boxLayout, QStringLiteral("g"), tr("List every match, not just the first"));
        addFlagChip(boxLayout, QStringLiteral("i"), tr("Ignore case"));
        addFlagChip(boxLayout, QStringLiteral("m"), tr("^ and $ match on every line"));
        addFlagChip(boxLayout, QStringLiteral("s"), tr(". matches a newline as well"));
        addFlagChip(boxLayout, QStringLiteral("u"), tr("Unicode character properties"));
        patternLayout->addWidget(m_patternBox);

        m_statusLabel = makeLabel(QString(), TypeRole::LabelMedium, Role::OnSurfaceVariant);
        patternLayout->addWidget(m_statusLabel);
        layout->addWidget(patternBlock);

        // The sample the pattern runs against.
        auto* sampleBlock = new QWidget;
        auto* sampleLayout = new QVBoxLayout(sampleBlock);
        sampleLayout->setContentsMargins(0, 0, 0, 0);
        sampleLayout->setSpacing(8);
        sampleLayout->addWidget(makeLabel(tr("Sample text"), TypeRole::LabelSmall, Role::OnSurfaceVariant, true));

        // The frame is left in place so the stylesheet can draw the rounded border.
        m_sampleEdit = new QPlainTextEdit;
        m_sampleEdit->setFixedHeight(SampleHeight);
        m_sampleEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_sampleEdit->setTabChangesFocus(true);
        connect(m_sampleEdit, &QPlainTextEdit::textChanged, this, &RegexBuilder::evaluate);
        sampleLayout->addWidget(m_sampleEdit);
        layout->addWidget(sampleBlock);

        // The matches, rebuilt on every change.
        auto* matchBlock = new QWidget;
        auto* matchLayout = new QVBoxLayout(matchBlock);
        matchLayout->setContentsMargins(0, 0, 0, 0);
        matchLayout->setSpacing(8);
        // The match count belongs to the status line under the pattern field,
        // so this heading is static.
        matchLayout->addWidget(
            makeLabel(tr("Matches & capture groups"), TypeRole::LabelSmall, Role::OnSurfaceVariant, true));

        m_matchPanel = new RegexPanel(Shape::Large, Role::SurfaceContainer);
        m_matchPanel->setMinimumHeight(MatchMinHeight);
        m_matchPanel->setMaximumHeight(MatchMaxHeight);
        auto* panelLayout = new QVBoxLayout(m_matchPanel);
        panelLayout->setContentsMargins(0, 0, 0, 0);

        auto* scroll = new QScrollArea;
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;border:none;}"
                                             "QScrollArea > QWidget > QWidget{background:transparent;}"));

        auto* rows = new QWidget;
        m_matchLayout = new QVBoxLayout(rows);
        m_matchLayout->setContentsMargins(16, 12, 16, 12);
        m_matchLayout->setSpacing(6);
        scroll->setWidget(rows);
        panelLayout->addWidget(scroll);
        matchLayout->addWidget(m_matchPanel);
        layout->addWidget(matchBlock);

        layout->addStretch(1);
        return editor;
    }

    QWidget* RegexBuilder::buildFooter()
    {
        auto* footer = new QWidget;
        auto* layout = new QHBoxLayout(footer);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        auto* hint = makeLabel(tr("Evaluated locally with a bounded step budget. Patterns and sample text are "
                                  "never transmitted or persisted."),
                               TypeRole::LabelMedium,
                               Role::OnSurfaceVariant);
        hint->setWordWrap(true);
        layout->addWidget(hint, 1);

        m_copyButton = new OutlinedButton(QStringLiteral("content_copy"), tr("Copy"));
        connect(m_copyButton, &ButtonBase::clicked, this, [this] { emit patternCopied(pattern()); });
        layout->addWidget(m_copyButton);

        m_applyButton = new FilledButton(QString(), tr("Apply to search"));
        connect(m_applyButton, &ButtonBase::clicked, this, [this] {
            emit patternApplied(pattern());
            closeOverlay();
        });
        layout->addWidget(m_applyButton);

        return footer;
    }

    void RegexBuilder::addFlagChip(QHBoxLayout* row, const QString& flag, const QString& hint)
    {
        auto* chip = new RegexTokenChip(flag, true);
        chip->setFixedSize(FlagChipSize, FlagChipSize);
        chip->setToolTip(hint);
        // The design opens with case insensitivity alone.
        chip->setChecked(flag == QLatin1String("i"));
        connect(chip, &QAbstractButton::toggled, this, &RegexBuilder::evaluate);
        row->addWidget(chip);
        m_flagChips.insert(flag, chip);
    }

    void RegexBuilder::insertToken(const QString& token, int caretBack)
    {
        m_patternEdit->insert(token);
        if (caretBack > 0) {
            m_patternEdit->setCursorPosition(qMax(0, m_patternEdit->cursorPosition() - caretBack));
        }
        m_patternEdit->setFocus(Qt::OtherFocusReason);
    }

    void RegexBuilder::evaluate()
    {
        const QString pattern = m_patternEdit->text();
        const QString sample = m_sampleEdit->toPlainText();

        auto flagOn = [this](const QString& flag) {
            RegexTokenChip* chip = m_flagChips.value(flag);
            return chip && chip->isChecked();
        };

        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (flagOn(QStringLiteral("i"))) {
            options |= QRegularExpression::CaseInsensitiveOption;
        }
        if (flagOn(QStringLiteral("m"))) {
            options |= QRegularExpression::MultilineOption;
        }
        if (flagOn(QStringLiteral("s"))) {
            options |= QRegularExpression::DotMatchesEverythingOption;
        }
        if (flagOn(QStringLiteral("u"))) {
            options |= QRegularExpression::UseUnicodePropertiesOption;
        }

        const QRegularExpression regex(pattern, options);
        m_valid = regex.isValid();

        while (QLayoutItem* item = m_matchLayout->takeAt(0)) {
            delete item->widget();
            delete item;
        }

        int count = 0;
        if (m_valid && !pattern.isEmpty() && !sample.isEmpty()) {
            QList<QRegularExpressionMatch> matches;
            if (flagOn(QStringLiteral("g"))) {
                auto iterator = regex.globalMatch(sample);
                while (iterator.hasNext() && matches.size() < MaxMatches) {
                    matches.append(iterator.next());
                }
            } else {
                const QRegularExpressionMatch match = regex.match(sample);
                if (match.hasMatch()) {
                    matches.append(match);
                }
            }
            count = matches.size();

            // The rows never scroll sideways, so long matches are elided to
            // whatever the panel can actually show.
            const QFontMetrics metrics{monoFont()};
            const int panelWidth = m_matchPanel->width() > 0 ? m_matchPanel->width() : DefaultEditorWidth;
            const int textBudget = qMax(160, panelWidth - MatchPosWidth - 120);

            for (const QRegularExpressionMatch& match : matches) {
                QString text = match.captured();
                text.replace(QLatin1Char('\n'), QLatin1String("\\n"));
                // A zero-width match still takes a row, so it says so.
                text = text.isEmpty() ? tr("(empty)") : metrics.elidedText(text, Qt::ElideRight, textBudget);

                QStringList groups;
                for (int index = 1; index <= match.lastCapturedIndex(); ++index) {
                    const QString captured = match.captured(index);
                    if (!captured.isEmpty()) {
                        groups.append(captured);
                    }
                }
                // The captures column is always drawn, so the three columns line
                // up down the panel; a pattern with no groups shows an em dash.
                const QString captures =
                    match.lastCapturedIndex() > 0 ? groups.join(QLatin1String(" | ")) : QString::fromUtf8("—");

                auto* row = new QWidget;
                auto* rowLayout = new QHBoxLayout(row);
                rowLayout->setContentsMargins(0, 0, 0, 0);
                rowLayout->setSpacing(12);

                // Zero padded to three digits so the column stays aligned
                // however far into the sample a match starts.
                auto* position = makeLabel(QStringLiteral("%1").arg(match.capturedStart(), 3, 10, QLatin1Char('0')),
                                           TypeRole::Mono,
                                           Role::OnSurfaceVariant);
                position->setMinimumWidth(MatchPosWidth);
                rowLayout->addWidget(position);
                rowLayout->addWidget(makeLabel(text, TypeRole::Mono, Role::OnSurface), 1);
                rowLayout->addWidget(makeLabel(captures, TypeRole::Mono, Role::OnSurfaceVariant));
                m_matchLayout->addWidget(row);
            }
        }

        if (count == 0) {
            const QString empty = m_valid ? tr("No matches in the sample.") : tr("The pattern does not compile.");
            m_matchLayout->addWidget(makeLabel(empty, TypeRole::Mono, Role::OnSurfaceVariant));
        }
        m_matchLayout->addStretch(1);

        if (pattern.isEmpty()) {
            m_statusLabel->setText(tr("Enter a pattern to preview its matches."));
            styleLabel(m_statusLabel, TypeRole::LabelMedium, Role::OnSurfaceVariant);
            m_patternBox->setBorder(theme()->color(Role::Outline), 2);
        } else if (m_valid) {
            m_statusLabel->setText(tr("Valid - %n match(es) in the sample", "", count));
            styleLabel(m_statusLabel, TypeRole::LabelMedium, Role::Green);
            m_patternBox->setBorder(theme()->color(Role::Primary), 2);
        } else {
            m_statusLabel->setText(
                tr("Syntax error at %1: %2").arg(regex.patternErrorOffset()).arg(regex.errorString()));
            styleLabel(m_statusLabel, TypeRole::LabelMedium, Role::Error);
            m_patternBox->setBorder(theme()->color(Role::Error), 2);
        }

        const bool usable = m_valid && !pattern.isEmpty();
        m_copyButton->setEnabled(usable);
        m_applyButton->setEnabled(usable);
    }

    void RegexBuilder::applyTheme()
    {
        restyleLabels(m_sheet);

        if (auto* symbol = m_sheet->findChild<QLabel*>(QStringLiteral("regexSymbol"))) {
            symbol->setPixmap(Icons::pixmap(QStringLiteral("regular_expression"), 26, theme()->color(Role::Primary)));
        }

        m_patternEdit->setFont(monoFont(1));
        m_patternEdit->setStyleSheet(QStringLiteral("QLineEdit{border:none;background:transparent;padding:0;"
                                                    "color:%1;selection-background-color:%2;selection-color:%3;}")
                                         .arg(theme()->hex(Role::OnSurface),
                                              theme()->hex(Role::SecondaryContainer),
                                              theme()->hex(Role::OnSecondaryContainer)));

        m_sampleEdit->setFont(monoFont(-1));
        m_sampleEdit->setStyleSheet(
            QStringLiteral("QPlainTextEdit{border:1px solid %1;border-radius:%2px;"
                           "background:%3;color:%4;padding:14px 16px;}")
                .arg(theme()->hex(Role::Outline))
                .arg(Shape::Large)
                .arg(theme()->hex(Role::SurfaceContainerLowest), theme()->hex(Role::OnSurface)));

        for (RegexTokenChip* chip : std::as_const(m_flagChips)) {
            chip->setFont(monoFont(-1));
        }

        evaluate();
        m_sheet->update();
    }

} // namespace Material
