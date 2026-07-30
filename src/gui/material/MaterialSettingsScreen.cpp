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

#include "MaterialSettingsScreen.h"

#include "MaterialButtons.h"
#include "MaterialCard.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialSearchBar.h"
#include "MaterialSegmentedButton.h"
#include "MaterialSwitch.h"
#include "MaterialVoice.h"

#include "config-keepassx.h"
#include "core/Config.h"
#include "keys/drivers/YubiKey.h"

#include <QEnterEvent>
#include <QFontInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSlider>
#include <QStringList>
#include <QVBoxLayout>
#include <QVector>

namespace Material
{
    namespace
    {
        /** The width the design's two column card grid stops growing at. */
        constexpr int GridMaxWidth = 1180;

        /** Dynamic properties that let a label be restyled after a theme change. */
        const char* const TypeProperty = "materialTypeRole";
        const char* const ColorProperty = "materialColorRole";

        void applyLabelStyle(QLabel* label)
        {
            const QVariant type = label->property(TypeProperty);
            const QVariant color = label->property(ColorProperty);
            if (!type.isValid() || !color.isValid()) {
                return;
            }
            label->setFont(theme()->font(static_cast<TypeRole>(type.toInt())));
            QPalette palette = label->palette();
            const QColor resolved = theme()->color(static_cast<Role>(color.toInt()));
            palette.setColor(QPalette::WindowText, resolved);
            palette.setColor(QPalette::Text, resolved);
            label->setPalette(palette);
        }

        QLabel* makeLabel(const QString& text, TypeRole type, Role color, QWidget* parent = nullptr)
        {
            auto* label = new QLabel(text, parent);
            label->setProperty(TypeProperty, static_cast<int>(type));
            label->setProperty(ColorProperty, static_cast<int>(color));
            applyLabelStyle(label);
            return label;
        }

        /** The hairline between two behaviour rows. */
        QFrame* makeSeparator(QWidget* parent = nullptr)
        {
            auto* line = new QFrame(parent);
            line->setFrameShape(QFrame::NoFrame);
            line->setFixedHeight(1);
            line->setAutoFillBackground(true);
            QPalette palette = line->palette();
            palette.setColor(QPalette::Window, theme()->color(Role::OutlineVariant));
            line->setPalette(palette);
            line->setProperty(ColorProperty, static_cast<int>(Role::OutlineVariant));
            return line;
        }

        /** Restyle every label and separator the screen owns. */
        void restyleChildren(QWidget* root)
        {
            for (auto* label : root->findChildren<QLabel*>()) {
                applyLabelStyle(label);
            }
            for (auto* line : root->findChildren<QFrame*>()) {
                if (!line->property(ColorProperty).isValid()) {
                    continue;
                }
                QPalette palette = line->palette();
                palette.setColor(QPalette::Window, theme()->color(Role::OutlineVariant));
                line->setPalette(palette);
            }
        }

        /** The behaviour card, in the order the design lists it. */
        struct ToggleSpec
        {
            Config::ConfigKey key;
            QString label;
            QString sub;
        };

        /** The readout on the interface font row, e.g. "Roboto · 14px · Regular". */
        QString fontRowText()
        {
            // The design reports the rendered pixel size. The type scale is
            // built in points, so the resolved size is asked for rather than
            // read back: QFont::pixelSize() is -1 for a font sized in points.
            const QFont body = theme()->font(TypeRole::BodyMedium);
            return SettingsScreen::tr("%1 · %2px · Regular")
                .arg(Theme::uiFamily())
                .arg(QFontInfo(body).pixelSize());
        }

        /** The design's five names for an English humour level. */
        QString englishLevelName(int level)
        {
            switch (qBound(Voice::MinLevel, level, Voice::MaxLevel)) {
            case 1:
                return SettingsScreen::tr("Fully serious");
            case 2:
                return SettingsScreen::tr("Dry");
            case 3:
                return SettingsScreen::tr("Warm");
            case 4:
                return SettingsScreen::tr("Cheeky");
            default:
                break;
            }
            return SettingsScreen::tr("Maximum");
        }

        /** The design's five Cantonese tier names, which it leaves untranslated. */
        QString cantoneseLevelName(int level)
        {
            switch (qBound(Voice::MinLevel, level, Voice::MaxLevel)) {
            case 1:
                return QStringLiteral("正經八百");
            case 2:
                return QStringLiteral("淡淡定");
            case 3:
                return QStringLiteral("有啲鬼馬");
            case 4:
                return QStringLiteral("好鬼馬");
            default:
                break;
            }
            return QStringLiteral("癲晒");
        }

        QString languageSegmentId(Voice::Language language)
        {
            switch (language) {
            case Voice::Language::Cantonese:
                return QStringLiteral("zh");
            case Voice::Language::Bilingual:
                return QStringLiteral("both");
            case Voice::Language::English:
                break;
            }
            return QStringLiteral("en");
        }

        Voice::Language languageFromSegment(const QString& id)
        {
            if (id == QLatin1String("zh")) {
                return Voice::Language::Cantonese;
            }
            if (id == QLatin1String("both")) {
                return Voice::Language::Bilingual;
            }
            return Voice::Language::English;
        }

        /**
         * The Qt translation a language segment asks for. Bilingual answers with
         * an empty string: it is a voice, not a shipped translation, so it
         * leaves the interface language where the user put it.
         */
        QString translationForSegment(const QString& id)
        {
            if (id == QLatin1String("zh")) {
                return QStringLiteral("zh_TW");
            }
            if (id == QLatin1String("en")) {
                return QStringLiteral("en");
            }
            return QString();
        }

        /** The message the live preview renders, as the design does. */
        const char* const PreviewKey = "entry.deleted";

        QVariantMap previewArgs()
        {
            return QVariantMap{{QStringLiteral("title"), SettingsScreen::tr("Example entry")}};
        }

        /**
         * A settings card.
         *
         * The design fills every card with surfaceContainerLow and still draws
         * the outlineVariant hairline. Card paints a fill for its Filled variant
         * and a border for its Outlined one but never both, so it paints itself.
         */
        class SettingsCard : public Card
        {
        public:
            explicit SettingsCard(QWidget* parent = nullptr)
                // Qualified: QFrame::Shape shadows Material::Shape in here.
                : Card(Card::Variant::Outlined, Material::Shape::ExtraLarge, parent)
            {
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing, true);
                paintSurface(&painter,
                             rect(),
                             radius(),
                             theme()->color(Role::SurfaceContainerLow),
                             theme()->color(Role::OutlineVariant));
            }
        };

        /**
         * The live preview panel.
         *
         * A rounded-16 surfaceContainer panel with the design's own 14px / 16px
         * padding, which a Card cannot carry: Card takes its padding from the
         * density's page padding and puts it back on every theme change.
         */
        class PreviewPanel : public QWidget
        {
        public:
            static constexpr int PaddingX = 16;
            static constexpr int PaddingY = 14;

            explicit PreviewPanel(QWidget* parent = nullptr)
                : QWidget(parent)
            {
                m_layout = new QVBoxLayout(this);
                m_layout->setContentsMargins(PaddingX, PaddingY, PaddingX, PaddingY);
                m_layout->setSpacing(6);
            }

            /** The column the overline and the preview line go into. */
            QVBoxLayout* contentLayout() const
            {
                return m_layout;
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing, true);
                paintSurface(&painter, rect(), Shape::Row, theme()->color(Role::SurfaceContainer));
            }

        private:
            QVBoxLayout* m_layout = nullptr;
        };
    } // namespace

    // ----------------------------------------------------------------- SeedSwatch

    SeedSwatch::SeedSwatch(Seed seed, QWidget* parent)
        : QAbstractButton(parent)
        , m_seed(seed)
    {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(Extent, Extent);
        setToolTip(Theme::seedDisplayName(seed));
    }

    SeedSwatch::~SeedSwatch() = default;

    Seed SeedSwatch::seed() const
    {
        return m_seed;
    }

    QSize SeedSwatch::sizeHint() const
    {
        return QSize(Extent, Extent);
    }

    QSize SeedSwatch::minimumSizeHint() const
    {
        return QSize(Extent, Extent);
    }

    void SeedSwatch::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool active = theme()->seed() == m_seed;
        const QRectF swatch(QRectF(rect()).center().x() - Diameter / 2.0,
                            QRectF(rect()).center().y() - Diameter / 2.0,
                            Diameter,
                            Diameter);

        if (active) {
            // A 3px primary halo, held off the swatch by the surface behind it.
            QPen halo(theme()->color(Role::Primary));
            halo.setWidthF(3.0);
            painter.setPen(halo);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5));
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(Theme::seedSwatch(m_seed));
        painter.drawEllipse(swatch);

        if (active) {
            const QRect glyph(qRound(swatch.center().x()) - 10, qRound(swatch.center().y()) - 10, 20, 20);
            painter.drawPixmap(glyph, Icons::pixmap(QStringLiteral("check"), 20, QColor(Qt::white)));
        }
    }

    // -------------------------------------------------------------- IntegrationRow

    IntegrationRow::IntegrationRow(const QString& symbol, const QString& title, QWidget* parent)
        : QWidget(parent)
        , m_symbol(symbol)
        , m_title(title)
    {
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_NoMousePropagation, true);
        setMinimumHeight(RowHeight);

        m_status = new PillLabel(PillKind::Off, QString(), this);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 10, 16, 10);
        layout->setSpacing(14);
        layout->addStretch(1);
        layout->addWidget(m_status, 0, Qt::AlignVCenter);
    }

    IntegrationRow::~IntegrationRow() = default;

    void IntegrationRow::setDetail(const QString& detail)
    {
        m_detail = detail;
        update();
    }

    void IntegrationRow::setStatus(PillKind kind, const QString& text)
    {
        m_status->setPillKind(kind);
        m_status->setPillText(text);
        update();
    }

    void IntegrationRow::setChevron(bool chevron)
    {
        m_chevron = chevron;
        // The two trailing treatments are exclusive: the design's editor row
        // ends in a chevron, the three status rows end in a pill.
        m_status->setVisible(!chevron);
        update();
    }

    QSize IntegrationRow::sizeHint() const
    {
        return QSize(280 + m_status->sizeHint().width(), RowHeight);
    }

    QSize IntegrationRow::minimumSizeHint() const
    {
        return QSize(200 + m_status->minimumSizeHint().width(), RowHeight);
    }

    void IntegrationRow::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        paintSurface(&painter,
                     rect(),
                     Shape::Row,
                     theme()->color(m_hovered ? Role::SurfaceContainerHigh : Role::SurfaceContainer));

        const QRect glyph(16, (height() - 22) / 2, 22, 22);
        painter.drawPixmap(glyph, Icons::pixmap(m_symbol, 22, theme()->color(Role::OnSurfaceVariant)));

        if (m_chevron) {
            const QRect chevron(width() - TrailingMargin - ChevronSize,
                                (height() - ChevronSize) / 2,
                                ChevronSize,
                                ChevronSize);
            painter.drawPixmap(
                chevron,
                Icons::pixmap(QStringLiteral("expand_more"), ChevronSize, theme()->color(Role::OnSurfaceVariant)));
        }

        const int left = glyph.right() + 14;
        const int right = m_chevron ? width() - TrailingMargin - ChevronSize - 14 : m_status->geometry().left() - 14;
        const int available = qMax(0, right - left);

        const QFont titleFont = theme()->font(TypeRole::BodyMedium);
        const QFont detailFont = theme()->font(TypeRole::LabelMedium);
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics detailMetrics(detailFont);

        const bool hasDetail = !m_detail.isEmpty();
        const int block = titleMetrics.height() + (hasDetail ? detailMetrics.height() : 0);
        int top = (height() - block) / 2;

        painter.setFont(titleFont);
        painter.setPen(theme()->color(Role::OnSurface));
        painter.drawText(QRect(left, top, available, titleMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         titleMetrics.elidedText(m_title, Qt::ElideRight, available));

        if (hasDetail) {
            top += titleMetrics.height();
            painter.setFont(detailFont);
            painter.setPen(theme()->color(Role::OnSurfaceVariant));
            painter.drawText(QRect(left, top, available, detailMetrics.height()),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             detailMetrics.elidedText(m_detail, Qt::ElideRight, available));
        }
    }

    void IntegrationRow::mouseReleaseEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            emit activated();
        }
        QWidget::mouseReleaseEvent(event);
    }

    void IntegrationRow::enterEvent(QEnterEvent* event)
    {
        m_hovered = true;
        update();
        QWidget::enterEvent(event);
    }

    void IntegrationRow::leaveEvent(QEvent* event)
    {
        m_hovered = false;
        update();
        QWidget::leaveEvent(event);
    }

    // -------------------------------------------------------------- SettingsScreen

    SettingsScreen::SettingsScreen(QWidget* parent)
        : Screen(parent)
    {
        setHeadline(tr("Settings"));
        setSupportingText(tr("Search every option label, description and current value on this surface."));
        setSearchVisible(true);
        if (auto* search = searchBar()) {
            search->setPlaceholder(tr("Search every setting"));
            search->setMaximumWidth(380);
            connect(search, &SearchBar::textChanged, this, &SettingsScreen::applyFilter);
        }

        // The design's four cards in a 2x2 grid: appearance and language above,
        // behaviour and integrations below.
        auto* grid = new QGridLayout;
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(16);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        grid->addWidget(createAppearanceCard(), 0, 0, Qt::AlignTop);
        grid->addWidget(createLanguageCard(), 0, 1, Qt::AlignTop);
        grid->addWidget(createBehaviourCard(), 1, 0, Qt::AlignTop);
        grid->addWidget(createIntegrationsCard(), 1, 1, Qt::AlignTop);
        grid->setRowStretch(2, 1);

        // The design's grid stops growing at 1180px; a host carries the cap,
        // because a layout cannot have a maximum width of its own.
        auto* gridHost = new QWidget;
        gridHost->setMaximumWidth(GridMaxWidth);
        gridHost->setLayout(grid);

        contentLayout()->addWidget(gridHost);
        contentLayout()->addStretch(1);

        connect(theme(), &Theme::changed, this, &SettingsScreen::refreshFromTheme);
        connect(Voice::notifier(), &Voice::Notifier::changed, this, &SettingsScreen::refreshFromVoice);
    }

    SettingsScreen::~SettingsScreen() = default;

    Card* SettingsScreen::createAppearanceCard()
    {
        auto* card = new SettingsCard;
        card->setTitleText(tr("Appearance"));

        auto* content = card->contentLayout();
        content->setSpacing(0);
        QStringList haystack{tr("Appearance")};

        auto caption = [&haystack](const QString& text) {
            haystack << text;
            return makeLabel(text, TypeRole::BodySmall, Role::OnSurfaceVariant);
        };

        content->addWidget(caption(tr("Theme")));
        content->addSpacing(8);

        m_themeSegment = new SegmentedButton;
        m_themeSegment->addSegment(QStringLiteral("light"), tr("Light"), QStringLiteral("light_mode"));
        m_themeSegment->addSegment(QStringLiteral("dark"), tr("Dark"), QStringLiteral("dark_mode"));
        m_themeSegment->setCurrentSegment(theme()->isDark() ? QStringLiteral("dark") : QStringLiteral("light"));
        connect(m_themeSegment, &SegmentedButton::segmentSelected, this, [this](const QString& id) {
            if (!m_updating) {
                theme()->setMode(id == QLatin1String("dark") ? Mode::Dark : Mode::Light);
            }
        });
        content->addWidget(m_themeSegment);
        content->addSpacing(18);
        haystack << tr("Light") << tr("Dark");

        content->addWidget(caption(tr("Seed colour")));
        content->addSpacing(8);

        auto* swatches = new QHBoxLayout;
        swatches->setContentsMargins(0, 0, 0, 0);
        // The swatch widget carries 4px of halo room on each side already.
        swatches->setSpacing(4);
        for (auto seed : {Seed::KeePass, Seed::Purple, Seed::Green, Seed::Amber}) {
            auto* swatch = new SeedSwatch(seed);
            connect(swatch, &QAbstractButton::clicked, this, [this, seed] {
                if (!m_updating) {
                    theme()->setSeed(seed);
                }
            });
            swatches->addWidget(swatch);
            m_swatches.append(swatch);
            haystack << Theme::seedDisplayName(seed);
        }
        swatches->addStretch(1);
        content->addLayout(swatches);
        content->addSpacing(18);

        content->addWidget(caption(tr("Density")));
        content->addSpacing(8);

        m_densitySegment = new SegmentedButton;
        m_densitySegment->addSegment(QStringLiteral("compact"), tr("Compact"));
        m_densitySegment->addSegment(QStringLiteral("comfortable"), tr("Comfortable"));
        m_densitySegment->addSegment(QStringLiteral("spacious"), tr("Spacious"));
        m_densitySegment->setCurrentSegment(Theme::densityToString(theme()->density()));
        connect(m_densitySegment, &SegmentedButton::segmentSelected, this, [this](const QString& id) {
            if (!m_updating) {
                theme()->setDensity(Theme::densityFromString(id));
            }
        });
        content->addWidget(m_densitySegment);
        content->addSpacing(18);
        haystack << tr("Compact") << tr("Comfortable") << tr("Spacious");

        auto* fontRow = new QHBoxLayout;
        fontRow->setContentsMargins(0, 0, 0, 0);
        fontRow->setSpacing(12);
        fontRow->addWidget(caption(tr("Interface font")), 1);

        m_fontRowButton = new OutlinedButton(QStringLiteral("expand_more"), fontRowText());
        m_fontRowButton->setRadius(Shape::Medium);
        m_fontRowButton->setFixedHeight(Layout::ButtonHeight);
        connect(m_fontRowButton, &QPushButton::clicked, this, &SettingsScreen::interfaceFontRequested);
        fontRow->addWidget(m_fontRowButton, 0, Qt::AlignVCenter);
        content->addLayout(fontRow);
        content->addSpacing(14);

        auto* hint =
            makeLabel(tr("The seed colour drives every accent in the application; density drives every row height."),
                      TypeRole::LabelMedium,
                      Role::OnSurfaceVariant);
        hint->setWordWrap(true);
        content->addWidget(hint);

        m_cards.append({card, haystack.join(QLatin1Char(' ')).toLower()});
        return card;
    }

    Card* SettingsScreen::createLanguageCard()
    {
        auto* card = new SettingsCard;
        card->setTitleText(tr("Language & tone"));

        auto* content = card->contentLayout();
        content->setSpacing(0);
        QStringList haystack{tr("Language & tone")};

        // The design gives language and tone a single control: it picks the
        // interface language and the language the application's own messages
        // are written in at the same time.
        m_languageSegment = new SegmentedButton;
        m_languageSegment->addSegment(QStringLiteral("en"), QStringLiteral("English"));
        m_languageSegment->addSegment(QStringLiteral("zh"), QStringLiteral("廣東話"));
        m_languageSegment->addSegment(QStringLiteral("both"), tr("Bilingual"));
        m_languageSegment->setCurrentSegment(languageSegmentId(Voice::language()));
        connect(m_languageSegment, &SegmentedButton::segmentSelected, this, [this](const QString& id) {
            if (m_updating) {
                return;
            }
            Voice::setLanguage(languageFromSegment(id));
            const QString translation = translationForSegment(id);
            if (!translation.isEmpty()) {
                config()->set(Config::GUI_Language, translation);
            }
            updateVoicePreview();
        });
        content->addWidget(m_languageSegment);
        content->addSpacing(8);
        haystack << QStringLiteral("English") << QStringLiteral("廣東話") << tr("Bilingual");

        content->addWidget(makeLabel(tr("A new interface language takes effect the next time KeePassXC starts."),
                                     TypeRole::LabelMedium,
                                     Role::OnSurfaceVariant));
        content->addSpacing(20);

        auto humourSlider = [&haystack, content](const QString& label, QLabel** value) {
            auto* row = new QHBoxLayout;
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(8);
            row->addWidget(makeLabel(label, TypeRole::BodySmall, Role::OnSurfaceVariant), 1);
            *value = makeLabel(QString(), TypeRole::LabelLarge, Role::OnSurface);
            row->addWidget(*value, 0, Qt::AlignRight);
            content->addLayout(row);
            content->addSpacing(6);
            haystack << label;

            auto* slider = new QSlider(Qt::Horizontal);
            slider->setRange(Voice::MinLevel, Voice::MaxLevel);
            slider->setSingleStep(1);
            slider->setPageStep(1);
            content->addWidget(slider);
            content->addSpacing(18);
            return slider;
        };

        m_englishFunnySlider = humourSlider(tr("Funny level — English"), &m_englishFunnyValue);
        m_englishFunnySlider->setValue(Voice::funnyLevel(Voice::Language::English));
        m_cantoneseFunnySlider = humourSlider(tr("Funny level — Cantonese"), &m_cantoneseFunnyValue);
        m_cantoneseFunnySlider->setValue(Voice::funnyLevel(Voice::Language::Cantonese));

        for (auto* slider : {m_englishFunnySlider, m_cantoneseFunnySlider}) {
            connect(slider, &QSlider::valueChanged, this, [this, slider] {
                updateVoicePreview();
                // Dragging only moves the preview; the setting follows on release.
                if (!m_updating && !slider->isSliderDown()) {
                    commitVoiceLevels();
                }
            });
            connect(slider, &QSlider::sliderReleased, this, &SettingsScreen::commitVoiceLevels);
        }

        auto* preview = new PreviewPanel;
        auto* previewContent = preview->contentLayout();

        auto* overline = makeLabel(tr("Live preview").toUpper(), TypeRole::LabelSmall, Role::OnSurfaceVariant);
        QFont overlineFont = overline->font();
        overlineFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        overline->setFont(overlineFont);
        previewContent->addWidget(overline);

        // One line, from the message catalogue, so the preview is the voice the
        // rest of the application will speak in rather than a sample of it.
        m_previewLabel = makeLabel(QString(), TypeRole::BodySmall, Role::OnSurface);
        m_previewLabel->setWordWrap(true);
        previewContent->addWidget(m_previewLabel);
        content->addWidget(preview);
        content->addSpacing(12);

        auto* disclosure = makeLabel(tr("The funny level styles every message, including errors, warnings and "
                                        "destructive confirmations. It changes voice only — what happened, what is "
                                        "affected and your options stay exact."),
                                     TypeRole::LabelMedium,
                                     Role::OnSurfaceVariant);
        disclosure->setWordWrap(true);
        content->addWidget(disclosure);
        haystack << disclosure->text();

        updateVoicePreview();

        m_cards.append({card, haystack.join(QLatin1Char(' ')).toLower()});
        return card;
    }

    Card* SettingsScreen::createBehaviourCard()
    {
        auto* card = new SettingsCard;
        card->setTitleText(tr("Security & behaviour"));

        auto* content = card->contentLayout();
        content->setSpacing(0);
        QStringList haystack{tr("Security & behaviour")};

        // The design names the two timeouts in the label rather than in a
        // second control, so the configured numbers are read back into it.
        const int idleMinutes = qMax(1, config()->get(Config::Security_LockDatabaseIdleSeconds).toInt() / 60);
        const int clipboardSeconds = config()->get(Config::Security_ClearClipboardTimeout).toInt();

        const QVector<ToggleSpec> toggles = {
            // The design's own rows, in its order.
            {Config::Security_LockDatabaseIdle,
             tr("Lock database after %n minute(s) idle", "", idleMinutes),
             tr("Also locks on screen lock and session switch.")},
            {Config::Security_ClearClipboard,
             tr("Clear clipboard after %n second(s)", "", clipboardSeconds),
             tr("Applies to passwords, TOTP codes and custom attributes.")},
            {Config::GUI_DimSumSurprise,
             tr("Dim sum surprise at startup"),
             tr("1% chance per launch. Non-blocking, auto-dismissing, bundled images only.")},
            // The rest of this surface's own rows, which the design's four
            // cards have no room for.
            {Config::SingleInstance,
             tr("Single application instance"),
             tr("A second launch raises the running window instead of opening another one.")},
            {Config::GUI_MinimizeOnStartup,
             tr("Start minimised"),
             tr("KeePassXC starts hidden and waits in the tray or task bar.")},
            {Config::MinimizeAfterUnlock,
             tr("Minimise after unlocking"),
             tr("The window steps aside once the database is open.")},
            {Config::RememberLastDatabases,
             tr("Remember recent databases"),
             tr("Keeps the recent list on the welcome screen and in the file menu.")},
            {Config::OpenPreviousDatabasesOnStartup,
             tr("Reopen previous databases"),
             tr("The databases open at the last launch are restored on the next one.")},
            {Config::AutoSaveAfterEveryChange,
             tr("Save after every change"),
             tr("Every edit is written to disk immediately.")},
            {Config::AutoSaveOnExit, tr("Save on exit"), tr("Unsaved changes are written when the database closes.")},
            {Config::BackupBeforeSave,
             tr("Back up before saving"),
             tr("Keeps a copy of the previous file next to the database.")},
            {Config::Security_HideNotes,
             tr("Hide entry notes"),
             tr("Notes stay masked in the preview panel until they are revealed.")},
        };

        for (int i = 0; i < toggles.size(); ++i) {
            const ToggleSpec& spec = toggles.at(i);

            auto* row = new QWidget;
            row->setMinimumHeight(60);
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 10, 0, 10);
            rowLayout->setSpacing(16);

            auto* text = new QVBoxLayout;
            text->setContentsMargins(0, 0, 0, 0);
            text->setSpacing(2);
            text->addWidget(makeLabel(spec.label, TypeRole::BodyMedium, Role::OnSurface));
            auto* sub = makeLabel(spec.sub, TypeRole::LabelMedium, Role::OnSurfaceVariant);
            sub->setWordWrap(true);
            text->addWidget(sub);
            rowLayout->addLayout(text, 1);

            auto* toggle = new Switch(row);
            toggle->setCheckable(true);
            toggle->setChecked(config()->get(spec.key).toBool());
            const Config::ConfigKey key = spec.key;
            connect(toggle, &QAbstractButton::toggled, this, [key](bool checked) { config()->set(key, checked); });
            rowLayout->addWidget(toggle, 0, Qt::AlignVCenter);

            content->addWidget(row);
            // The hairline belongs to the row in the design, so the last one
            // draws it too.
            content->addWidget(makeSeparator());
            haystack << spec.label << spec.sub;
        }

        m_cards.append({card, haystack.join(QLatin1Char(' ')).toLower()});
        return card;
    }

    Card* SettingsScreen::createIntegrationsCard()
    {
        auto* card = new SettingsCard;
        card->setTitleText(tr("Integrations"));

        auto* content = card->contentLayout();
        content->setSpacing(10);
        QStringList haystack{tr("Integrations")};

        auto addRow = [this, content, &haystack](const QString& id,
                                                 const QString& symbol,
                                                 const QString& title,
                                                 const QString& detail,
                                                 PillKind kind,
                                                 const QString& status) {
            auto* row = new IntegrationRow(symbol, title);
            row->setDetail(detail);
            row->setStatus(kind, status);
            connect(row, &IntegrationRow::activated, this, [this, id] { emit integrationActivated(id); });
            content->addWidget(row);
            haystack << title << detail << status;
            return row;
        };

        // The design ends the editor row in a chevron rather than a status
        // pill: there is nothing to report, only somewhere to go.
        addRow(QStringLiteral("external-editor"),
               QStringLiteral("edit_document"),
               tr("External editor"),
               tr("Opens notes and attachments in the system editor."),
               PillKind::Off,
               QString())
            ->setChevron(true);

#ifdef KPXC_FEATURE_BROWSER
        const bool browser = config()->get(Config::Browser_Enabled).toBool();
        addRow(QStringLiteral("browser"),
               QStringLiteral("extension"),
               tr("Browser integration"),
               browser ? tr("Connected browsers can request credentials for the open database.")
                       : tr("The browser extension cannot reach this database."),
               browser ? PillKind::Good : PillKind::Off,
               browser ? tr("Active") : tr("Disabled"));
#else
        addRow(QStringLiteral("browser"),
               QStringLiteral("extension"),
               tr("Browser integration"),
               tr("This build was compiled without browser support."),
               PillKind::Off,
               tr("Unavailable"));
#endif

#ifdef KPXC_FEATURE_SSHAGENT
        const bool sshAgent = config()->get(Config::SSHAgent_Enabled).toBool();
        addRow(QStringLiteral("ssh-agent"),
               QStringLiteral("terminal"),
               tr("SSH agent"),
               sshAgent ? tr("Keys stored in the database are published while it is unlocked.")
                        : tr("Stored keys are never published to the agent."),
               sshAgent ? PillKind::Good : PillKind::Off,
               sshAgent ? tr("Active") : tr("Disabled"));
#else
        addRow(QStringLiteral("ssh-agent"),
               QStringLiteral("terminal"),
               tr("SSH agent"),
               tr("This build was compiled without SSH agent support."),
               PillKind::Off,
               tr("Unavailable"));
#endif

        const int keys = YubiKey::instance()->isInitialized() ? YubiKey::instance()->connectedKeys() : -1;
        addRow(QStringLiteral("yubikey"),
               QStringLiteral("key"),
               tr("YubiKey challenge-response"),
               keys > 0 ? tr("A hardware key is connected and ready to answer a challenge.")
                        : tr("Plug in a hardware key to use challenge-response."),
               keys > 0 ? PillKind::Good : (keys == 0 ? PillKind::Warn : PillKind::Off),
               keys > 0 ? tr("%1 connected").arg(keys) : (keys == 0 ? tr("Not present") : tr("Unavailable")));

        m_cards.append({card, haystack.join(QLatin1Char(' ')).toLower()});
        return card;
    }

    void SettingsScreen::applyFilter(const QString& text)
    {
        const QString needle = text.trimmed().toLower();
        for (const auto& entry : m_cards) {
            entry.card->setVisible(needle.isEmpty() || entry.haystack.contains(needle));
        }
        setSupportingText(
            needle.isEmpty()
                ? tr("Search every option label, description and current value on this surface.")
                : tr("Matching “%1” across Appearance, Language, Behaviour and Integrations.").arg(text.trimmed()));
    }

    void SettingsScreen::refreshFromTheme()
    {
        // Writing the resolved state back into the controls must not look like
        // the user touching them, or the theme would set itself again.
        m_updating = true;

        m_themeSegment->setCurrentSegment(theme()->isDark() ? QStringLiteral("dark") : QStringLiteral("light"));
        m_densitySegment->setCurrentSegment(Theme::densityToString(theme()->density()));
        for (auto* swatch : m_swatches) {
            swatch->update();
        }

        restyleChildren(this);
        m_fontRowButton->setText(fontRowText());
        updateVoicePreview();

        m_updating = false;
    }

    void SettingsScreen::refreshFromVoice()
    {
        m_updating = true;

        m_languageSegment->setCurrentSegment(languageSegmentId(Voice::language()));
        m_englishFunnySlider->setValue(Voice::funnyLevel(Voice::Language::English));
        m_cantoneseFunnySlider->setValue(Voice::funnyLevel(Voice::Language::Cantonese));
        updateVoicePreview();

        m_updating = false;
    }

    void SettingsScreen::commitVoiceLevels()
    {
        Voice::setFunnyLevel(Voice::Language::English, m_englishFunnySlider->value());
        Voice::setFunnyLevel(Voice::Language::Cantonese, m_cantoneseFunnySlider->value());
    }

    void SettingsScreen::updateVoicePreview()
    {
        if (!m_previewLabel) {
            return;
        }

        const Voice::Language language =
            m_languageSegment ? languageFromSegment(m_languageSegment->currentSegment()) : Voice::language();
        const int english = m_englishFunnySlider->value();
        const int cantonese = m_cantoneseFunnySlider->value();

        m_englishFunnyValue->setText(tr("%1 · %2").arg(english).arg(englishLevelName(english)));
        m_cantoneseFunnyValue->setText(tr("%1 · %2").arg(cantonese).arg(cantoneseLevelName(cantonese)));

        // The category is left to the catalogue, which classifies this message
        // as destructive - the level styles it all the same.
        const Voice::Line line =
            Voice::preview(language, english, cantonese, QLatin1String(PreviewKey), previewArgs());
        m_previewLabel->setText(line.joined());
    }

} // namespace Material
