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
#include "gui/Application.h"
#include "keys/drivers/YubiKey.h"

#include <QEnterEvent>
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

#include <iterator>

namespace Material
{
    namespace
    {
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

        QString languageSample(const QString& language)
        {
            if (language == QLatin1String("en")) {
                return QStringLiteral("Entry copied to the clipboard. It clears in ten seconds.");
            }
            if (language.startsWith(QLatin1String("zh"))) {
                return QStringLiteral("項目已複製到剪貼簿，十秒後自動清除。");
            }
            return SettingsScreen::tr("Entry copied to the clipboard. It clears in ten seconds.");
        }

        QString fontSizeLabel(int step)
        {
            const QStringList steps{SettingsScreen::tr("Smallest"),
                                    SettingsScreen::tr("Small"),
                                    SettingsScreen::tr("Default"),
                                    SettingsScreen::tr("Large"),
                                    SettingsScreen::tr("Largest")};
            return steps.at(qBound(1, step, static_cast<int>(steps.size())) - 1);
        }

        QString recentCountLabel(int count)
        {
            return count == 1 ? SettingsScreen::tr("1 database") : SettingsScreen::tr("%1 databases").arg(count);
        }

        /** The readout on the interface font row, e.g. "Roboto · 10 pt · Regular". */
        QString fontRowText()
        {
            return SettingsScreen::tr("%1 · %2 pt · Regular")
                .arg(Theme::uiFamily())
                .arg(theme()->font(TypeRole::BodyMedium).pointSize());
        }

        QString voiceSegmentId(Voice::Language language)
        {
            switch (language) {
            case Voice::Language::Cantonese:
                return QStringLiteral("cantonese");
            case Voice::Language::Bilingual:
                return QStringLiteral("bilingual");
            case Voice::Language::English:
                break;
            }
            return QStringLiteral("english");
        }

        Voice::Language voiceLanguageFromSegment(const QString& id)
        {
            if (id == QLatin1String("cantonese")) {
                return Voice::Language::Cantonese;
            }
            if (id == QLatin1String("bilingual")) {
                return Voice::Language::Bilingual;
            }
            return Voice::Language::English;
        }

        /** The samples the voice preview renders: a routine message and an error. */
        struct VoiceSample
        {
            const char* key;
            Voice::Category category;
        };

        constexpr VoiceSample VoiceSamples[] = {{"clipboard.copied", Voice::Category::Success},
                                                {"database.save.failed", Voice::Category::Error}};

        QVariantMap voiceSampleArgs()
        {
            return QVariantMap{
                {QStringLiteral("seconds"), config()->get(Config::Security_ClearClipboardTimeout).toInt()},
                {QStringLiteral("name"), QStringLiteral("Personal.kdbx")},
                {QStringLiteral("error"), SettingsScreen::tr("there is no space left on the device")}};
        }
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

        const int left = glyph.right() + 14;
        const int right = m_status->geometry().left() - 14;
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
            search->setPlaceholder(tr("Search settings"));
            search->setMaximumWidth(380);
            connect(search, &SearchBar::textChanged, this, &SettingsScreen::applyFilter);
        }

        auto* grid = new QGridLayout;
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(16);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        grid->addWidget(createAppearanceCard(), 0, 0);
        grid->addWidget(createLanguageCard(), 0, 1);
        grid->addWidget(createVoiceCard(), 1, 0);
        grid->addWidget(createBehaviourCard(), 1, 1);
        grid->addWidget(createIntegrationsCard(), 2, 0);
        grid->setRowStretch(3, 1);

        contentLayout()->addLayout(grid);
        contentLayout()->addStretch(1);

        connect(theme(), &Theme::changed, this, &SettingsScreen::refreshFromTheme);
        connect(Voice::notifier(), &Voice::Notifier::changed, this, &SettingsScreen::refreshFromVoice);
    }

    SettingsScreen::~SettingsScreen() = default;

    Card* SettingsScreen::createAppearanceCard()
    {
        auto* card = new Card(Card::Variant::Outlined, Shape::ExtraLarge);
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

        {
            const QString label = tr("Dim sum surprise");
            const QString sub = tr("One launch in a hundred, a dim sum dish drops into the corner for a few seconds.");

            auto* row = new QHBoxLayout;
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(16);

            auto* text = new QVBoxLayout;
            text->setContentsMargins(0, 0, 0, 0);
            text->setSpacing(2);
            text->addWidget(makeLabel(label, TypeRole::BodyMedium, Role::OnSurface));
            auto* note = makeLabel(sub, TypeRole::LabelMedium, Role::OnSurfaceVariant);
            note->setWordWrap(true);
            text->addWidget(note);
            row->addLayout(text, 1);

            auto* toggle = new Switch;
            toggle->setCheckable(true);
            toggle->setChecked(config()->get(Config::GUI_DimSumSurprise).toBool());
            connect(toggle, &QAbstractButton::toggled, this, [](bool checked) {
                config()->set(Config::GUI_DimSumSurprise, checked);
            });
            row->addWidget(toggle, 0, Qt::AlignVCenter);

            content->addLayout(row);
            content->addSpacing(14);
            haystack << label << sub;
        }

        content->addWidget(
            makeLabel(tr("The seed colour drives every accent in the application; density drives every row height."),
                      TypeRole::LabelMedium,
                      Role::OnSurfaceVariant));

        m_cards.append({card, haystack.join(QLatin1Char(' ')).toLower()});
        return card;
    }

    Card* SettingsScreen::createLanguageCard()
    {
        auto* card = new Card(Card::Variant::Outlined, Shape::ExtraLarge);
        card->setTitleText(tr("Language & text"));

        auto* content = card->contentLayout();
        content->setSpacing(0);
        QStringList haystack{tr("Language & text")};

        const QString language = config()->get(Config::GUI_Language).toString();

        m_languageSegment = new SegmentedButton;
        m_languageSegment->addSegment(QStringLiteral("system"), tr("System"));
        m_languageSegment->addSegment(QStringLiteral("en"), QStringLiteral("English"));
        m_languageSegment->addSegment(QStringLiteral("zh_TW"), QStringLiteral("中文"));
        // Keep a language chosen elsewhere selectable instead of silently losing it.
        if (language != QLatin1String("system") && language != QLatin1String("en")
            && language != QLatin1String("zh_TW")) {
            m_languageSegment->addSegment(language, language);
        }
        m_languageSegment->setCurrentSegment(language);
        connect(m_languageSegment, &SegmentedButton::segmentSelected, this, [this](const QString& id) {
            if (m_updating) {
                return;
            }
            config()->set(Config::GUI_Language, id);
            updateLanguagePreview();
        });
        content->addWidget(m_languageSegment);
        content->addSpacing(8);
        haystack << tr("System") << QStringLiteral("English") << QStringLiteral("中文") << language;

        content->addWidget(makeLabel(tr("A new language takes effect the next time KeePassXC starts."),
                                     TypeRole::LabelMedium,
                                     Role::OnSurfaceVariant));
        content->addSpacing(16);

        auto sliderRow = [&haystack, content](const QString& label, QLabel** value) {
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
            slider->setRange(1, 5);
            slider->setSingleStep(1);
            slider->setPageStep(1);
            content->addWidget(slider);
            content->addSpacing(18);
            return slider;
        };

        m_fontSizeSlider = sliderRow(tr("Interface font size"), &m_fontSizeValue);
        m_fontSizeSlider->setValue(qBound(-2, config()->get(Config::GUI_FontSizeOffset).toInt(), 2) + 3);
        m_fontSizeValue->setText(fontSizeLabel(m_fontSizeSlider->value()));
        connect(m_fontSizeSlider, &QSlider::valueChanged, this, [this](int value) {
            m_fontSizeValue->setText(fontSizeLabel(value));
            updateLanguagePreview();
            // Dragging only moves the preview; the application font follows on release.
            if (!m_updating && !m_fontSizeSlider->isSliderDown()) {
                commitFontSize();
            }
        });
        connect(m_fontSizeSlider, &QSlider::sliderReleased, this, &SettingsScreen::commitFontSize);

        m_recentSlider = sliderRow(tr("Recent databases remembered"), &m_recentValue);
        m_recentSlider->setValue(qBound(1, config()->get(Config::NumberOfRememberedLastDatabases).toInt(), 5));
        m_recentValue->setText(recentCountLabel(m_recentSlider->value()));
        connect(m_recentSlider, &QSlider::valueChanged, this, [this](int value) {
            m_recentValue->setText(recentCountLabel(value));
            if (!m_updating) {
                config()->set(Config::NumberOfRememberedLastDatabases, value);
            }
        });

        auto* preview = new Card(Card::Variant::Filled, Shape::Row);
        preview->setFillRole(Role::SurfaceContainer);
        auto* previewContent = preview->contentLayout();
        previewContent->setSpacing(6);

        auto* overline = makeLabel(tr("PREVIEW"), TypeRole::LabelSmall, Role::OnSurfaceVariant);
        QFont overlineFont = overline->font();
        overlineFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        overline->setFont(overlineFont);
        previewContent->addWidget(overline);

        m_previewLabel = makeLabel(QString(), TypeRole::BodySmall, Role::OnSurface);
        m_previewLabel->setWordWrap(true);
        previewContent->addWidget(m_previewLabel);
        content->addWidget(preview);
        updateLanguagePreview();

        m_cards.append({card, haystack.join(QLatin1Char(' ')).toLower()});
        return card;
    }

    Card* SettingsScreen::createVoiceCard()
    {
        auto* card = new Card(Card::Variant::Outlined, Shape::ExtraLarge);
        card->setTitleText(tr("Voice & humour"));

        auto* content = card->contentLayout();
        content->setSpacing(0);
        QStringList haystack{tr("Voice & humour")};

        content->addWidget(makeLabel(tr("Message language"), TypeRole::BodySmall, Role::OnSurfaceVariant));
        content->addSpacing(8);

        m_voiceSegment = new SegmentedButton;
        m_voiceSegment->addSegment(QStringLiteral("english"), QStringLiteral("English"));
        m_voiceSegment->addSegment(QStringLiteral("cantonese"), QStringLiteral("廣東話"));
        m_voiceSegment->addSegment(QStringLiteral("bilingual"), tr("Both"));
        m_voiceSegment->setCurrentSegment(voiceSegmentId(Voice::language()));
        connect(m_voiceSegment, &SegmentedButton::segmentSelected, this, [this](const QString& id) {
            if (!m_updating) {
                Voice::setLanguage(voiceLanguageFromSegment(id));
            }
        });
        content->addWidget(m_voiceSegment);
        content->addSpacing(18);
        haystack << QStringLiteral("English") << QStringLiteral("廣東話") << tr("Both") << tr("Cantonese");

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
            content->addSpacing(16);
            return slider;
        };

        m_englishFunnySlider = humourSlider(tr("English humour"), &m_englishFunnyValue);
        m_englishFunnySlider->setValue(Voice::funnyLevel(Voice::Language::English));
        m_cantoneseFunnySlider = humourSlider(tr("Cantonese humour 廣東話"), &m_cantoneseFunnyValue);
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

        auto* preview = new Card(Card::Variant::Filled, Shape::Row);
        preview->setFillRole(Role::SurfaceContainer);
        auto* previewContent = preview->contentLayout();
        previewContent->setSpacing(4);

        auto* overline = makeLabel(tr("PREVIEW"), TypeRole::LabelSmall, Role::OnSurfaceVariant);
        QFont overlineFont = overline->font();
        overlineFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        overline->setFont(overlineFont);
        previewContent->addWidget(overline);

        // One routine message and one error, because the level styles both.
        for (int sample = 0; sample < 2; ++sample) {
            if (sample > 0) {
                previewContent->addSpacing(10);
            }
            auto* primary = makeLabel(QString(), TypeRole::BodyMedium, Role::OnSurface);
            primary->setWordWrap(true);
            previewContent->addWidget(primary);
            m_voicePrimaryLabels.append(primary);

            auto* secondary = makeLabel(QString(), TypeRole::LabelMedium, Role::OnSurfaceVariant);
            secondary->setWordWrap(true);
            previewContent->addWidget(secondary);
            m_voiceSecondaryLabels.append(secondary);
        }
        content->addWidget(preview);
        content->addSpacing(14);

        auto* disclosure = makeLabel(Voice::disclosureText(), TypeRole::LabelMedium, Role::OnSurfaceVariant);
        disclosure->setWordWrap(true);
        content->addWidget(disclosure);
        content->addSpacing(12);
        haystack << Voice::disclosureText();

        auto* resetRow = new QHBoxLayout;
        resetRow->setContentsMargins(0, 0, 0, 0);
        resetRow->addStretch(1);
        auto* reset = new OutlinedButton(QStringLiteral("refresh"), tr("Reset voice to defaults"));
        reset->setRadius(Shape::Medium);
        reset->setFixedHeight(Layout::ButtonHeight);
        connect(reset, &QPushButton::clicked, this, [] { Voice::resetToDefaults(); });
        resetRow->addWidget(reset, 0, Qt::AlignRight);
        content->addLayout(resetRow);
        haystack << tr("Reset voice to defaults");

        updateVoicePreview();

        m_cards.append({card, haystack.join(QLatin1Char(' ')).toLower()});
        return card;
    }

    Card* SettingsScreen::createBehaviourCard()
    {
        auto* card = new Card(Card::Variant::Outlined, Shape::ExtraLarge);
        card->setTitleText(tr("Behaviour"));

        auto* content = card->contentLayout();
        content->setSpacing(0);
        QStringList haystack{tr("Behaviour")};

        const QVector<ToggleSpec> toggles = {
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
            {Config::Security_LockDatabaseIdle,
             tr("Lock when idle"),
             tr("Locks the database after the inactivity timeout expires.")},
            {Config::Security_ClearClipboard,
             tr("Clear the clipboard"),
             tr("Applies to passwords, TOTP codes and custom attributes.")},
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
            if (i + 1 < toggles.size()) {
                content->addWidget(makeSeparator());
            }
            haystack << spec.label << spec.sub;
        }

        m_cards.append({card, haystack.join(QLatin1Char(' ')).toLower()});
        return card;
    }

    Card* SettingsScreen::createIntegrationsCard()
    {
        auto* card = new Card(Card::Variant::Outlined, Shape::ExtraLarge);
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
        };

        addRow(QStringLiteral("external-editor"),
               QStringLiteral("edit_document"),
               tr("External editor"),
               tr("Opens notes and attachments in the system editor."),
               PillKind::Action,
               tr("Configure"));

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
        m_fontSizeSlider->setValue(qBound(-2, config()->get(Config::GUI_FontSizeOffset).toInt(), 2) + 3);
        for (auto* swatch : m_swatches) {
            swatch->update();
        }

        restyleChildren(this);
        m_fontRowButton->setText(fontRowText());
        updateLanguagePreview();
        updateVoicePreview();

        m_updating = false;
    }

    void SettingsScreen::refreshFromVoice()
    {
        m_updating = true;

        m_voiceSegment->setCurrentSegment(voiceSegmentId(Voice::language()));
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
        if (m_voicePrimaryLabels.isEmpty()) {
            return;
        }

        const Voice::Language language =
            m_voiceSegment ? voiceLanguageFromSegment(m_voiceSegment->currentSegment()) : Voice::language();
        const int english = m_englishFunnySlider->value();
        const int cantonese = m_cantoneseFunnySlider->value();

        m_englishFunnyValue->setText(tr("%1 · %2").arg(english).arg(Voice::levelName(english)));
        m_cantoneseFunnyValue->setText(tr("%1 · %2").arg(cantonese).arg(Voice::levelName(cantonese)));

        const QVariantMap args = voiceSampleArgs();
        const int samples = qMin(m_voicePrimaryLabels.size(), static_cast<int>(std::size(VoiceSamples)));
        for (int i = 0; i < samples; ++i) {
            const Voice::Line line = Voice::preview(
                language, english, cantonese, QLatin1String(VoiceSamples[i].key), args, VoiceSamples[i].category);
            m_voicePrimaryLabels.at(i)->setText(line.primary);
            m_voiceSecondaryLabels.at(i)->setText(line.secondary);
            m_voiceSecondaryLabels.at(i)->setVisible(line.hasSecondary());
        }
    }

    void SettingsScreen::commitFontSize()
    {
        config()->set(Config::GUI_FontSizeOffset, m_fontSizeSlider->value() - 3);
        Application::applyFontSize();
        theme()->reload();
    }

    int SettingsScreen::previewPointSize() const
    {
        const int applied = qBound(-2, config()->get(Config::GUI_FontSizeOffset).toInt(), 2);
        const int pending = m_fontSizeSlider ? m_fontSizeSlider->value() - 3 : applied;
        return qMax(1, theme()->font(TypeRole::BodyMedium).pointSize() + pending - applied);
    }

    void SettingsScreen::updateLanguagePreview()
    {
        if (!m_previewLabel) {
            return;
        }
        const QString language =
            m_languageSegment ? m_languageSegment->currentSegment() : config()->get(Config::GUI_Language).toString();
        m_previewLabel->setText(languageSample(language));

        QFont font = theme()->font(TypeRole::BodyMedium);
        font.setPointSize(previewPointSize());
        m_previewLabel->setFont(font);
    }

} // namespace Material
