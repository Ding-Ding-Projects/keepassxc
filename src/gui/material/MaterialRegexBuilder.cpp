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

#include "MaterialRegexLab.h"
#include "MaterialRegexSafety.h"
#include "MaterialSegmentedButton.h"

#include "MaterialButtons.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"
#include "MaterialVaultSidebar.h"

#include <QAbstractButton>
#include <QMouseEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDrag>
#include <QApplication>
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
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>
#include <QVariant>

#include <functional>
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
    /**
     * One token block of the design: a coloured mono chip with a drag handle.
     * Click removes it from the pattern; drag it onto another block to move it
     * there; with the keyboard, Delete removes and Ctrl+Left / Ctrl+Right move.
     */
    class RegexTokenBlock : public QAbstractButton
    {
    public:
        RegexTokenBlock(int index, const RegexLab::Token& token, QWidget* parent = nullptr)
            : QAbstractButton(parent)
            , m_index(index)
            , m_type(token.type)
        {
            setText(token.text);
            setFont(monoFont(-1));
            setCursor(Qt::OpenHandCursor);
            setFocusPolicy(Qt::TabFocus);
            setAcceptDrops(true);
            setToolTip(RegexBuilder::tr("%1 · %2 (click to remove, drag to move)").arg(token.english, token.cantonese));
            setAccessibleName(RegexBuilder::tr("Token block %1: %2").arg(token.text, token.english));
            setAccessibleDescription(RegexBuilder::tr("Delete removes it; Ctrl+Left and Ctrl+Right move it."));
        }

        int index() const
        {
            return m_index;
        }

        std::function<void(int from, int to)> onMove;
        std::function<void(int index)> onRemove;

        QSize sizeHint() const override
        {
            return {fontMetrics().horizontalAdvance(text()) + 20 + 16 + 6, 30};
        }

        QSize minimumSizeHint() const override
        {
            return sizeHint();
        }

        static QPair<Role, Role> colours(const QString& type)
        {
            if (type == QLatin1String("charclass") || type == QLatin1String("class") || type == QLatin1String("unicode")) {
                return {Role::SecondaryContainer, Role::OnSecondaryContainer};
            }
            if (type == QLatin1String("group") || type == QLatin1String("backref")) {
                return {Role::PrimaryContainer, Role::OnPrimaryContainer};
            }
            if (type == QLatin1String("look")) {
                return {Role::AmberContainer, Role::OnAmberContainer};
            }
            if (type == QLatin1String("quant")) {
                return {Role::GreenContainer, Role::OnGreenContainer};
            }
            if (type == QLatin1String("anchor")) {
                return {Role::ErrorContainer, Role::OnErrorContainer};
            }
            if (type == QLatin1String("alt") || type == QLatin1String("dot")) {
                return {Role::SurfaceContainerHighest, Role::OnSurface};
            }
            if (type == QLatin1String("escape")) {
                return {Role::SurfaceContainer, Role::OnSurfaceVariant};
            }
            return {Role::SurfaceContainer, Role::OnSurface};
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const auto roles = colours(m_type);
            paintSurface(&painter, rect(), Shape::Small, theme()->color(roles.first));
            if (hasFocus() || m_dropTarget) {
                painter.setPen(QPen(theme()->color(Role::Primary), 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), Shape::Small, Shape::Small);
            }
            QColor handle = theme()->color(roles.second);
            handle.setAlphaF(0.65);
            Icons::symbol(QStringLiteral("drag_indicator"), handle).paint(&painter, QRect(6, (height() - 14) / 2, 14, 14));
            painter.setFont(font());
            painter.setPen(theme()->color(roles.second));
            painter.drawText(rect().adjusted(24, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, text());
        }

        void mousePressEvent(QMouseEvent* event) override
        {
            m_pressPos = event->pos();
            QAbstractButton::mousePressEvent(event);
        }

        void mouseMoveEvent(QMouseEvent* event) override
        {
            if (!(event->buttons() & Qt::LeftButton) || (event->pos() - m_pressPos).manhattanLength() < QApplication::startDragDistance()) {
                QAbstractButton::mouseMoveEvent(event);
                return;
            }
            auto* drag = new QDrag(this);
            auto* mime = new QMimeData;
            mime->setData(QStringLiteral("application/x-kpxc-regex-token"), QByteArray::number(m_index));
            drag->setMimeData(mime);
            drag->setPixmap(grab());
            setDown(false);
            drag->exec(Qt::MoveAction);
        }

        void dragEnterEvent(QDragEnterEvent* event) override
        {
            if (event->mimeData()->hasFormat(QStringLiteral("application/x-kpxc-regex-token"))) {
                m_dropTarget = true;
                update();
                event->acceptProposedAction();
            }
        }

        void dragLeaveEvent(QDragLeaveEvent*) override
        {
            m_dropTarget = false;
            update();
        }

        void dropEvent(QDropEvent* event) override
        {
            m_dropTarget = false;
            update();
            const int from = event->mimeData()->data(QStringLiteral("application/x-kpxc-regex-token")).toInt();
            if (onMove && from != m_index) {
                onMove(from, m_index);
            }
            event->acceptProposedAction();
        }

        void keyPressEvent(QKeyEvent* event) override
        {
            if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
                if (onRemove) onRemove(m_index);
                return;
            }
            if (event->modifiers().testFlag(Qt::ControlModifier) && event->key() == Qt::Key_Left) {
                if (onMove && m_index > 0) onMove(m_index, m_index - 1);
                return;
            }
            if (event->modifiers().testFlag(Qt::ControlModifier) && event->key() == Qt::Key_Right) {
                if (onMove) onMove(m_index, m_index + 1);
                return;
            }
            QAbstractButton::keyPressEvent(event);
        }

    private:
        int m_index;
        QString m_type;
        QPoint m_pressPos;
        bool m_dropTarget = false;
    };

    /** The dashed drop zone the token blocks sit in. */
    class RegexTokenStrip : public QWidget
    {
    public:
        explicit RegexTokenStrip(QWidget* parent = nullptr)
            : QWidget(parent)
        {
            setMinimumHeight(44);
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            paintSurface(&painter, rect(), Shape::Large, theme()->color(Role::SurfaceContainerLowest));
            QPen pen(theme()->color(Role::OutlineVariant), 1, Qt::DashLine);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Shape::Large, Shape::Large);
        }
    };

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
        m_subtitle = makeLabel(tr("QRegularExpression (PCRE2) runs here · the same engine as every search bar"),
                               TypeRole::LabelMedium,
                               Role::OnSurfaceVariant);
        titles->addWidget(m_subtitle);
        layout->addLayout(titles, 1);

        // The dialect switch sits in the header, as the design places it.
        m_dialects = new SegmentedButton;
        m_dialects->setObjectName(QStringLiteral("regexDialects"));
        m_dialects->addSegment(QStringLiteral("js"), tr("ECMAScript"));
        m_dialects->addSegment(QStringLiteral("qt"), tr("QRegularExpression"));
        m_dialects->addSegment(QStringLiteral("both"), tr("Both"));
        m_dialects->setCurrentSegment(m_dialect);
        connect(m_dialects, &SegmentedButton::segmentSelected, this, [this](const QString& id) { setDialect(id); });
        layout->addWidget(m_dialects);

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

        layout->addWidget(buildLibrary(), 1);
        return palette;
    }

    QWidget* RegexBuilder::buildLibrary()
    {
        // The pattern library: every preset the design ships, filtered by its
        // own search field, applied with one click.
        auto* library = new QWidget;
        auto* layout = new QVBoxLayout(library);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        layout->addWidget(makeLabel(tr("Pattern library"), TypeRole::LabelSmall, Role::OnSurfaceVariant, true));

        m_librarySearch = new QLineEdit;
        m_librarySearch->setObjectName(QStringLiteral("regexLibrarySearch"));
        m_librarySearch->setPlaceholderText(tr("Search presets"));
        m_librarySearch->setClearButtonEnabled(true);
        m_librarySearch->setAccessibleName(tr("Search the pattern library"));
        connect(m_librarySearch, &QLineEdit::textChanged, this, &RegexBuilder::filterLibrary);
        layout->addWidget(m_librarySearch);

        auto* scroll = new QScrollArea;
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;border:none;}"
                                             "QScrollArea > QWidget > QWidget{background:transparent;}"));
        auto* rows = new QWidget;
        m_libraryLayout = new QVBoxLayout(rows);
        m_libraryLayout->setContentsMargins(0, 0, 0, 0);
        m_libraryLayout->setSpacing(4);
        for (const RegexLab::Preset& preset : RegexLab::presets()) {
            auto* row = new RegexTokenChip(QStringLiteral("%1 · %2").arg(preset.name, preset.cantonese), false);
            row->setObjectName(QStringLiteral("regexPreset_") + preset.id);
            row->setToolTip(preset.pattern);
            row->setAccessibleName(tr("Preset %1: %2").arg(preset.name, preset.pattern));
            row->setProperty("presetId", preset.id);
            row->setProperty("haystack", (preset.name + QLatin1Char(' ') + preset.cantonese + QLatin1Char(' ') + preset.pattern).toLower());
            connect(row, &QAbstractButton::clicked, this, [this, id = preset.id] { applyPreset(id); });
            m_libraryLayout->addWidget(row);
            m_libraryRows.append(row);
        }
        m_libraryLayout->addStretch(1);
        scroll->setWidget(rows);
        layout->addWidget(scroll, 1);
        return library;
    }

    void RegexBuilder::filterLibrary(const QString& query)
    {
        const QString needle = query.trimmed().toLower();
        for (QAbstractButton* row : m_libraryRows) {
            row->setVisible(needle.isEmpty() || row->property("haystack").toString().contains(needle));
        }
    }

    bool RegexBuilder::applyPreset(const QString& id)
    {
        for (const RegexLab::Preset& preset : RegexLab::presets()) {
            if (preset.id == id) {
                setSampleText(preset.sample);
                setFlags(preset.flags);
                setPattern(preset.pattern);
                setCurrentTab(QStringLiteral("matches"));
                return true;
            }
        }
        return false;
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

        // The design's token blocks: the pattern as coloured pieces to remove
        // by clicking or reorder by dragging.
        auto* stripHeader = new QHBoxLayout;
        stripHeader->setContentsMargins(0, 4, 0, 0);
        stripHeader->setSpacing(8);
        stripHeader->addWidget(makeLabel(tr("Token blocks"), TypeRole::LabelSmall, Role::OnSurfaceVariant, true));
        auto* stripRule = new QFrame;
        stripRule->setFrameShape(QFrame::HLine);
        stripRule->setFixedHeight(1);
        stripRule->setStyleSheet(QStringLiteral("background:%1;border:none;").arg(theme()->hex(Role::OutlineVariant)));
        stripHeader->addWidget(stripRule, 1);
        stripHeader->addWidget(makeLabel(tr("click to remove · drag to reorder"), TypeRole::LabelSmall, Role::Outline));
        layout->addLayout(stripHeader);
        m_tokenStrip = new RegexTokenStrip;
        m_tokenStrip->setObjectName(QStringLiteral("regexTokenStrip"));
        m_tokenStrip->setAccessibleName(tr("Token blocks"));
        auto* stripLayout = new FlowLayout(m_tokenStrip, 5, 5);
        stripLayout->setContentsMargins(9, 9, 9, 9);
        m_tokenStripLayout = stripLayout;
        m_tokenStripEmpty = makeLabel(tr("Type a pattern and its pieces appear here."), TypeRole::LabelMedium, Role::Outline);
        m_tokenStripEmpty->setObjectName(QStringLiteral("regexTokenStripEmpty"));
        stripLayout->addWidget(m_tokenStripEmpty);
        layout->addWidget(m_tokenStrip);

        // The workbench: which dialect is described, and which pane is open.
        auto* controls = new QHBoxLayout;
        controls->setContentsMargins(0, 0, 0, 0);
        controls->setSpacing(12);
        m_tabs = new SegmentedButton;
        m_tabs->setObjectName(QStringLiteral("regexWorkbenchTabs"));
        m_tabs->addSegment(QStringLiteral("matches"), tr("Matches"));
        m_tabs->addSegment(QStringLiteral("explain"), tr("Explain"));
        m_tabs->addSegment(QStringLiteral("replace"), tr("Replace"));
        m_tabs->addSegment(QStringLiteral("export"), tr("Export"));
        m_tabs->addSegment(QStringLiteral("cheat"), tr("Cheat sheet"));
        m_tabs->addSegment(QStringLiteral("dialect"), tr("Dialects"));
        connect(m_tabs, &SegmentedButton::segmentSelected, this, [this](const QString& id) { setCurrentTab(id); });
        controls->addWidget(m_tabs, 1);
        layout->addLayout(controls);

        m_pages = new QStackedWidget;
        m_pages->setObjectName(QStringLiteral("regexWorkbenchPages"));
        auto* matchesPage = new QWidget;
        auto* matchesLayout = new QVBoxLayout(matchesPage);
        matchesLayout->setContentsMargins(0, 0, 0, 0);
        matchesLayout->setSpacing(14);

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
        matchesLayout->addWidget(sampleBlock);

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
        matchesLayout->addWidget(matchBlock);
        matchesLayout->addStretch(1);

        m_pages->addWidget(matchesPage);
        m_pages->addWidget(buildExplainPage());
        m_pages->addWidget(buildReplacePage());
        m_pages->addWidget(buildExportPage());
        m_pages->addWidget(buildCheatPage());
        m_pages->addWidget(buildDialectPage());
        layout->addWidget(m_pages, 1);
        return editor;
    }

    namespace
    {
        QScrollArea* transparentScroll(QWidget* content)
        {
            auto* scroll = new QScrollArea;
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setWidgetResizable(true);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;border:none;}"
                                                 "QScrollArea > QWidget > QWidget{background:transparent;}"));
            scroll->setWidget(content);
            return scroll;
        }
    } // namespace

    void RegexBuilder::clearLayout(QLayout* layout)
    {
        while (QLayoutItem* item = layout->takeAt(0)) {
            delete item->widget();
            delete item;
        }
    }

    QWidget* RegexBuilder::buildExplainPage()
    {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        layout->addWidget(makeLabel(tr("Token by token"), TypeRole::LabelSmall, Role::OnSurfaceVariant, true));
        auto* panel = new RegexPanel(Shape::Large, Role::SurfaceContainer);
        auto* panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(0, 0, 0, 0);
        auto* rows = new QWidget;
        m_explainLayout = new QVBoxLayout(rows);
        m_explainLayout->setContentsMargins(16, 12, 16, 12);
        m_explainLayout->setSpacing(6);
        panelLayout->addWidget(transparentScroll(rows));
        layout->addWidget(panel, 1);
        return page;
    }

    void RegexBuilder::rebuildExplain(const QString& pattern)
    {
        clearLayout(m_explainLayout);
        const auto tokens = RegexLab::tokenize(pattern);
        if (tokens.isEmpty()) {
            m_explainLayout->addWidget(
                makeLabel(tr("Nothing to explain yet - the pattern is empty."), TypeRole::LabelMedium, Role::OnSurfaceVariant));
        }
        for (const RegexLab::Token& token : tokens) {
            auto* row = new QWidget;
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(12);
            auto* chip = new RegexTokenChip(token.text, false);
            chip->setToolTip(token.type);
            chip->setAccessibleName(tr("Token %1: %2").arg(token.text, token.english));
            connect(chip, &QAbstractButton::clicked, this, [this, token] {
                m_patternEdit->setSelection(token.start, token.end - token.start);
                m_patternEdit->setFocus(Qt::OtherFocusReason);
            });
            rowLayout->addWidget(chip);
            auto* text = makeLabel(token.pcreOnly ? tr("%1 (PCRE2 only)").arg(token.english) : token.english,
                                   TypeRole::BodySmall,
                                   Role::OnSurface);
            text->setWordWrap(true);
            rowLayout->addWidget(text, 1);
            rowLayout->addWidget(makeLabel(token.cantonese, TypeRole::BodySmall, Role::OnSurfaceVariant));
            m_explainLayout->addWidget(row);
        }
        m_explainLayout->addStretch(1);
    }

    void RegexBuilder::rebuildTokenStrip(const QString& pattern)
    {
        if (!m_tokenStrip) {
            return;
        }
        const auto children = m_tokenStrip->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget* child : children) {
            if (child != m_tokenStripEmpty) {
                // A block may be rebuilding from its own click, so it is detached
                // now and deleted once its signal has returned.
                m_tokenStripLayout->removeWidget(child);
                child->hide();
                child->setParent(nullptr);
                child->deleteLater();
            }
        }
        const auto tokens = RegexLab::tokenize(pattern);
        m_tokenStripEmpty->setVisible(tokens.isEmpty());
        auto texts = QStringList();
        for (const auto& token : tokens) {
            texts << token.text;
        }
        for (int index = 0; index < tokens.size(); ++index) {
            auto* block = new RegexTokenBlock(index, tokens.at(index), m_tokenStrip);
            block->setObjectName(QStringLiteral("regexTokenBlock_%1").arg(index));
            block->onRemove = [this, texts](int at) {
                QStringList next = texts;
                if (at >= 0 && at < next.size()) {
                    next.removeAt(at);
                }
                setPattern(next.join(QString()));
            };
            block->onMove = [this, texts](int from, int to) {
                QStringList next = texts;
                if (from < 0 || from >= next.size()) {
                    return;
                }
                const QString piece = next.takeAt(from);
                next.insert(qBound(0, to, next.size()), piece);
                setPattern(next.join(QString()));
            };
            connect(block, &QAbstractButton::clicked, this, [block] {
                if (block->onRemove) block->onRemove(block->index());
            });
            m_tokenStripLayout->addWidget(block);
        }
        m_tokenStrip->setAccessibleDescription(tokens.isEmpty() ? tr("No token blocks") : tr("%n token block(s)", "", tokens.size()));
    }

    QWidget* RegexBuilder::buildReplacePage()
    {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        layout->addWidget(makeLabel(tr("Replacement · $1 $<name> $&"), TypeRole::LabelSmall, Role::OnSurfaceVariant, true));
        m_replaceEdit = new QLineEdit;
        m_replaceEdit->setObjectName(QStringLiteral("regexReplacement"));
        m_replaceEdit->setPlaceholderText(tr("replacement"));
        m_replaceEdit->setAccessibleName(tr("Replacement template"));
        connect(m_replaceEdit, &QLineEdit::textChanged, this, &RegexBuilder::rebuildReplace);
        layout->addWidget(m_replaceEdit);
        layout->addWidget(makeLabel(tr("Preview"), TypeRole::LabelSmall, Role::OnSurfaceVariant, true));
        m_replacePreview = new QPlainTextEdit;
        m_replacePreview->setObjectName(QStringLiteral("regexReplacePreview"));
        m_replacePreview->setReadOnly(true);
        m_replacePreview->setLineWrapMode(QPlainTextEdit::NoWrap);
        layout->addWidget(m_replacePreview, 1);
        m_replaceNote = makeLabel(tr("QString::replace() takes \\1 and \\0, never $1 or $&; the Export tab writes that form. "
                                     "Named references resolve through the pattern's own group order."),
                                  TypeRole::LabelMedium,
                                  Role::OnSurfaceVariant);
        m_replaceNote->setWordWrap(true);
        layout->addWidget(m_replaceNote);
        return page;
    }

    void RegexBuilder::rebuildReplace()
    {
        if (!m_replacePreview) {
            return;
        }
        const QString pattern = m_patternEdit->text();
        if (pattern.isEmpty() || !m_valid) {
            m_replacePreview->setPlainText(m_valid ? tr("Enter a pattern to preview replacements.")
                                                   : tr("The pattern does not compile."));
            return;
        }
        QString sample = m_sampleEdit->toPlainText();
        const QString translated = RegexLab::qtReplacement(pattern, m_replaceEdit->text());
        sample.replace(QRegularExpression(pattern, optionsForFlags(flags())), translated);
        m_replacePreview->setPlainText(sample);
    }

    QString RegexBuilder::replacement() const
    {
        return m_replaceEdit ? m_replaceEdit->text() : QString();
    }

    void RegexBuilder::setReplacement(const QString& text)
    {
        if (m_replaceEdit) {
            m_replaceEdit->setText(text);
        }
    }

    QString RegexBuilder::replacementPreview() const
    {
        return m_replacePreview ? m_replacePreview->toPlainText() : QString();
    }

    QWidget* RegexBuilder::buildExportPage()
    {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        auto* rows = new QWidget;
        m_exportLayout = new QVBoxLayout(rows);
        m_exportLayout->setContentsMargins(0, 0, 0, 0);
        m_exportLayout->setSpacing(12);
        layout->addWidget(transparentScroll(rows), 1);
        return page;
    }

    void RegexBuilder::rebuildExport()
    {
        clearLayout(m_exportLayout);
        const QString pattern = m_patternEdit->text();
        for (const RegexLab::Export& item : RegexLab::translate(pattern, flags())) {
            if (m_dialect == QLatin1String("js") && item.id == QLatin1String("qt")) {
                continue;
            }
            if (m_dialect == QLatin1String("qt") && item.id == QLatin1String("js")) {
                continue;
            }
            auto* block = new QWidget;
            auto* blockLayout = new QVBoxLayout(block);
            blockLayout->setContentsMargins(0, 0, 0, 0);
            blockLayout->setSpacing(4);
            auto* head = new QHBoxLayout;
            head->setContentsMargins(0, 0, 0, 0);
            head->addWidget(makeLabel(item.label, TypeRole::LabelSmall, Role::OnSurfaceVariant, true), 1);
            auto* copy = new OutlinedButton(QStringLiteral("content_copy"), tr("Copy"));
            copy->setObjectName(QStringLiteral("regexExportCopy_") + item.id);
            copy->setEnabled(!pattern.isEmpty());
            connect(copy, &ButtonBase::clicked, this, [this, code = item.code] { emit patternCopied(code); });
            head->addWidget(copy);
            blockLayout->addLayout(head);
            auto* code = new QPlainTextEdit;
            code->setReadOnly(true);
            code->setPlainText(pattern.isEmpty() ? tr("(enter a pattern)") : item.code);
            code->setLineWrapMode(QPlainTextEdit::NoWrap);
            code->setFixedHeight(item.id == QLatin1String("qt") ? 96 : 44);
            code->setAccessibleName(tr("%1 export").arg(item.label));
            blockLayout->addWidget(code);
            m_exportLayout->addWidget(block);
        }
        m_exportLayout->addStretch(1);
    }

    QWidget* RegexBuilder::buildCheatPage()
    {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        auto* rows = new QWidget;
        auto* rowsLayout = new QVBoxLayout(rows);
        rowsLayout->setContentsMargins(0, 0, 0, 0);
        rowsLayout->setSpacing(4);
        for (const RegexLab::CheatEntry& entry : RegexLab::cheatSheet()) {
            auto* row = new QWidget;
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(12);
            auto* chip = new RegexTokenChip(entry.token, false);
            chip->setAccessibleName(tr("Insert %1: %2").arg(entry.token, entry.english));
            connect(chip, &QAbstractButton::clicked, this, [this, token = entry.token] { insertToken(token, 0); });
            rowLayout->addWidget(chip);
            auto* text = makeLabel(entry.english, TypeRole::BodySmall, Role::OnSurface);
            text->setWordWrap(true);
            rowLayout->addWidget(text, 1);
            rowLayout->addWidget(makeLabel(entry.cantonese, TypeRole::BodySmall, Role::OnSurfaceVariant));
            rowLayout->addWidget(makeLabel(entry.pcreOnly ? tr("PCRE2 only") : tr("both engines"),
                                           TypeRole::LabelSmall,
                                           entry.pcreOnly ? Role::Primary : Role::OnSurfaceVariant));
            rowsLayout->addWidget(row);
        }
        rowsLayout->addStretch(1);
        layout->addWidget(transparentScroll(rows), 1);
        return page;
    }

    QWidget* RegexBuilder::buildDialectPage()
    {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        auto* cards = new QWidget;
        m_dialectLayout = new QVBoxLayout(cards);
        m_dialectLayout->setContentsMargins(0, 0, 0, 0);
        m_dialectLayout->setSpacing(12);
        layout->addWidget(transparentScroll(cards), 1);
        rebuildDialectPage();
        return page;
    }

    void RegexBuilder::rebuildDialectPage()
    {
        clearLayout(m_dialectLayout);
        for (const RegexLab::Dialect& dialect : RegexLab::dialects()) {
            if (m_dialect != QLatin1String("both") && m_dialect != dialect.id) {
                continue;
            }
            auto* card = new RegexPanel(Shape::Large, Role::SurfaceContainer);
            auto* cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(16, 12, 16, 12);
            cardLayout->setSpacing(6);
            auto* head = new QHBoxLayout;
            head->addWidget(makeLabel(QStringLiteral("%1 · %2").arg(dialect.label, dialect.cantonese), TypeRole::TitleSmall, Role::OnSurface), 1);
            head->addWidget(makeLabel(dialect.id == QLatin1String("qt") ? tr("RUNS HERE") : tr("DESCRIBED ONLY"),
                                      TypeRole::LabelSmall,
                                      dialect.id == QLatin1String("qt") ? Role::Green : Role::OnSurfaceVariant));
            cardLayout->addLayout(head);
            for (const RegexLab::DialectFlag& flag : dialect.flags) {
                cardLayout->addWidget(makeLabel(QStringLiteral("%1  %2 · %3").arg(flag.flag, flag.english, flag.cantonese),
                                                TypeRole::BodySmall,
                                                Role::OnSurface));
            }
            for (const QString& note : dialect.notes) {
                auto* label = makeLabel(note, TypeRole::BodySmall, Role::OnSurfaceVariant);
                label->setWordWrap(true);
                cardLayout->addWidget(label);
            }
            m_dialectLayout->addWidget(card);
        }
        m_dialectLayout->addStretch(1);
    }

    QString RegexBuilder::currentTab() const
    {
        return m_tabs ? m_tabs->currentSegment() : QString();
    }

    void RegexBuilder::setCurrentTab(const QString& id)
    {
        static const QStringList order = {QStringLiteral("matches"), QStringLiteral("explain"), QStringLiteral("replace"),
                                          QStringLiteral("export"), QStringLiteral("cheat"), QStringLiteral("dialect")};
        const int index = order.indexOf(id);
        if (index < 0 || !m_pages) {
            return;
        }
        m_pages->setCurrentIndex(index);
        if (m_tabs && m_tabs->currentSegment() != id) {
            m_tabs->setCurrentSegment(id);
        }
    }

    QString RegexBuilder::dialect() const
    {
        return m_dialect;
    }

    void RegexBuilder::setDialect(const QString& id)
    {
        if (id != QLatin1String("js") && id != QLatin1String("qt") && id != QLatin1String("both")) {
            return;
        }
        m_dialect = id;
        if (m_dialects && m_dialects->currentSegment() != id) {
            m_dialects->setCurrentSegment(id);
        }
        if (m_flagCaption) {
            m_flagCaption->setText(id == QLatin1String("qt") ? tr("QRegularExpression::PatternOptions") : tr("RegExp flags"));
        }
        rebuildExport();
        rebuildDialectPage();
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

        // The other panes follow the pattern; the explain pane only when the
        // pattern itself changed, because it does not depend on the sample.
        if (m_explainLayout && pattern != m_lastPattern) {
            rebuildExplain(pattern);
            rebuildTokenStrip(pattern);
            m_lastPattern = pattern;
        }
        rebuildReplace();
        if (m_exportLayout) {
            rebuildExport();
        }
        if (m_tabs) {
            const int tokens = RegexLab::tokenize(pattern).size();
            m_tabs->setSegmentLabel(QStringLiteral("matches"), count > 0 ? tr("Matches %1").arg(count) : tr("Matches"));
            m_tabs->setSegmentLabel(QStringLiteral("explain"), tokens > 0 ? tr("Explain %1").arg(tokens) : tr("Explain"));
        }
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
