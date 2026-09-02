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

#include "MaterialEntryDetail.h"

#include "MaterialButtons.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialSearchBar.h"

#include <QAbstractButton>
#include <QDateTime>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace Material
{
    namespace
    {
        // Metrics taken straight from the mockup's detail pane.
        constexpr int PaneMargin = 16; // card gutter
        constexpr int TextMargin = 20; // header and overline gutter
        constexpr int SymbolTileSize = 52;
        constexpr int SymbolGlyphSize = 28;
        constexpr int RowButtonSize = 36;
        constexpr int RowGlyphSize = 19;
        constexpr int UsernameRowHeight = 56;
        constexpr int TotpRowHeight = 66;
        constexpr int AttachmentRowHeight = 48;
        constexpr int HistoryRowHeight = 48;
        constexpr int RingSize = 36;
        constexpr int RingDiscSize = 26;
        constexpr int MeterHeight = 6;
        constexpr int DividerInset = 16;
        constexpr int TotpStep = 30;
        // The mask is a fixed length so that it does not leak the password's.
        constexpr int MaskedLength = 16;
        constexpr int HeroButtonSize = 36;
        constexpr int HealthChipHeight = 26;
        constexpr int HealthChipPaddingX = 11;
        constexpr int HealthChipGlyph = 14;
        constexpr int FieldRowMinHeight = 56;
        constexpr int FieldButtonSize = 38;
        constexpr int FooterButtonHeight = 44;

        QString maskedPassword()
        {
            return QString(MaskedLength, QChar(0x2022));
        }

        QString emptyValue()
        {
            return QStringLiteral("—");
        }

        /**
         * The 11px caption lines. The design leaves them at regular weight and
         * keeps medium for the overlines and the strength label, while the
         * 11px type role is medium throughout.
         */
        QFont captionFont()
        {
            QFont font = theme()->font(TypeRole::LabelSmall);
            font.setWeight(QFont::Normal);
            return font;
        }

        /** The mockup renders a six digit code as two groups of three: 418 302. */
        QString groupedCode(const QString& code)
        {
            if (code.length() < 4 || code.length() % 2 != 0) {
                return code;
            }
            const int half = code.length() / 2;
            return code.left(half) + QLatin1Char(' ') + code.mid(half);
        }

        /** The mono role is specified at 14px; scale it to another design size. */
        QFont monoFont(int designPx)
        {
            QFont font = theme()->font(TypeRole::Mono);
            if (font.pointSizeF() > 0.0) {
                font.setPointSizeF(font.pointSizeF() * designPx / 14.0);
            } else {
                font.setPixelSize(qMax(1, qRound(font.pixelSize() * designPx / 14.0)));
            }
            return font;
        }

        void styleLabel(QLabel* label, const QFont& font, const QColor& color)
        {
            label->setFont(font);
            QPalette palette = label->palette();
            palette.setColor(QPalette::WindowText, color);
            palette.setColor(QPalette::Text, color);
            label->setPalette(palette);
        }

        /** Lay @p text out over at most @p maxLines lines, eliding the last one. */
        QString elideToLines(const QString& text, const QFont& font, int width, int maxLines)
        {
            if (text.isEmpty() || width <= 0) {
                return text;
            }

            const QFontMetrics metrics(font);
            QTextLayout layout(text, font);
            QStringList lines;

            layout.beginLayout();
            while (lines.size() < maxLines) {
                QTextLine line = layout.createLine();
                if (!line.isValid()) {
                    break;
                }
                line.setLineWidth(width);
                const int start = line.textStart();
                const bool last = lines.size() == maxLines - 1;
                if (last && start + line.textLength() < text.length()) {
                    lines << metrics.elidedText(text.mid(start), Qt::ElideRight, width);
                } else {
                    lines << text.mid(start, line.textLength());
                }
            }
            layout.endLayout();

            return lines.join(QLatin1Char('\n'));
        }

        /**
         * A rounded surface with an optional hairline border.
         *
         * Material::Card has no filled-and-outlined variant, which is what the
         * credentials card and the attachment rows are, so the pane paints its
         * own surfaces through the shared primitive.
         */
        class SurfacePanel : public QWidget
        {
        public:
            SurfacePanel(Role fill, int radius, bool bordered, QWidget* parent = nullptr)
                : QWidget(parent)
                , m_fill(fill)
                , m_radius(radius)
                , m_bordered(bordered)
            {
            }

        protected:
            void paintEvent(QPaintEvent*) override
            {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                const QColor border = m_bordered ? theme()->color(Role::OutlineVariant) : QColor();
                paintSurface(&painter, rect(), m_radius, theme()->color(m_fill), border);
            }

        private:
            Role m_fill;
            int m_radius;
            bool m_bordered;
        };

        /** The hairline between two credential rows, inset either side. */
        class Divider : public QWidget
        {
        public:
            explicit Divider(QWidget* parent = nullptr)
                : QWidget(parent)
            {
                setFixedHeight(1);
            }

        protected:
            void paintEvent(QPaintEvent*) override
            {
                QPainter painter(this);
                const int width = qMax(0, this->width() - 2 * DividerInset);
                painter.fillRect(QRect(DividerInset, 0, width, 1), theme()->color(Role::OutlineVariant));
            }
        };

        /** Shared hover tracking for the pane's hand-painted rows and buttons. */
        class HoverButton : public QAbstractButton
        {
        public:
            explicit HoverButton(QWidget* parent = nullptr)
                : QAbstractButton(parent)
            {
                setCursor(Qt::PointingHandCursor);
                setFocusPolicy(Qt::TabFocus);
            }

        protected:
            bool isHovered() const
            {
                return m_hovered && isEnabled();
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

        /**
         * The destructive action: a round outlined button drawn in error that
         * fills with errorContainer on hover. No button variant carries that
         * combination, so it is painted here.
         */
        class DeleteButton : public HoverButton
        {
        public:
            explicit DeleteButton(QWidget* parent = nullptr)
                : HoverButton(parent)
            {
                setFixedSize(Layout::IconButtonSize, Layout::IconButtonSize);
            }

        protected:
            void paintEvent(QPaintEvent*) override
            {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);

                const bool active = isHovered() || (isDown() && isEnabled());
                const QColor fill = active ? theme()->color(Role::ErrorContainer) : QColor();
                paintSurface(&painter, rect(), Shape::Full, fill, theme()->color(Role::Outline));

                QColor tint = theme()->color(Role::Error);
                if (!isEnabled()) {
                    tint.setAlphaF(0.38f);
                }
                QRect glyph(0, 0, 20, 20);
                glyph.moveCenter(rect().center());
                Icons::symbol(QStringLiteral("delete"), tint).paint(&painter, glyph);
            }
        };

        /** A 48px outlined attachment row: glyph, name, size and open_in_new. */
        class AttachmentRow : public HoverButton
        {
        public:
            AttachmentRow(const QString& name, const QString& size, QWidget* parent = nullptr)
                : HoverButton(parent)
                , m_name(name)
                , m_size(size)
            {
                setFixedHeight(AttachmentRowHeight);
                setToolTip(name);
            }

        protected:
            void paintEvent(QPaintEvent*) override
            {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);

                const QColor fill = isHovered() || isDown() ? theme()->color(Role::SurfaceContainerHigh) : QColor();
                paintSurface(&painter, rect(), Shape::Large, fill, theme()->color(Role::OutlineVariant));

                const QColor meta = theme()->color(Role::OnSurfaceVariant);
                QRect leading(0, 0, 20, 20);
                leading.moveTo(14, (height() - leading.height()) / 2);
                Icons::symbol(QStringLiteral("description"), meta).paint(&painter, leading);

                QRect trailing(0, 0, 18, 18);
                trailing.moveTo(width() - 14 - trailing.width(), (height() - trailing.height()) / 2);
                Icons::symbol(QStringLiteral("open_in_new"), meta).paint(&painter, trailing);

                const QFont sizeFont = captionFont();
                const int sizeWidth = m_size.isEmpty() ? 0 : QFontMetrics(sizeFont).horizontalAdvance(m_size);
                const int nameLeft = leading.right() + 1 + 12;
                const int nameRight = trailing.left() - 12 - (sizeWidth > 0 ? sizeWidth + 12 : 0);

                const QFont nameFont = theme()->font(TypeRole::BodySmall);
                const QRect nameRect(nameLeft, 0, qMax(0, nameRight - nameLeft), height());
                painter.setFont(nameFont);
                painter.setPen(theme()->color(Role::OnSurface));
                painter.drawText(nameRect,
                                 Qt::AlignVCenter | Qt::AlignLeft,
                                 QFontMetrics(nameFont).elidedText(m_name, Qt::ElideMiddle, nameRect.width()));

                if (sizeWidth > 0) {
                    painter.setFont(sizeFont);
                    painter.setPen(meta);
                    painter.drawText(QRect(nameRect.right() + 1 + 12, 0, sizeWidth, height()),
                                     Qt::AlignVCenter | Qt::AlignRight,
                                     m_size);
                }
            }

        private:
            QString m_name;
            QString m_size;
        };

        /**
         * The history row: a filled rounded-14 row with a leading history glyph
         * and a trailing chevron. The summary itself is a wrapping label the
         * pane owns, so the row grows with the text.
         */
        class HistoryRow : public HoverButton
        {
        public:
            HistoryRow(QLabel* summary, QWidget* parent = nullptr)
                : HoverButton(parent)
            {
                setMinimumHeight(HistoryRowHeight);
                auto* layout = new QHBoxLayout(this);
                // Padding, glyph and gap either side; the glyphs themselves are painted.
                layout->setContentsMargins(16 + 20 + 10, 8, 16 + 18 + 10, 8);
                layout->setSpacing(0);
                summary->setParent(this);
                layout->addWidget(summary);
            }

        protected:
            void paintEvent(QPaintEvent*) override
            {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);

                const Role fill = isHovered() || isDown() ? Role::SurfaceContainerHigh : Role::SurfaceContainer;
                paintSurface(&painter, rect(), Shape::Large, theme()->color(fill));

                const QColor meta = theme()->color(Role::OnSurfaceVariant);
                QRect leading(0, 0, 20, 20);
                leading.moveTo(16, (height() - leading.height()) / 2);
                Icons::symbol(QStringLiteral("history"), meta).paint(&painter, leading);

                QRect trailing(0, 0, 18, 18);
                trailing.moveTo(width() - 16 - trailing.width(), (height() - trailing.height()) / 2);
                Icons::symbol(QStringLiteral("chevron_right"), meta).paint(&painter, trailing);
            }
        };
    } // namespace

    // ------------------------------------------------------------- ValueLabel

    /**
     * A label that keeps its full text and elides it into the width the layout
     * actually gave it: one line for the credentials, two for the entry title.
     */
    class EntryDetail::ValueLabel : public QLabel
    {
    public:
        explicit ValueLabel(int maxLines = 1, QWidget* parent = nullptr)
            : QLabel(parent)
            , m_maxLines(qMax(1, maxLines))
        {
            setWordWrap(false);
            setTextInteractionFlags(Qt::NoTextInteraction);
            // The pane's width is fixed, so the label takes what it is given
            // rather than asking for the width of the untruncated value.
            setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        }

        void setFullText(const QString& text)
        {
            m_fullText = text;
            reflow();
        }

        QString fullText() const
        {
            return m_fullText;
        }

    protected:
        void resizeEvent(QResizeEvent* event) override
        {
            QLabel::resizeEvent(event);
            reflow();
        }

        void changeEvent(QEvent* event) override
        {
            QLabel::changeEvent(event);
            if (event->type() == QEvent::FontChange) {
                reflow();
            }
        }

    private:
        void reflow()
        {
            if (width() <= 0) {
                QLabel::setText(m_fullText);
                return;
            }
            if (m_maxLines == 1) {
                QLabel::setText(fontMetrics().elidedText(m_fullText, Qt::ElideRight, width()));
                return;
            }
            QLabel::setText(elideToLines(m_fullText, font(), width(), m_maxLines));
        }

        QString m_fullText;
        int m_maxLines;
    };

    // -------------------------------------------------------------- HeroPanel

    /**
     * The header of the design: a flat container in the entry's health colour
     * carrying the chip, the actions, the title and the URL.
     */
    class EntryDetail::HeroPanel : public QWidget
    {
    public:
        explicit HeroPanel(QWidget* parent = nullptr)
            : QWidget(parent)
        {
        }

        void setHealth(Health health)
        {
            m_health = health;
            update();
        }

        Health health() const
        {
            return m_health;
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.fillRect(rect(), theme()->colors().healthContainer(m_health));
        }

    private:
        Health m_health = Health::Unknown;
    };

    // ------------------------------------------------------------- HealthChip

    /** The 26px health chip at the top of the hero: a glyph and the health word. */
    class EntryDetail::HealthChip : public QWidget
    {
    public:
        explicit HealthChip(QWidget* parent = nullptr)
            : QWidget(parent)
        {
            setFixedHeight(HealthChipHeight);
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        }

        void setHealth(Health health)
        {
            m_health = health;
            m_text = health == Health::Unknown ? EntryDetail::tr("Unchecked") : Theme::healthLabel(health);
            setAccessibleName(EntryDetail::tr("Health: %1").arg(m_text));
            updateGeometry();
            update();
        }

        QSize sizeHint() const override
        {
            const QFontMetrics metrics(chipFont());
            return QSize(2 * HealthChipPaddingX + HealthChipGlyph + 6 + metrics.horizontalAdvance(m_text), HealthChipHeight);
        }

        QSize minimumSizeHint() const override
        {
            return sizeHint();
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const QColor content = theme()->colors().onHealthContainer(m_health);
            paintSurface(&painter, rect(), Material::Shape::Small, theme()->color(Role::SurfaceContainerLowest));
            const QString symbol = m_health == Health::Ok ? QStringLiteral("verified_user")
                                   : m_health == Health::Unknown ? QStringLiteral("help")
                                                                 : QStringLiteral("warning");
            QRect glyph(HealthChipPaddingX, (height() - HealthChipGlyph) / 2, HealthChipGlyph, HealthChipGlyph);
            Icons::symbol(symbol, content).paint(&painter, glyph);
            painter.setFont(chipFont());
            painter.setPen(content);
            painter.drawText(QRect(glyph.right() + 1 + 6, 0, width() - glyph.right() - 7 - HealthChipPaddingX, height()),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             m_text);
        }

    private:
        static QFont chipFont()
        {
            QFont font = theme()->font(TypeRole::LabelSmall);
            font.setWeight(QFont::Medium);
            return font;
        }

        Health m_health = Health::Unknown;
        QString m_text;
    };

    // --------------------------------------------------------------- FieldRow

    /**
     * One field container: surfaceContainer (surfaceContainerHighest for a
     * secret) with a 14px corner, the uppercase key over the value and the
     * trailing round actions.
     */
    class EntryDetail::FieldRow : public QWidget
    {
    public:
        FieldRow(const QString& key, bool protectedValue, QWidget* parent = nullptr)
            : QWidget(parent)
            , m_key(key)
            , m_protected(protectedValue)
        {
            setMinimumHeight(FieldRowMinHeight);
            m_layout = new QHBoxLayout(this);
            m_layout->setContentsMargins(14, 9, 8, 9);
            m_layout->setSpacing(12);
            m_text = new QWidget(this);
            m_column = new QVBoxLayout(m_text);
            m_column->setContentsMargins(0, 0, 0, 0);
            m_column->setSpacing(2);
            m_layout->addWidget(m_text, 1);
        }

        QString key() const
        {
            return m_key;
        }

        QVBoxLayout* column() const
        {
            return m_column;
        }

        void addTrailing(QWidget* widget)
        {
            m_layout->addWidget(widget, 0, Qt::AlignVCenter);
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            paintSurface(&painter,
                         rect(),
                         Material::Shape::Large,
                         theme()->color(m_protected ? Role::SurfaceContainerHighest : Role::SurfaceContainer));
        }

    private:
        QString m_key;
        bool m_protected;
        QHBoxLayout* m_layout = nullptr;
        QWidget* m_text = nullptr;
        QVBoxLayout* m_column = nullptr;
    };

    // ---------------------------------------------------------- StrengthMeter

    /** The 6px pill under the password: a track plus a health-coloured fill. */
    class EntryDetail::StrengthMeter : public QWidget
    {
    public:
        explicit StrengthMeter(QWidget* parent = nullptr)
            : QWidget(parent)
        {
            setFixedHeight(MeterHeight);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }

        void setValue(int percent, Health health)
        {
            m_percent = qBound(0, percent, 100);
            m_health = health;
            update();
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);

            const qreal radius = height() / 2.0;
            painter.setBrush(theme()->color(Role::SurfaceContainerHighest));
            painter.drawRoundedRect(rect(), radius, radius);

            const int filled = qRound(width() * m_percent / 100.0);
            if (filled > 0) {
                painter.setBrush(theme()->colors().healthColor(m_health));
                painter.drawRoundedRect(QRect(0, 0, qMax(filled, MeterHeight), height()), radius, radius);
            }
        }

    private:
        int m_percent = 0;
        Health m_health = Health::Unknown;
    };

    // ---------------------------------------------------------- CountdownRing

    /**
     * The TOTP countdown: a conical progress arc around a surface-coloured disc
     * carrying the seconds left in the current step.
     */
    class EntryDetail::CountdownRing : public QWidget
    {
    public:
        explicit CountdownRing(QWidget* parent = nullptr)
            : QWidget(parent)
        {
            setFixedSize(RingSize, RingSize);
        }

        void setRemaining(int seconds, int period)
        {
            m_seconds = qMax(0, seconds);
            m_period = qMax(1, period);
            update();
        }

        void setActive(bool active)
        {
            if (m_active == active) {
                return;
            }
            m_active = active;
            update();
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);

            const QRectF ring(0.0, 0.0, RingSize, RingSize);
            painter.setBrush(theme()->color(Role::SurfaceContainerHighest));
            painter.drawEllipse(ring);

            if (m_active) {
                // Clockwise from twelve o'clock, like the mockup's conic gradient.
                const qreal fraction = qBound(0.0, qreal(m_seconds) / m_period, 1.0);
                painter.setBrush(theme()->color(Role::Primary));
                painter.drawPie(ring, 90 * 16, -qRound(fraction * 360.0) * 16);
            }

            QRectF disc(0.0, 0.0, RingDiscSize, RingDiscSize);
            disc.moveCenter(ring.center());
            painter.setBrush(theme()->color(Role::SurfaceContainerLowest));
            painter.drawEllipse(disc);

            painter.setPen(theme()->color(Role::OnSurface));
            painter.setFont(theme()->font(TypeRole::LabelSmall));
            painter.drawText(disc, Qt::AlignCenter, m_active ? QString::number(m_seconds) : QStringLiteral("–"));
        }

    private:
        int m_seconds = 0;
        int m_period = TotpStep;
        bool m_active = false;
    };

    // ------------------------------------------------------------ EntryDetail

    EntryDetail::EntryDetail(QWidget* parent)
        : QScrollArea(parent)
        , m_totpTimer(new QTimer(this))
    {
        setObjectName(QStringLiteral("materialEntryDetail"));
        setFrameShape(QFrame::NoFrame);
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFixedWidth(Layout::DetailPaneWidth);
        // Keep the viewport clear of the left border the pane paints itself.
        setViewportMargins(1, 0, 0, 0);

        m_totpTimer->setInterval(1000);
        m_totpTimer->setTimerType(Qt::CoarseTimer);
        connect(m_totpTimer, &QTimer::timeout, this, &EntryDetail::updateTotp);

        buildUi();

        connect(theme(), &Theme::changed, this, &EntryDetail::applyTheme);
        applyTheme();
        clear();
    }

    EntryDetail::~EntryDetail() = default;

    void EntryDetail::buildUi()
    {
        m_content = new QWidget(this);
        m_content->setAutoFillBackground(false);

        auto* column = new QVBoxLayout(m_content);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(0);
        column->addWidget(buildHeader());
        column->addWidget(buildActions());
        column->addWidget(inset(buildCredentials(), {PaneMargin, 0, PaneMargin, 12}));
        column->addWidget(buildNotes());
        column->addWidget(buildAttachments());
        column->addWidget(buildHistory());
        column->addWidget(buildFooter());
        column->addStretch(1);

        setWidget(m_content);
    }

    QWidget* EntryDetail::buildHeader()
    {
        // The design's hero: health chip and round actions on the first line,
        // then the title and the URL, all on the entry's health colour.
        m_hero = new HeroPanel(m_content);
        m_hero->setObjectName(QStringLiteral("entryDetailHero"));
        auto* layout = new QVBoxLayout(m_hero);
        layout->setContentsMargins(22, 22, 22, 18);
        layout->setSpacing(4);

        auto* top = new QHBoxLayout;
        top->setContentsMargins(0, 0, 0, 8);
        top->setSpacing(8);
        m_healthChip = new HealthChip(m_hero);
        m_healthChip->setObjectName(QStringLiteral("entryDetailHealthChip"));
        top->addWidget(m_healthChip, 0, Qt::AlignVCenter);
        top->addStretch(1);

        m_favouriteButton = new IconButton(QStringLiteral("star"), m_hero);
        m_favouriteButton->setCheckable(true);
        m_favouriteButton->setSymbolSize(RowGlyphSize);
        m_favouriteButton->setDiameter(HeroButtonSize);
        m_favouriteButton->setToolTip(tr("Toggle favourite"));
        connect(m_favouriteButton, &QAbstractButton::toggled, this, [this](bool favourite) {
            m_data.favourite = favourite;
            updateFavouriteState();
            emit favouriteToggled(favourite);
        });
        top->addWidget(m_favouriteButton, 0, Qt::AlignVCenter);

        m_editButton = new IconButton(QStringLiteral("edit"), m_hero);
        m_editButton->setObjectName(QStringLiteral("entryDetailEdit"));
        m_editButton->setSymbolSize(RowGlyphSize);
        m_editButton->setDiameter(HeroButtonSize);
        m_editButton->setFilled(true);
        m_editButton->setToolTip(tr("Edit"));
        m_editButton->setAccessibleName(tr("Edit entry"));
        connect(m_editButton, &QAbstractButton::clicked, this, &EntryDetail::editRequested);
        top->addWidget(m_editButton, 0, Qt::AlignVCenter);

        m_historyButton = new IconButton(QStringLiteral("history"), m_hero);
        m_historyButton->setObjectName(QStringLiteral("entryDetailHistory"));
        m_historyButton->setSymbolSize(RowGlyphSize);
        m_historyButton->setDiameter(HeroButtonSize);
        m_historyButton->setFilled(true);
        m_historyButton->setToolTip(tr("Revision history"));
        m_historyButton->setAccessibleName(tr("Revision history"));
        connect(m_historyButton, &QAbstractButton::clicked, this, &EntryDetail::historyRequested);
        top->addWidget(m_historyButton, 0, Qt::AlignVCenter);
        layout->addLayout(top);

        // The symbol tile of the earlier pane is kept out of the picture; the
        // label still exists so the entry glyph reaches assistive technology.
        m_symbolLabel = new QLabel(m_hero);
        m_symbolLabel->hide();

        m_titleLabel = new ValueLabel(2, m_hero);
        m_titleLabel->setObjectName(QStringLiteral("entryDetailTitle"));
        m_urlLabel = new ValueLabel(2, m_hero);
        layout->addWidget(m_titleLabel);
        layout->addWidget(m_urlLabel);

        return m_hero;
    }

    QWidget* EntryDetail::buildActions()
    {
        auto* actions = new QWidget(m_content);
        auto* layout = new QHBoxLayout(actions);
        layout->setContentsMargins(TextMargin, 16, TextMargin, 12);
        layout->setSpacing(8);

        m_autoTypeButton = new FilledButton(QStringLiteral("keyboard"), tr("Auto-Type"), actions);
        m_autoTypeButton->setSymbolSize(20);
        m_autoTypeButton->setFixedHeight(Layout::ButtonHeight);
        m_autoTypeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(m_autoTypeButton, &QAbstractButton::clicked, this, &EntryDetail::autoTypeRequested);
        layout->addWidget(m_autoTypeButton, 1);

        auto* deleteButton = new DeleteButton(actions);
        deleteButton->setToolTip(tr("Delete entry"));
        connect(deleteButton, &QAbstractButton::clicked, this, &EntryDetail::deleteRequested);
        m_deleteButton = deleteButton;
        layout->addWidget(deleteButton, 0);

        return actions;
    }

    EntryDetail::FieldRow* EntryDetail::addFieldRow(QVBoxLayout* column, const QString& key, bool protectedValue, ValueLabel** value)
    {
        auto* row = new FieldRow(key, protectedValue, m_content);
        row->setObjectName(QStringLiteral("entryDetailField_") + key);
        row->column()->addWidget(createOverline(key));
        *value = new ValueLabel(1, row);
        row->column()->addWidget(*value);
        column->addWidget(row);
        return row;
    }

    QWidget* EntryDetail::buildCredentials()
    {
        // The design's field containers: one rounded card per field, the key in
        // small capitals over the value, and the actions at the trailing edge.
        auto* card = new QWidget(m_content);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        cardLayout->setSpacing(8);

        // Username
        m_usernameRow = addFieldRow(cardLayout, tr("Username"), false, &m_usernameLabel);
        m_copyUsernameButton = createRowButton(QStringLiteral("content_copy"), tr("Copy username"));
        connect(m_copyUsernameButton, &QAbstractButton::clicked, this, [this] {
            emit copyRequested(QStringLiteral("username"));
        });
        m_usernameRow->addTrailing(m_copyUsernameButton);

        // Password
        m_passwordRow = addFieldRow(cardLayout, tr("Password"), true, &m_passwordLabel);
        auto* meterRow = new QWidget(m_passwordRow);
        auto* meterLayout = new QHBoxLayout(meterRow);
        meterLayout->setContentsMargins(0, 8, 0, 0);
        meterLayout->setSpacing(8);
        m_strengthMeter = new StrengthMeter(meterRow);
        meterLayout->addWidget(m_strengthMeter, 1);
        m_strengthLabel = new QLabel(meterRow);
        meterLayout->addWidget(m_strengthLabel, 0);
        m_passwordRow->column()->addWidget(meterRow);

        m_revealButton = createRowButton(QStringLiteral("visibility"), tr("Show password"));
        m_revealButton->setCheckable(true);
        connect(m_revealButton, &QAbstractButton::toggled, this, &EntryDetail::setPasswordVisible);
        m_passwordRow->addTrailing(m_revealButton);

        m_copyPasswordButton = createRowButton(QStringLiteral("content_copy"), tr("Copy password"));
        connect(m_copyPasswordButton, &QAbstractButton::clicked, this, [this] {
            emit copyRequested(QStringLiteral("password"));
        });
        m_passwordRow->addTrailing(m_copyPasswordButton);

        // URL
        m_urlRow = addFieldRow(cardLayout, tr("URL"), false, &m_urlFieldLabel);
        m_copyUrlButton = createRowButton(QStringLiteral("content_copy"), tr("Copy URL"));
        connect(m_copyUrlButton, &QAbstractButton::clicked, this, [this] {
            emit copyRequested(QStringLiteral("url"));
        });
        m_urlRow->addTrailing(m_copyUrlButton);

        // Modified
        m_modifiedRow = addFieldRow(cardLayout, tr("Modified"), false, &m_modifiedLabel);

        // One-time password, in the protected tone with its countdown ring.
        m_totpDivider = new QWidget(card);
        m_totpDivider->setFixedHeight(0);
        cardLayout->addWidget(m_totpDivider);
        auto* totpRow = new FieldRow(tr("One-time password"), true, m_content);
        totpRow->setMinimumHeight(TotpRowHeight);
        m_totpRow = totpRow;
        m_totpRing = new CountdownRing(totpRow);
        totpRow->column()->addWidget(createOverline(tr("One-time password")));
        m_totpLabel = new ValueLabel(1, totpRow);
        totpRow->column()->addWidget(m_totpLabel);
        totpRow->addTrailing(m_totpRing);
        m_copyTotpButton = createRowButton(QStringLiteral("content_copy"), tr("Copy one-time password"));
        connect(m_copyTotpButton, &QAbstractButton::clicked, this, [this] {
            emit copyRequested(QStringLiteral("totp"));
        });
        totpRow->addTrailing(m_copyTotpButton);
        cardLayout->addWidget(totpRow);

        return card;
    }

    QWidget* EntryDetail::buildNotes()
    {
        m_notesSection = new QWidget(m_content);
        auto* layout = new QVBoxLayout(m_notesSection);
        layout->setContentsMargins(0, 0, 0, 14);
        layout->setSpacing(0);

        QLabel* overline = createOverline(tr("Notes"));
        overline->setContentsMargins(TextMargin, 8, TextMargin, 4);
        layout->addWidget(overline);

        auto* card = new SurfacePanel(Role::SurfaceContainer, Material::Shape::Row, false);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 14, 16, 14);
        cardLayout->setSpacing(0);
        m_notesLabel = new QLabel(card);
        m_notesLabel->setWordWrap(true);
        cardLayout->addWidget(m_notesLabel);
        layout->addWidget(inset(card, {PaneMargin, 0, PaneMargin, 0}));

        return m_notesSection;
    }

    QWidget* EntryDetail::buildAttachments()
    {
        m_attachmentsSection = new QWidget(m_content);
        auto* layout = new QVBoxLayout(m_attachmentsSection);
        layout->setContentsMargins(0, 0, 0, 16);
        layout->setSpacing(0);

        layout->addWidget(createOverline(tr("Attachments")));

        // The design's "Filter attachments & fields" search with its own
        // regex affordance; it narrows the field rows and the attachment list.
        m_attachmentFilter = new SearchBar(SearchBar::Variant::Surface, m_attachmentsSection);
        m_attachmentFilter->setObjectName(QStringLiteral("entryDetailAttachmentFilter"));
        m_attachmentFilter->setPlaceholder(tr("Filter attachments & fields"));
        m_attachmentFilter->setIdentity(QStringLiteral("vault.attachments"), tr("Entry attachments and fields filter"));
        m_attachmentFilter->lineEdit()->setAccessibleName(tr("Filter attachments and fields"));
        connect(m_attachmentFilter, &SearchBar::textChanged, this, [this] { applyDetailFilter(); });
        connect(m_attachmentFilter, &SearchBar::regexToggled, this, [this] { applyDetailFilter(); });
        layout->addWidget(inset(m_attachmentFilter, {PaneMargin, 0, PaneMargin, 8}));

        m_attachmentsList = new QWidget(m_attachmentsSection);
        m_attachmentsLayout = new QVBoxLayout(m_attachmentsList);
        m_attachmentsLayout->setContentsMargins(0, 0, 0, 0);
        m_attachmentsLayout->setSpacing(8);
        layout->addWidget(inset(m_attachmentsList, {PaneMargin, 0, PaneMargin, 0}));

        return m_attachmentsSection;
    }

    QWidget* EntryDetail::buildHistory()
    {
        m_historyLabel = new QLabel(m_content);
        m_historyLabel->setWordWrap(true);

        auto* row = new HistoryRow(m_historyLabel);
        connect(row, &QAbstractButton::clicked, this, &EntryDetail::historyRequested);

        m_historySection = inset(row, {PaneMargin, 0, PaneMargin, 22});
        return m_historySection;
    }

    QWidget* EntryDetail::buildFooter()
    {
        // The design's closing actions: a full-width Copy password and a round
        // Open URL beside it.
        auto* footer = new QWidget(m_content);
        auto* layout = new QHBoxLayout(footer);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        m_copyPasswordAction = new FilledButton(QStringLiteral("content_copy"), tr("Copy password"), footer);
        m_copyPasswordAction->setObjectName(QStringLiteral("entryDetailCopyPassword"));
        m_copyPasswordAction->setSymbolSize(RowGlyphSize);
        m_copyPasswordAction->setFixedHeight(FooterButtonHeight);
        m_copyPasswordAction->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(m_copyPasswordAction, &QAbstractButton::clicked, this, [this] {
            emit copyRequested(QStringLiteral("password"));
        });
        layout->addWidget(m_copyPasswordAction, 1);

        m_openUrlButton = new IconButton(QStringLiteral("open_in_new"), footer);
        m_openUrlButton->setObjectName(QStringLiteral("entryDetailOpenUrl"));
        m_openUrlButton->setSymbolSize(RowGlyphSize);
        m_openUrlButton->setDiameter(FooterButtonHeight);
        m_openUrlButton->setToolTip(tr("Open URL"));
        m_openUrlButton->setAccessibleName(tr("Open URL"));
        connect(m_openUrlButton, &QAbstractButton::clicked, this, &EntryDetail::openUrlRequested);
        layout->addWidget(m_openUrlButton, 0);

        return inset(footer, {PaneMargin, 0, PaneMargin, 22});
    }

    SearchBar* EntryDetail::attachmentFilter() const
    {
        return m_attachmentFilter;
    }

    QStringList EntryDetail::visibleFieldKeys() const
    {
        QStringList keys;
        for (FieldRow* row : {m_usernameRow, m_passwordRow, m_urlRow, m_modifiedRow}) {
            if (row && !row->isHidden()) {
                keys << row->key();
            }
        }
        return keys;
    }

    void EntryDetail::applyDetailFilter()
    {
        const QString needle = m_attachmentFilter ? m_attachmentFilter->text().trimmed() : QString();
        QRegularExpression pattern;
        bool useRegex = false;
        if (m_attachmentFilter && m_attachmentFilter->isRegexEnabled() && !needle.isEmpty()) {
            pattern = QRegularExpression(needle, QRegularExpression::CaseInsensitiveOption);
            // An unparsable pattern changes nothing rather than hiding
            // everything or quietly turning into a literal search.
            if (!pattern.isValid()) {
                return;
            }
            useRegex = true;
        }
        auto accepts = [&](const QString& haystack) {
            if (needle.isEmpty()) {
                return true;
            }
            if (useRegex) {
                return pattern.match(haystack).hasMatch();
            }
            return haystack.contains(needle, Qt::CaseInsensitive);
        };

        const struct
        {
            FieldRow* row;
            QString value;
        } fields[] = {{m_usernameRow, m_data.username},
                      {m_passwordRow, QString()},
                      {m_urlRow, m_data.url},
                      {m_modifiedRow, m_data.modified}};
        for (const auto& field : fields) {
            if (field.row) {
                field.row->setVisible(accepts(field.row->key() + QLatin1Char(' ') + field.value));
            }
        }
        const auto rows = m_attachmentsList->findChildren<QAbstractButton*>(QString(), Qt::FindDirectChildrenOnly);
        for (QAbstractButton* row : rows) {
            row->setVisible(accepts(row->toolTip()));
        }
    }

    QLabel* EntryDetail::createCaption(const QString& text)
    {
        auto* label = new QLabel(text, m_content);
        m_captions.append(label);
        return label;
    }

    QLabel* EntryDetail::createOverline(const QString& text)
    {
        auto* label = new QLabel(text, m_content);
        label->setContentsMargins(TextMargin, 0, TextMargin, 4);
        m_overlines.append(label);
        return label;
    }

    IconButton* EntryDetail::createRowButton(const QString& symbol, const QString& tooltip)
    {
        auto* button = new IconButton(symbol, m_content);
        button->setDiameter(RowButtonSize);
        button->setSymbolSize(RowGlyphSize);
        button->setToolTip(tooltip);
        return button;
    }

    QWidget* EntryDetail::inset(QWidget* child, const QMargins& margins)
    {
        auto* container = new QWidget(m_content);
        auto* layout = new QVBoxLayout(container);
        layout->setContentsMargins(margins);
        layout->setSpacing(0);
        layout->addWidget(child);
        return container;
    }

    // ------------------------------------------------------------------- data

    const EntryDetailData& EntryDetail::entryData() const
    {
        return m_data;
    }

    void EntryDetail::setEntryData(const EntryDetailData& entry)
    {
        m_data = entry;
        m_hasEntry = true;
        setPasswordVisible(false);
        updateContent();
    }

    void EntryDetail::clear()
    {
        m_data = EntryDetailData();
        m_hasEntry = false;
        setPasswordVisible(false);
        updateContent();
    }

    bool EntryDetail::isPasswordVisible() const
    {
        return m_passwordVisible;
    }

    void EntryDetail::setPasswordVisible(bool visible)
    {
        if (m_passwordVisible == visible) {
            return;
        }
        m_passwordVisible = visible;
        updatePasswordDisplay();
    }

    void EntryDetail::updateContent()
    {
        const QString symbol = m_data.symbol.isEmpty() ? QStringLiteral("key") : m_data.symbol;
        m_symbolLabel->setPixmap(Icons::pixmap(symbol, SymbolGlyphSize, theme()->color(Role::OnPrimaryContainer)));

        m_titleLabel->setFullText(m_data.title);
        m_urlLabel->setFullText(m_data.url.isEmpty() ? tr("no URL") : m_data.url);
        m_hero->setHealth(m_data.health);
        m_healthChip->setHealth(m_data.health);
        m_healthChip->setVisible(m_hasEntry);
        m_historyButton->setEnabled(m_hasEntry);
        m_urlFieldLabel->setFullText(m_data.url.isEmpty() ? emptyValue() : m_data.url);
        m_copyUrlButton->setEnabled(!m_data.url.isEmpty());
        m_openUrlButton->setEnabled(!m_data.url.isEmpty());
        m_copyPasswordAction->setEnabled(!m_data.password.isEmpty());
        m_modifiedLabel->setFullText(m_data.modified.isEmpty() ? emptyValue() : m_data.modified);
        m_modifiedRow->setVisible(m_hasEntry);
        applyDetailFilter();

        {
            const QSignalBlocker blocker(m_favouriteButton);
            m_favouriteButton->setChecked(m_data.favourite);
        }
        updateFavouriteState();

        m_favouriteButton->setEnabled(m_hasEntry);
        m_autoTypeButton->setEnabled(m_hasEntry);
        m_editButton->setEnabled(m_hasEntry);
        m_deleteButton->setEnabled(m_hasEntry);
        m_copyUsernameButton->setEnabled(!m_data.username.isEmpty());

        m_usernameLabel->setFullText(m_data.username.isEmpty() ? emptyValue() : m_data.username);
        updatePasswordDisplay();

        m_strengthMeter->setValue(m_data.strengthPercent, m_data.health);
        m_strengthLabel->setText(m_data.strengthLabel);
        m_strengthLabel->setVisible(!m_data.strengthLabel.isEmpty());
        styleLabel(m_strengthLabel, theme()->font(TypeRole::LabelSmall), theme()->colors().healthColor(m_data.health));

        // The credentials card always has three rows. An entry without a
        // one-time password keeps the row and shows the placeholder state
        // rather than collapsing the card; only an empty pane drops it.
        m_totpDivider->setVisible(m_hasEntry);
        m_totpRow->setVisible(m_hasEntry);
        updateTotp();

        m_notesLabel->setText(m_data.notes);
        m_notesSection->setVisible(!m_data.notes.isEmpty());

        updateAttachments();

        m_historyLabel->setText(m_data.historySummary);
        m_historySection->setVisible(!m_data.historySummary.isEmpty());
    }

    void EntryDetail::updateAttachments()
    {
        qDeleteAll(m_attachmentsList->findChildren<QWidget*>(Qt::FindDirectChildrenOnly));

        for (const auto& attachment : m_data.attachments) {
            auto* row = new AttachmentRow(attachment.name, attachment.size, m_attachmentsList);
            const QString name = attachment.name;
            connect(row, &QAbstractButton::clicked, this, [this, name] { emit attachmentActivated(name); });
            m_attachmentsLayout->addWidget(row);
        }

        m_attachmentsSection->setVisible(!m_data.attachments.isEmpty());
    }

    void EntryDetail::updatePasswordDisplay()
    {
        const bool hasPassword = !m_data.password.isEmpty();

        {
            const QSignalBlocker blocker(m_revealButton);
            m_revealButton->setChecked(m_passwordVisible);
        }
        m_revealButton->setSymbol(m_passwordVisible ? QStringLiteral("visibility_off") : QStringLiteral("visibility"));
        m_revealButton->setToolTip(m_passwordVisible ? tr("Hide password") : tr("Show password"));
        m_revealButton->setEnabled(hasPassword);
        m_copyPasswordButton->setEnabled(hasPassword);

        QString value = emptyValue();
        if (hasPassword) {
            value = m_passwordVisible ? m_data.password : maskedPassword();
        }
        m_passwordLabel->setFullText(value);
    }

    void EntryDetail::updateFavouriteState()
    {
        // The filled star picks up the accent; the empty one stays neutral.
        if (m_data.favourite) {
            m_favouriteButton->setRoles(Role::PrimaryContainer, Role::Primary);
        } else {
            m_favouriteButton->clearRoles();
        }
        m_favouriteButton->update();
    }

    void EntryDetail::updateTotp()
    {
        const bool active = !m_data.totpCode.isEmpty();
        const int period = m_data.totpPeriod > 0 ? m_data.totpPeriod : TotpStep;

        int remaining = period;
        if (active) {
            // Steps are aligned to the epoch, so the ring matches every other
            // authenticator without the pane knowing the shared secret.
            remaining = period - static_cast<int>(QDateTime::currentSecsSinceEpoch() % period);
        }

        m_totpRing->setActive(active);
        m_totpRing->setRemaining(remaining, period);
        m_totpLabel->setFullText(active ? groupedCode(m_data.totpCode) : QStringLiteral("— — —"));
        m_copyTotpButton->setEnabled(active);

        syncTotpTimer();
    }

    void EntryDetail::syncTotpTimer()
    {
        const bool run = isVisible() && !m_data.totpCode.isEmpty();
        if (run && !m_totpTimer->isActive()) {
            m_totpTimer->start();
        } else if (!run && m_totpTimer->isActive()) {
            m_totpTimer->stop();
        }
    }

    // ------------------------------------------------------------------ theme

    void EntryDetail::applyTheme()
    {
        QPalette viewportPalette = viewport()->palette();
        viewportPalette.setColor(QPalette::Window, theme()->color(Role::SurfaceContainerLow));
        viewportPalette.setColor(QPalette::Base, theme()->color(Role::SurfaceContainerLow));
        viewport()->setPalette(viewportPalette);
        viewport()->setBackgroundRole(QPalette::Window);
        viewport()->setAutoFillBackground(true);

        const QColor onSurface = theme()->color(Role::OnSurface);
        const QColor onSurfaceVariant = theme()->color(Role::OnSurfaceVariant);

        const QColor onHero = theme()->colors().onHealthContainer(m_data.health);
        QFont heroTitle = theme()->font(TypeRole::TitleLarge);
        styleLabel(m_titleLabel, heroTitle, onHero);
        QColor heroUrl = onHero;
        heroUrl.setAlphaF(0.78);
        styleLabel(m_urlLabel, monoFont(12), heroUrl);
        styleLabel(m_urlFieldLabel, monoFont(14), onSurface);
        styleLabel(m_modifiedLabel, monoFont(14), onSurface);

        for (auto* caption : std::as_const(m_captions)) {
            styleLabel(caption, captionFont(), onSurfaceVariant);
        }

        QFont overline = theme()->font(TypeRole::LabelSmall);
        overline.setCapitalization(QFont::AllUppercase);
        overline.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        for (auto* label : std::as_const(m_overlines)) {
            styleLabel(label, overline, onSurfaceVariant);
        }

        styleLabel(m_usernameLabel, monoFont(14), onSurface);

        QFont password = monoFont(14);
        password.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
        styleLabel(m_passwordLabel, password, onSurface);

        QFont totp = monoFont(20);
        totp.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
        totp.setWeight(QFont::Medium);
        styleLabel(m_totpLabel, totp, onSurface);

        styleLabel(m_notesLabel, theme()->font(TypeRole::BodySmall), onSurfaceVariant);
        styleLabel(m_historyLabel, theme()->font(TypeRole::BodySmall), onSurface);

        updateContent();
        update();
    }

    // ------------------------------------------------------------------ paint

    void EntryDetail::paintEvent(QPaintEvent* event)
    {
        QPainter painter(this);
        painter.fillRect(rect(), theme()->color(Role::SurfaceContainerLow));
        painter.fillRect(QRect(0, 0, 1, height()), theme()->color(Role::OutlineVariant));
        painter.end();

        QScrollArea::paintEvent(event);
    }

    void EntryDetail::showEvent(QShowEvent* event)
    {
        QScrollArea::showEvent(event);
        updateTotp();
    }

    void EntryDetail::hideEvent(QHideEvent* event)
    {
        QScrollArea::hideEvent(event);
        m_totpTimer->stop();
    }

} // namespace Material
