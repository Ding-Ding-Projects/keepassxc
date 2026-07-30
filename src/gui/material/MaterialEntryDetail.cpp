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

#include <QAbstractButton>
#include <QDateTime>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
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

        QString maskedPassword()
        {
            return QString(MaskedLength, QChar(0x2022));
        }

        QString emptyValue()
        {
            return QStringLiteral("—");
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

                const QFont sizeFont = theme()->font(TypeRole::LabelSmall);
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
        column->addStretch(1);

        setWidget(m_content);
    }

    QWidget* EntryDetail::buildHeader()
    {
        auto* header = new QWidget(m_content);
        auto* layout = new QHBoxLayout(header);
        layout->setContentsMargins(TextMargin, 22, TextMargin, 16);
        layout->setSpacing(14);

        // Material::Shape has to be qualified here: QFrame::Shape shadows it.
        auto* tile = new SurfacePanel(Role::PrimaryContainer, Material::Shape::Rail, false, header);
        tile->setFixedSize(SymbolTileSize, SymbolTileSize);
        auto* tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(0, 0, 0, 0);
        m_symbolLabel = new QLabel(tile);
        m_symbolLabel->setAlignment(Qt::AlignCenter);
        tileLayout->addWidget(m_symbolLabel);
        layout->addWidget(tile, 0, Qt::AlignTop);

        auto* text = new QWidget(header);
        auto* textLayout = new QVBoxLayout(text);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);
        m_titleLabel = new ValueLabel(2, text);
        m_urlLabel = new ValueLabel(1, text);
        textLayout->addWidget(m_titleLabel);
        textLayout->addWidget(m_urlLabel);
        textLayout->addStretch(1);
        layout->addWidget(text, 1);

        m_favouriteButton = new IconButton(QStringLiteral("star"), header);
        m_favouriteButton->setCheckable(true);
        m_favouriteButton->setSymbolSize(22);
        m_favouriteButton->setToolTip(tr("Toggle favourite"));
        connect(m_favouriteButton, &QAbstractButton::toggled, this, [this](bool favourite) {
            m_data.favourite = favourite;
            updateFavouriteState();
            emit favouriteToggled(favourite);
        });
        layout->addWidget(m_favouriteButton, 0, Qt::AlignTop);

        return header;
    }

    QWidget* EntryDetail::buildActions()
    {
        auto* actions = new QWidget(m_content);
        auto* layout = new QHBoxLayout(actions);
        layout->setContentsMargins(TextMargin, 0, TextMargin, 16);
        layout->setSpacing(8);

        m_autoTypeButton = new FilledButton(QStringLiteral("keyboard"), tr("Auto-Type"), actions);
        m_autoTypeButton->setSymbolSize(20);
        m_autoTypeButton->setFixedHeight(Layout::ButtonHeight);
        m_autoTypeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(m_autoTypeButton, &QAbstractButton::clicked, this, &EntryDetail::autoTypeRequested);
        layout->addWidget(m_autoTypeButton, 1);

        m_editButton = new TonalButton(QStringLiteral("edit"), tr("Edit"), actions);
        m_editButton->setSymbolSize(20);
        m_editButton->setFixedHeight(Layout::ButtonHeight);
        m_editButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(m_editButton, &QAbstractButton::clicked, this, &EntryDetail::editRequested);
        layout->addWidget(m_editButton, 1);

        auto* deleteButton = new DeleteButton(actions);
        deleteButton->setToolTip(tr("Delete entry"));
        connect(deleteButton, &QAbstractButton::clicked, this, &EntryDetail::deleteRequested);
        m_deleteButton = deleteButton;
        layout->addWidget(deleteButton, 0);

        return actions;
    }

    QWidget* EntryDetail::buildCredentials()
    {
        auto* card = new SurfacePanel(Role::SurfaceContainerLowest, Material::Shape::Rail, true, m_content);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(0, 4, 0, 4);
        cardLayout->setSpacing(0);

        // Username
        auto* usernameRow = new QWidget(card);
        usernameRow->setFixedHeight(UsernameRowHeight);
        auto* usernameLayout = new QHBoxLayout(usernameRow);
        usernameLayout->setContentsMargins(16, 0, 16, 0);
        usernameLayout->setSpacing(12);

        auto* usernameText = new QWidget(usernameRow);
        auto* usernameColumn = new QVBoxLayout(usernameText);
        usernameColumn->setContentsMargins(0, 0, 0, 0);
        usernameColumn->setSpacing(0);
        usernameColumn->addWidget(createCaption(tr("Username")));
        m_usernameLabel = new ValueLabel(1, usernameText);
        usernameColumn->addWidget(m_usernameLabel);
        usernameLayout->addWidget(usernameText, 1);

        m_copyUsernameButton = createRowButton(QStringLiteral("content_copy"), tr("Copy username"));
        connect(m_copyUsernameButton, &QAbstractButton::clicked, this, [this] {
            emit copyRequested(QStringLiteral("username"));
        });
        usernameLayout->addWidget(m_copyUsernameButton, 0, Qt::AlignVCenter);
        cardLayout->addWidget(usernameRow);

        cardLayout->addWidget(new Divider(card));

        // Password
        auto* passwordRow = new QWidget(card);
        auto* passwordLayout = new QHBoxLayout(passwordRow);
        passwordLayout->setContentsMargins(16, 12, 16, 12);
        passwordLayout->setSpacing(12);

        auto* passwordText = new QWidget(passwordRow);
        auto* passwordColumn = new QVBoxLayout(passwordText);
        passwordColumn->setContentsMargins(0, 0, 0, 0);
        passwordColumn->setSpacing(0);
        passwordColumn->addWidget(createCaption(tr("Password")));
        m_passwordLabel = new ValueLabel(1, passwordText);
        passwordColumn->addWidget(m_passwordLabel);

        auto* meterRow = new QWidget(passwordText);
        auto* meterLayout = new QHBoxLayout(meterRow);
        meterLayout->setContentsMargins(0, 10, 0, 0);
        meterLayout->setSpacing(8);
        m_strengthMeter = new StrengthMeter(meterRow);
        meterLayout->addWidget(m_strengthMeter, 1);
        m_strengthLabel = new QLabel(meterRow);
        meterLayout->addWidget(m_strengthLabel, 0);
        passwordColumn->addWidget(meterRow);
        passwordLayout->addWidget(passwordText, 1);

        m_revealButton = createRowButton(QStringLiteral("visibility"), tr("Show password"));
        m_revealButton->setCheckable(true);
        connect(m_revealButton, &QAbstractButton::toggled, this, &EntryDetail::setPasswordVisible);
        passwordLayout->addWidget(m_revealButton, 0, Qt::AlignVCenter);

        m_copyPasswordButton = createRowButton(QStringLiteral("content_copy"), tr("Copy password"));
        connect(m_copyPasswordButton, &QAbstractButton::clicked, this, [this] {
            emit copyRequested(QStringLiteral("password"));
        });
        passwordLayout->addWidget(m_copyPasswordButton, 0, Qt::AlignVCenter);
        cardLayout->addWidget(passwordRow);

        m_totpDivider = new Divider(card);
        cardLayout->addWidget(m_totpDivider);

        // One-time password
        m_totpRow = new QWidget(card);
        m_totpRow->setFixedHeight(TotpRowHeight);
        auto* totpLayout = new QHBoxLayout(m_totpRow);
        totpLayout->setContentsMargins(16, 0, 16, 0);
        totpLayout->setSpacing(14);

        m_totpRing = new CountdownRing(m_totpRow);
        totpLayout->addWidget(m_totpRing, 0, Qt::AlignVCenter);

        auto* totpText = new QWidget(m_totpRow);
        auto* totpColumn = new QVBoxLayout(totpText);
        totpColumn->setContentsMargins(0, 0, 0, 0);
        totpColumn->setSpacing(0);
        totpColumn->addWidget(createCaption(tr("One-time password")));
        m_totpLabel = new ValueLabel(1, totpText);
        totpColumn->addWidget(m_totpLabel);
        totpLayout->addWidget(totpText, 1);

        auto* copyTotp = createRowButton(QStringLiteral("content_copy"), tr("Copy one-time password"));
        connect(copyTotp, &QAbstractButton::clicked, this, [this] { emit copyRequested(QStringLiteral("totp")); });
        totpLayout->addWidget(copyTotp, 0, Qt::AlignVCenter);
        cardLayout->addWidget(m_totpRow);

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
        m_urlLabel->setFullText(m_data.url);
        m_urlLabel->setVisible(!m_data.url.isEmpty());

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

        const bool hasTotp = !m_data.totpCode.isEmpty();
        m_totpDivider->setVisible(hasTotp);
        m_totpRow->setVisible(hasTotp);
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
        m_totpLabel->setFullText(active ? m_data.totpCode : QStringLiteral("— — —"));

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

        styleLabel(m_titleLabel, theme()->font(TypeRole::TitleLarge), onSurface);
        styleLabel(m_urlLabel, theme()->font(TypeRole::BodySmall), onSurfaceVariant);

        for (auto* caption : std::as_const(m_captions)) {
            styleLabel(caption, theme()->font(TypeRole::LabelSmall), onSurfaceVariant);
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
