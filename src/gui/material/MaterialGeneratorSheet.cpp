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

#include "MaterialButtons.h"
#include "MaterialChip.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QAbstractButton>
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
        constexpr int MinLength = 1;
        constexpr int MaxLength = 128;
        constexpr int DefaultLength = 24;
        constexpr int MeterHeight = 8;
        constexpr int ValuePadding = 18;
        constexpr int ChipGap = 8;
        // Width of the value column before the sheet has been laid out.
        constexpr int DefaultValueWidth =
            SheetWidth - 2 * SheetPadding - 2 * ValuePadding - 2 * Layout::IconButtonSize - 40;
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

        QString poolFor(GeneratorSheet::CharClass charClass)
        {
            switch (charClass) {
            case GeneratorSheet::CharClass::Upper:
                return QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
            case GeneratorSheet::CharClass::Lower:
                return QStringLiteral("abcdefghijklmnopqrstuvwxyz");
            case GeneratorSheet::CharClass::Digits:
                return QStringLiteral("0123456789");
            case GeneratorSheet::CharClass::Special:
                return QStringLiteral(R"(!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~)");
            case GeneratorSheet::CharClass::Extended:
                break;
            }

            // Printable Latin-1, without the invisible soft hyphen.
            QString extended;
            for (ushort code = 0xA1; code <= 0xFF; ++code) {
                if (code != 0xAD) {
                    extended.append(QChar(code));
                }
            }
            return extended;
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
        Chip* chip = m_charsetChips.value(static_cast<int>(charClass));
        return chip && chip->isChecked();
    }

    void GeneratorSheet::setClassEnabled(CharClass charClass, bool enabled)
    {
        if (Chip* chip = m_charsetChips.value(static_cast<int>(charClass))) {
            chip->setChecked(enabled);
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
        auto* box = new GeneratorPanel(Shape::Row, Role::SurfaceContainer);
        auto* layout = new QHBoxLayout(box);
        layout->setContentsMargins(ValuePadding, ValuePadding, 12, ValuePadding);
        layout->setSpacing(8);

        m_valueLabel = new QLabel;
        m_valueLabel->setWordWrap(true);
        m_valueLabel->setFont(valueFont());
        layout->addWidget(m_valueLabel, 1);

        auto* copy = new IconButton(QStringLiteral("content_copy"));
        copy->setToolTip(tr("Copy the password"));
        connect(copy, &IconButton::clicked, this, [this] { emit passwordCopied(m_password); });
        layout->addWidget(copy, 0, Qt::AlignTop);

        auto* refresh = new IconButton(QStringLiteral("refresh"));
        refresh->setToolTip(tr("Generate another password"));
        connect(refresh, &IconButton::clicked, this, &GeneratorSheet::regenerate);
        layout->addWidget(refresh, 0, Qt::AlignTop);

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

        m_lengthSlider = new QSlider(Qt::Horizontal);
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

        const QList<Charset> charsets = {{CharClass::Upper, QStringLiteral("A-Z"), true},
                                         {CharClass::Lower, QStringLiteral("a-z"), true},
                                         {CharClass::Digits, QStringLiteral("0-9"), true},
                                         {CharClass::Special, tr("Special"), true},
                                         {CharClass::Extended, tr("Extended"), false}};

        auto* block = new QWidget;
        auto* layout = new QVBoxLayout(block);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(ChipGap);

        QList<QWidget*> chips;
        for (const Charset& charset : charsets) {
            auto* chip = new Chip(QString(), charset.label, Chip::Kind::Filter);
            chip->setRadius(Shape::Small);
            chip->setCheckable(true);
            chip->setChecked(charset.enabled);
            connect(chip, &QAbstractButton::toggled, this, [this, chip](bool checked) {
                // The generator always keeps at least one pool to draw from.
                if (!checked && characterPool().isEmpty()) {
                    chip->setChecked(true);
                    return;
                }
                regenerate();
            });
            m_charsetChips.insert(static_cast<int>(charset.charClass), chip);
            chips.append(chip);
        }
        wrapIntoRows(layout, chips, SheetWidth - 2 * SheetPadding, ChipGap);

        return block;
    }

    QWidget* GeneratorSheet::buildFooter()
    {
        auto* footer = new QWidget;
        auto* layout = new QHBoxLayout(footer);
        layout->setContentsMargins(0, 6, 0, 0);
        layout->setSpacing(8);
        layout->addStretch(1);

        auto* close = new TextButton(QString(), tr("Close"));
        connect(close, &ButtonBase::clicked, this, &Overlay::closeOverlay);
        layout->addWidget(close);

        m_useButton = new FilledButton(QString(), tr("Use password"));
        connect(m_useButton, &ButtonBase::clicked, this, [this] {
            emit passwordAccepted(m_password);
            closeOverlay();
        });
        layout->addWidget(m_useButton);

        return footer;
    }

    QString GeneratorSheet::characterPool() const
    {
        QString pool;
        for (auto it = m_charsetChips.cbegin(); it != m_charsetChips.cend(); ++it) {
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
        m_useButton->setEnabled(!m_password.isEmpty());
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

        m_lengthSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal{height:6px;border-radius:3px;background:%1;}"
                           "QSlider::sub-page:horizontal{height:6px;border-radius:3px;background:%2;}"
                           "QSlider::handle:horizontal{width:16px;margin:-5px 0;border-radius:8px;background:%2;}")
                .arg(theme()->hex(Role::SecondaryContainer), theme()->hex(Role::Primary)));

        const QFont chipFont = theme()->font(TypeRole::Mono);
        for (Chip* chip : std::as_const(m_charsetChips)) {
            chip->setFont(chipFont);
        }

        // The value is re-wrapped: a new type scale means a new line length.
        updateReadouts();
        m_sheet->update();
    }

} // namespace Material
