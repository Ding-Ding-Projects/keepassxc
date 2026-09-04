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

#include "MaterialGeneratorSheet.h"

#include "MaterialSlider.h"

#include "MaterialButtons.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QAbstractButton>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QPainter>
#include <QRandomGenerator>
#include <QSlider>
#include <QVBoxLayout>
#include <QVariant>

#include <cmath>
#include <utility>

namespace Material
{
    namespace
    {
        constexpr int SheetWidth = 560;
        constexpr int SheetPadding = 26;
        constexpr int MinLength = 8;
        constexpr int MaxLength = 64;
        constexpr int DefaultLength = 24;
        constexpr int MeterHeight = 8;
        constexpr int ValuePadding = 18;
        constexpr int ValueRadius = 18;
        constexpr int ChipGap = 8;
        // The charset pill: 36px tall with 14px either side of a 13px mono label.
        constexpr int CharsetHeight = 36;
        constexpr int CharsetPadding = 14;
        // The two footer actions fill the sheet width as one 44px row.
        constexpr int ActionHeight = 44;
        constexpr int ActionGap = 10;
        constexpr int ActionSymbolSize = 20;
        // The 16px slider track, a full-radius pill the handle rides inside.
        constexpr int SliderTrackHeight = 16;
        // Width of the value column before the sheet has been laid out.
        constexpr int DefaultValueWidth = SheetWidth - 2 * SheetPadding - 2 * ValuePadding;
        // The meter is full at the entropy of a 24 character mixed password.
        constexpr double FullEntropy = 160.0;

        // Labels remember their type and colour role so a theme change can
        // restyle the whole sheet without a rebuild.
        constexpr const char* TypeProperty = "materialType";
        constexpr const char* ColorProperty = "materialRole";

        void styleLabel(QLabel* label, TypeRole type, Role color)
        {
            label->setProperty(TypeProperty, static_cast<int>(type));
            label->setProperty(ColorProperty, static_cast<int>(color));
            label->setFont(theme()->font(type));
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
                           static_cast<Role>(label->property(ColorProperty).toInt()));
            }
        }

        QLabel* makeLabel(const QString& text, TypeRole type, Role color)
        {
            auto* label = new QLabel(text);
            styleLabel(label, type, color);
            return label;
        }

        /** The 20px monospace face the generated value is shown in. */
        QFont valueFont()
        {
            QFont font = theme()->font(TypeRole::Mono);
            font.setPointSize(qMax(1, qRound(font.pointSize() * 20.0 / 14.0)));
            return font;
        }

        /** The 13px monospace face the charset pills are labelled in. */
        QFont pillFont()
        {
            QFont font = theme()->font(TypeRole::Mono);
            font.setPointSize(qMax(1, qRound(font.pointSize() * 13.0 / 14.0)));
            return font;
        }

        /**
         * Break @p value into lines that fit @p width. A password is one long
         * word, which QLabel would rather overflow than wrap, so the breaks are
         * measured here - the value itself is never handed out wrapped.
         */
        QString wrapValue(const QString& value, const QFont& font, int width)
        {
            if (value.isEmpty()) {
                return value;
            }
            const QFontMetrics metrics(font);
            const int charWidth = qMax(1, metrics.horizontalAdvance(QLatin1Char('W')));
            const int perLine = qMax(8, width / charWidth);

            QString wrapped;
            for (int index = 0; index < value.size(); index += perLine) {
                if (index > 0) {
                    wrapped.append(QLatin1Char('\n'));
                }
                wrapped.append(value.mid(index, perLine));
            }
            return wrapped;
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

        /** A rounded panel filled with a colour role. */
        class GeneratorPanel : public QWidget
        {
        public:
            GeneratorPanel(int radius, Role fill, QWidget* parent = nullptr)
                : QWidget(parent)
                , m_radius(radius)
                , m_fill(fill)
            {
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                paintSurface(&painter, rect(), m_radius, theme()->color(m_fill));
            }

        private:
            int m_radius;
            Role m_fill;
        };

        /**
         * The five pools the pills switch on. The look-alikes are left out on
         * purpose - no I or O, no l or o, no 0 or 1 - so a generated password
         * survives being read off a screen and typed on another machine.
         */
        QString poolFor(GeneratorSheet::CharClass charClass)
        {
            switch (charClass) {
            case GeneratorSheet::CharClass::Upper:
                return QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ");
            case GeneratorSheet::CharClass::Lower:
                return QStringLiteral("abcdefghijkmnpqrstuvwxyz");
            case GeneratorSheet::CharClass::Digits:
                return QStringLiteral("23456789");
            case GeneratorSheet::CharClass::Special:
                return QStringLiteral("!@#$%&*_-+=?");
            case GeneratorSheet::CharClass::Extended:
                break;
            }
            return QString::fromUtf8("àéîõüçñß");
        }
    } // namespace

    /** The 8px entropy pill: a container track with a green fill. */
    class EntropyMeter : public QWidget
    {
    public:
        explicit EntropyMeter(QWidget* parent = nullptr)
            : QWidget(parent)
        {
            setFixedHeight(MeterHeight);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }

        void setFraction(qreal fraction)
        {
            m_fraction = qBound(0.0, fraction, 1.0);
            update();
        }

    protected:
        void paintEvent(QPaintEvent* event) override
        {
            Q_UNUSED(event)
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            paintSurface(&painter, rect(), Shape::Full, theme()->color(Role::SurfaceContainerHighest));

            const int filled = qRound(width() * m_fraction);
            if (filled > 0) {
                // Never narrower than the pill is tall, so a low score still reads.
                const QRect bar(0, 0, qMax(filled, height()), height());
                paintSurface(&painter, bar, Shape::Full, theme()->color(Role::Green));
            }
        }

    private:
        qreal m_fraction = 0.0;
    };

    /**
     * One character class pill.
     *
     * Deliberately not a Material::Chip: the design's charset pill is taller
     * than the chip scale, carries no leading check glyph, fills with primary
     * rather than the secondary container, and labels itself in the mono face -
     * which Chip paints in the body face whatever font it is given.
     */
    class CharsetPill : public QAbstractButton
    {
    public:
        explicit CharsetPill(const QString& text, QWidget* parent = nullptr)
            : QAbstractButton(parent)
        {
            setText(text);
            setCheckable(true);
            setCursor(Qt::PointingHandCursor);
            setFont(pillFont());
        }

        QSize sizeHint() const override
        {
            return {fontMetrics().horizontalAdvance(text()) + 2 * CharsetPadding, CharsetHeight};
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
            if (isChecked()) {
                fill = theme()->color(Role::Primary);
                content = theme()->color(Role::OnPrimary);
            } else {
                border = theme()->color(Role::Outline);
                content = theme()->color(Role::OnSurfaceVariant);
                if (m_hovered || isDown()) {
                    fill = theme()->color(Role::SurfaceContainerHigh);
                }
            }

            paintSurface(&painter, rect(), Shape::Small, fill, border);
            painter.setPen(content);
            painter.setFont(font());
            painter.drawText(
                rect(), Qt::AlignCenter, painter.fontMetrics().elidedText(text(), Qt::ElideRight, width() - 8));
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

    // -------------------------------------------------------------- generator

    GeneratorSheet::GeneratorSheet(QWidget* parent)
        : Overlay(parent)
    {
        m_sheet = new GeneratorPanel(Shape::ExtraLarge, Role::SurfaceContainerLowest);

        auto* layout = new QVBoxLayout(m_sheet);
        layout->setContentsMargins(SheetPadding, 24, SheetPadding, 22);
        layout->setSpacing(14);
        layout->addWidget(buildHeader());
        layout->addWidget(buildValueBox());
        layout->addWidget(buildMeter());
        layout->addWidget(buildLength());
        layout->addWidget(buildCharsets());
        layout->addWidget(buildFooter());

        setSheetWidth(SheetWidth);
        setSheetWidget(m_sheet);

        connect(theme(), &Theme::changed, this, &GeneratorSheet::applyTheme);
        applyTheme();
        regenerate();
    }

    GeneratorSheet::~GeneratorSheet() = default;

    QString GeneratorSheet::password() const
    {
        return m_password;
    }

    int GeneratorSheet::length() const
    {
        return m_lengthSlider->value();
    }

    void GeneratorSheet::setLength(int length)
    {
        m_lengthSlider->setValue(qBound(MinLength, length, MaxLength));
    }

    bool GeneratorSheet::isClassEnabled(CharClass charClass) const
    {
        CharsetPill* pill = m_charsetPills.value(static_cast<int>(charClass));
        return pill && pill->isChecked();
    }

    void GeneratorSheet::setClassEnabled(CharClass charClass, bool enabled)
    {
        if (CharsetPill* pill = m_charsetPills.value(static_cast<int>(charClass))) {
            pill->setChecked(enabled);
        }
    }

    double GeneratorSheet::entropyBits() const
    {
        const QString pool = characterPool();
        if (pool.isEmpty()) {
            return 0.0;
        }
        return length() * std::log2(static_cast<double>(pool.size()));
    }

    void GeneratorSheet::regenerate()
    {
        const QString pool = characterPool();
        m_password.clear();
        if (!pool.isEmpty()) {
            const auto poolSize = static_cast<quint32>(pool.size());
            auto* generator = QRandomGenerator::system();
            m_password.reserve(length());
            for (int i = 0; i < length(); ++i) {
                m_password.append(pool.at(static_cast<int>(generator->bounded(poolSize))));
            }
        }
        updateReadouts();
    }

    void GeneratorSheet::aboutToOpen()
    {
        regenerate();
    }

    QWidget* GeneratorSheet::buildHeader()
    {
        auto* header = new QWidget;
        auto* layout = new QHBoxLayout(header);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);

        auto* symbol = new QLabel;
        symbol->setObjectName(QStringLiteral("generatorSymbol"));
        symbol->setPixmap(Icons::pixmap(QStringLiteral("casino"), 26, theme()->color(Role::Primary)));
        layout->addWidget(symbol);
        layout->addWidget(makeLabel(tr("Password generator"), TypeRole::TitleLarge, Role::OnSurface), 1);

        auto* close = new IconButton(QStringLiteral("close"));
        close->setToolTip(tr("Close"));
        connect(close, &IconButton::clicked, this, &Overlay::closeOverlay);
        layout->addWidget(close);

        return header;
    }

    QWidget* GeneratorSheet::buildValueBox()
    {
        // The value box holds nothing but the value; copying and regenerating
        // are the footer's two actions.
        auto* box = new GeneratorPanel(ValueRadius, Role::SurfaceContainer);
        auto* layout = new QHBoxLayout(box);
        layout->setContentsMargins(ValuePadding, ValuePadding, ValuePadding, ValuePadding);
        layout->setSpacing(8);

        m_valueLabel = new QLabel;
        m_valueLabel->setWordWrap(true);
        m_valueLabel->setFont(valueFont());
        layout->addWidget(m_valueLabel, 1);

        return box;
    }

    QWidget* GeneratorSheet::buildMeter()
    {
        auto* row = new QWidget;
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        m_meter = new EntropyMeter;
        layout->addWidget(m_meter, 1);

        m_entropyLabel = makeLabel(QString(), TypeRole::LabelMedium, Role::Green);
        layout->addWidget(m_entropyLabel);

        return row;
    }

    QWidget* GeneratorSheet::buildLength()
    {
        auto* block = new QWidget;
        auto* layout = new QVBoxLayout(block);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        auto* caption = new QHBoxLayout;
        caption->setContentsMargins(0, 0, 0, 0);
        caption->setSpacing(8);
        caption->addWidget(makeLabel(tr("Length"), TypeRole::BodySmall, Role::OnSurfaceVariant), 1);
        m_lengthValue = makeLabel(QString::number(DefaultLength), TypeRole::LabelMedium, Role::OnSurface);
        caption->addWidget(m_lengthValue);
        layout->addLayout(caption);

        m_lengthSlider = new Slider(Qt::Horizontal);
        m_lengthSlider->setRange(MinLength, MaxLength);
        m_lengthSlider->setValue(DefaultLength);
        m_lengthSlider->setPageStep(4);
        connect(m_lengthSlider, &QSlider::valueChanged, this, &GeneratorSheet::regenerate);
        layout->addWidget(m_lengthSlider);

        return block;
    }

    QWidget* GeneratorSheet::buildCharsets()
    {
        struct Charset
        {
            GeneratorSheet::CharClass charClass;
            QString label;
            bool enabled;
        };

        // The last two pills are samples of their pool, not words: the design
        // labels every class in the same monospace shorthand.
        const QList<Charset> charsets = {{CharClass::Upper, QString::fromUtf8("A–Z"), true},
                                         {CharClass::Lower, QString::fromUtf8("a–z"), true},
                                         {CharClass::Digits, QString::fromUtf8("0–9"), true},
                                         {CharClass::Special, QString::fromUtf8("/*_&…"), true},
                                         {CharClass::Extended, QString::fromUtf8("À–ÿ"), false}};

        auto* block = new QWidget;
        auto* layout = new QVBoxLayout(block);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(ChipGap);

        QList<QWidget*> pills;
        for (const Charset& charset : charsets) {
            auto* pill = new CharsetPill(charset.label);
            pill->setChecked(charset.enabled);
            connect(pill, &QAbstractButton::toggled, this, [this, pill](bool checked) {
                // The generator always keeps at least one pool to draw from.
                if (!checked && characterPool().isEmpty()) {
                    pill->setChecked(true);
                    return;
                }
                regenerate();
            });
            m_charsetPills.insert(static_cast<int>(charset.charClass), pill);
            pills.append(pill);
        }
        wrapIntoRows(layout, pills, SheetWidth - 2 * SheetPadding, ChipGap);

        return block;
    }

    QWidget* GeneratorSheet::buildFooter()
    {
        auto* footer = new QWidget;
        auto* layout = new QHBoxLayout(footer);
        layout->setContentsMargins(0, 6, 0, 0);
        layout->setSpacing(ActionGap);

        // Two equal pills, not a right-aligned dialog row: regenerating and
        // copying are the only two things this sheet does.
        auto* regenerateButton = new OutlinedButton(QStringLiteral("refresh"), tr("Regenerate"));
        regenerateButton->setFixedHeight(ActionHeight);
        regenerateButton->setSymbolSize(ActionSymbolSize);
        connect(regenerateButton, &ButtonBase::clicked, this, &GeneratorSheet::regenerate);
        layout->addWidget(regenerateButton, 1);

        m_copyButton = new FilledButton(QStringLiteral("content_copy"), tr("Copy"));
        m_copyButton->setFixedHeight(ActionHeight);
        m_copyButton->setSymbolSize(ActionSymbolSize);
        connect(m_copyButton, &ButtonBase::clicked, this, [this] { emit passwordCopied(m_password); });
        layout->addWidget(m_copyButton, 1);

        return footer;
    }

    QString GeneratorSheet::characterPool() const
    {
        QString pool;
        for (auto it = m_charsetPills.cbegin(); it != m_charsetPills.cend(); ++it) {
            if (it.value()->isChecked()) {
                pool.append(poolFor(static_cast<CharClass>(it.key())));
            }
        }
        return pool;
    }

    void GeneratorSheet::updateReadouts()
    {
        const int valueWidth = m_valueLabel->width() > 0 ? m_valueLabel->width() : DefaultValueWidth;
        m_valueLabel->setText(wrapValue(m_password, m_valueLabel->font(), valueWidth));
        m_lengthValue->setText(QString::number(length()));

        const double bits = entropyBits();
        m_meter->setFraction(bits / FullEntropy);
        m_entropyLabel->setText(tr("%1 bits").arg(qRound(bits)));
        m_copyButton->setEnabled(!m_password.isEmpty());
    }

    void GeneratorSheet::applyTheme()
    {
        restyleLabels(m_sheet);

        if (auto* symbol = m_sheet->findChild<QLabel*>(QStringLiteral("generatorSymbol"))) {
            symbol->setPixmap(Icons::pixmap(QStringLiteral("casino"), 26, theme()->color(Role::Primary)));
        }

        m_valueLabel->setFont(valueFont());
        m_valueLabel->setStyleSheet(
            QStringLiteral("color:%1;background:transparent;").arg(theme()->hex(Role::OnSurface)));

        // The length slider is a Material::Slider and paints itself from the
        // theme, so it needs no stylesheet here.

        const QFont chipFont = pillFont();
        for (CharsetPill* pill : std::as_const(m_charsetPills)) {
            pill->setFont(chipFont);
            pill->updateGeometry();
        }

        // The value is re-wrapped: a new type scale means a new line length.
        updateReadouts();
        m_sheet->update();
    }

} // namespace Material
