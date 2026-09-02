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

#include "MaterialAppearanceEditor.h"

#include "core/Config.h"
#include "MaterialButtons.h"
#include "MaterialColorPicker.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialSearchBar.h"
#include "MaterialSegmentedButton.h"
#include "MaterialSelect.h"
#include "MaterialSlider.h"
#include "MaterialSwitch.h"
#include "MaterialTheme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFontDatabase>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include <functional>

namespace Material
{
    namespace
    {
        constexpr int EditorWidth = 440;
        constexpr int EditorMaxHeight = 760;
        constexpr int EditorRadius = 28;
        constexpr int EditorPadding = 20;
        constexpr int RowSpacing = 10;
        constexpr int RainbowTickMs = 50;
        constexpr double ReducedMotionHue = 0.55;
        constexpr int FontSizeMin = 8;
        constexpr int FontSizeMax = 48;
        const char* BaseStyleProperty = "materialBaseStyle";
        const char* BaseFontProperty = "materialBaseFont";
        const char* AppliedProperty = "materialOverrideApplied";
    } // namespace

    // ------------------------------------------------------ AppearanceApplier

    AppearanceApplier* AppearanceApplier::instance()
    {
        static AppearanceApplier applier;
        return &applier;
    }

    AppearanceApplier::AppearanceApplier()
    {
        connect(ElementOverrides::instance(), &ElementOverrides::overrideChanged, this, &AppearanceApplier::apply);
        m_rainbowTimer = new QTimer(this);
        m_rainbowTimer->setInterval(RainbowTickMs);
        m_rainbowTimer->setTimerType(Qt::CoarseTimer);
        connect(m_rainbowTimer, &QTimer::timeout, this, &AppearanceApplier::tick);
    }

    QString AppearanceApplier::keyFor(const QWidget* widget)
    {
        for (const QWidget* candidate = widget; candidate; candidate = candidate->parentWidget()) {
            const QString name = candidate->objectName();
            if (!name.isEmpty() && !name.startsWith(QLatin1String("qt_"))) {
                return name;
            }
        }
        return QString();
    }

    double AppearanceApplier::rainbowHue() const
    {
        return m_reducedMotion ? ReducedMotionHue : m_hue;
    }

    bool AppearanceApplier::reducedMotion() const
    {
        return m_reducedMotion;
    }

    void AppearanceApplier::setReducedMotion(bool reduced)
    {
        m_reducedMotion = reduced;
        tick();
    }

    void AppearanceApplier::tick()
    {
        m_hue = std::fmod(m_hue + static_cast<double>(RainbowTickMs) / qMax(1, m_cycleMs), 1.0);
        bool any = false;
        const QStringList keys = ElementOverrides::instance()->customisedKeys();
        for (const QString& key : keys) {
            const auto value = ElementOverrides::instance()->get(key);
            if (value.rainbow.value_or(false)) {
                any = true;
                m_cycleMs = ColorText::rainbowCycleMs(value.rainbowLevel.value_or(3));
                apply(key);
            }
        }
        if (!any) {
            m_rainbowTimer->stop();
        }
    }

    QString AppearanceApplier::styleFor(const ElementOverrides::Override& value, const QString& key) const
    {
        QString style;
        QColor background;
        if (value.rainbow.value_or(false)) {
            // Walk the wheel rather than fade through grey: full saturation,
            // a light value so text stays readable on it.
            background = QColor::fromHsvF(rainbowHue(), 0.55, 0.96);
        } else if (value.background) {
            background = *value.background;
        }
        if (background.isValid()) {
            if (value.opacity && value.elevation.value_or(0) > 0) {
                background.setAlphaF(background.alphaF() * *value.opacity);
            }
            style += QStringLiteral("background:%1;").arg(background.name(QColor::HexArgb));
        }
        if (value.foreground) style += QStringLiteral("color:%1;").arg(value.foreground->name(QColor::HexArgb));
        if (value.radius) style += QStringLiteral("border-radius:%1px;").arg(*value.radius);
        if (value.spacing) style += QStringLiteral("padding:%1px;").arg(*value.spacing);
        if (value.borderWidth) {
            const QColor border = value.borderColor.value_or(theme()->color(Role::Outline));
            style += QStringLiteral("border:%1px solid %2;").arg(*value.borderWidth).arg(border.name(QColor::HexArgb));
        }
        if (style.isEmpty()) {
            return QString();
        }
        return QStringLiteral("#%1 { %2 }").arg(key, style);
    }

    void AppearanceApplier::applyTo(QWidget* widget)
    {
        if (!widget) {
            return;
        }
        const QString key = widget->objectName();
        if (key.isEmpty()) {
            return;
        }
        const auto value = ElementOverrides::instance()->get(key);
        const bool applied = widget->property(AppliedProperty).toBool();
        if (value.isEmpty()) {
            if (applied) {
                widget->setStyleSheet(widget->property(BaseStyleProperty).toString());
                widget->setFont(widget->property(BaseFontProperty).value<QFont>());
                widget->setMinimumHeight(0);
                widget->setGraphicsEffect(nullptr);
                widget->setProperty(AppliedProperty, false);
            }
            return;
        }
        if (!applied) {
            widget->setProperty(BaseStyleProperty, widget->styleSheet());
            widget->setProperty(BaseFontProperty, widget->font());
            widget->setProperty(AppliedProperty, true);
        }
        QFont font = widget->property(BaseFontProperty).value<QFont>();
        if (value.fontFamily) font.setFamily(*value.fontFamily);
        if (value.fontSize) font.setPointSize(*value.fontSize);
        if (value.fontWeight) font.setWeight(static_cast<QFont::Weight>(*value.fontWeight));
        if (value.italic) font.setItalic(*value.italic);
        if (value.underline) font.setUnderline(*value.underline);
        if (value.strikeout) font.setStrikeOut(*value.strikeout);
        if (value.overline) font.setOverline(*value.overline);
        if (value.letterSpacing) font.setLetterSpacing(QFont::AbsoluteSpacing, *value.letterSpacing);
        if (value.capitalization) font.setCapitalization(static_cast<QFont::Capitalization>(*value.capitalization));
        widget->setFont(font);
        if (value.height) {
            widget->setMinimumHeight(*value.height);
        }
        QString style = styleFor(value, key);
        if (value.lineHeight) {
            // Qt style sheets carry no line-height for widgets, so the extra
            // leading is added as vertical padding around the text.
            const int extra = qRound((*value.lineHeight - 1.0) * QFontMetrics(font).height() / 2.0);
            if (extra > 0) {
                style += QStringLiteral(" #%1 { padding-top:%2px; padding-bottom:%2px; }").arg(key).arg(extra);
            }
        }
        widget->setStyleSheet(style);
        if (value.elevation.value_or(0) > 0) {
            auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(widget->graphicsEffect());
            if (!shadow) {
                shadow = new QGraphicsDropShadowEffect(widget);
                widget->setGraphicsEffect(shadow);
            }
            const int level = *value.elevation;
            shadow->setBlurRadius(level * 6.0);
            shadow->setOffset(0, level * 1.5);
            QColor tint(0, 0, 0);
            tint.setAlphaF(0.22 + level * 0.04);
            shadow->setColor(tint);
        } else if (value.opacity) {
            auto* effect = qobject_cast<QGraphicsOpacityEffect*>(widget->graphicsEffect());
            if (!effect) {
                effect = new QGraphicsOpacityEffect(widget);
                widget->setGraphicsEffect(effect);
            }
            effect->setOpacity(*value.opacity);
        } else if (widget->graphicsEffect()) {
            widget->setGraphicsEffect(nullptr);
        }
        if (value.rainbow.value_or(false) && !m_rainbowTimer->isActive()) {
            m_cycleMs = ColorText::rainbowCycleMs(value.rainbowLevel.value_or(3));
            m_rainbowTimer->start();
        }
    }

    void AppearanceApplier::apply(const QString& key)
    {
        if (key.isEmpty()) {
            return;
        }
        const auto widgets = QApplication::allWidgets();
        for (QWidget* widget : widgets) {
            if (widget->objectName() == key) {
                applyTo(widget);
            }
        }
    }

    bool AppearanceApplier::eventFilter(QObject* watched, QEvent* event)
    {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget) {
            return false;
        }
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Polish:
            if (!widget->objectName().isEmpty() && !ElementOverrides::instance()->get(widget->objectName()).isEmpty()) {
                applyTo(widget);
            }
            break;
        case QEvent::MouseButtonPress: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            // Shift+right-click opens the editor directly on whatever was hit.
            if (mouse->button() == Qt::RightButton && mouse->modifiers().testFlag(Qt::ShiftModifier)) {
                QWidget* editor = AppearanceEditor::instance();
                if (editor->isAncestorOf(widget) || widget == editor) {
                    return false;
                }
                QWidget* named = widget;
                while (named && (named->objectName().isEmpty() || named->objectName().startsWith(QLatin1String("qt_")))) {
                    named = named->parentWidget();
                }
                if (named) {
                    AppearanceEditor::instance()->editElement(named);
                    return true;
                }
            }
            break;
        }
        case QEvent::KeyPress: {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_E && key->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
                QWidget* focus = QApplication::focusWidget();
                QWidget* editor = AppearanceEditor::instance();
                if (focus && !editor->isAncestorOf(focus) && focus != editor) {
                    QWidget* named = focus;
                    while (named && (named->objectName().isEmpty() || named->objectName().startsWith(QLatin1String("qt_")))) {
                        named = named->parentWidget();
                    }
                    if (named) {
                        AppearanceEditor::instance()->editElement(named);
                        return true;
                    }
                }
            }
            break;
        }
        default:
            break;
        }
        return false;
    }

    // ----------------------------------------------------------- PropertyRow

    /** One labelled property: a caption and its control, searchable by keywords. */
    class AppearanceEditor::PropertyRow : public QWidget
    {
    public:
        PropertyRow(const QString& id, const QString& label, QWidget* control, const QString& keywords, QWidget* parent)
            : QWidget(parent)
            , m_id(id)
            , m_haystack((label + QLatin1Char(' ') + keywords).toLower())
        {
            setObjectName(QStringLiteral("appearanceRow_") + id);
            auto* column = new QVBoxLayout(this);
            column->setContentsMargins(0, 0, 0, 0);
            column->setSpacing(4);
            m_label = new QLabel(label, this);
            m_label->setObjectName(QStringLiteral("appearanceRowLabel_") + id);
            column->addWidget(m_label);
            control->setParent(this);
            column->addWidget(control);
            if (control->accessibleName().isEmpty()) {
                control->setAccessibleName(label);
            }
        }

        QString id() const { return m_id; }
        bool matches(const QString& needle, const QRegularExpression* pattern) const
        {
            if (needle.isEmpty()) return true;
            if (pattern) return pattern->match(m_haystack).hasMatch();
            return m_haystack.contains(needle, Qt::CaseInsensitive);
        }
        QLabel* label() const { return m_label; }

    private:
        QString m_id;
        QString m_haystack;
        QLabel* m_label = nullptr;
    };

    // ------------------------------------------------------ AppearanceEditor

    AppearanceEditor* AppearanceEditor::instance()
    {
        static AppearanceEditor* editor = new AppearanceEditor();
        return editor;
    }

    AppearanceEditor::AppearanceEditor(QWidget* parent)
        : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    {
        setObjectName(QStringLiteral("materialAppearanceEditor"));
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, false);
        setFixedWidth(EditorWidth);
        setMaximumHeight(EditorMaxHeight);
        setAccessibleName(tr("Edit appearance"));
        buildUi();
        loadPresets();
        connect(theme(), &Theme::changed, this, [this] { applyTheme(); });
        applyTheme();
    }

    AppearanceEditor::~AppearanceEditor() = default;

    void AppearanceEditor::buildUi()
    {
        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(EditorPadding, EditorPadding, EditorPadding, EditorPadding);
        outer->setSpacing(RowSpacing);

        auto* head = new QHBoxLayout;
        head->setSpacing(8);
        auto* titles = new QVBoxLayout;
        titles->setSpacing(0);
        m_title = new QLabel(tr("Edit appearance"), this);
        m_title->setObjectName(QStringLiteral("appearanceEditorTitle"));
        titles->addWidget(m_title);
        m_subtitle = new QLabel(this);
        m_subtitle->setObjectName(QStringLiteral("appearanceEditorSubtitle"));
        titles->addWidget(m_subtitle);
        head->addLayout(titles, 1);
        m_close = new IconButton(QStringLiteral("close"), this);
        m_close->setObjectName(QStringLiteral("appearanceEditorClose"));
        m_close->setToolTip(tr("Close"));
        m_close->setAccessibleName(tr("Close the appearance editor"));
        connect(m_close, &QAbstractButton::clicked, this, &AppearanceEditor::closeEditor);
        head->addWidget(m_close, 0, Qt::AlignTop);
        outer->addLayout(head);

        // The property inspector's own search, with its anchored builder.
        m_search = new SearchBar(SearchBar::Variant::Surface, this);
        m_search->setObjectName(QStringLiteral("appearanceEditorSearch"));
        m_search->setPlaceholder(tr("Search properties"));
        m_search->setIdentity(QStringLiteral("appearance.editor"), tr("Appearance editor property search"));
        m_search->lineEdit()->setAccessibleName(tr("Search appearance properties"));
        connect(m_search, &SearchBar::textChanged, this, [this] { applyFilter(); });
        connect(m_search, &SearchBar::regexToggled, this, [this] { applyFilter(); });
        outer->addWidget(m_search);

        m_tabs = new SegmentedButton(this);
        m_tabs->setObjectName(QStringLiteral("appearanceEditorTabs"));
        m_tabs->addSegment(QStringLiteral("typography"), tr("Type"));
        m_tabs->addSegment(QStringLiteral("colour"), tr("Colour"));
        m_tabs->addSegment(QStringLiteral("shape"), tr("Shape"));
        m_tabs->addSegment(QStringLiteral("presets"), tr("Presets"));
        outer->addWidget(m_tabs);

        m_pages = new QStackedWidget(this);
        auto* scroll = new QScrollArea(this);
        scroll->setObjectName(QStringLiteral("appearanceEditorScroll"));
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
        scroll->viewport()->setAutoFillBackground(false);
        scroll->setWidget(m_pages);
        m_pages->setAutoFillBackground(false);
        m_pages->addWidget(buildTypographyPage());
        m_pages->addWidget(buildColourPage());
        m_pages->addWidget(buildShapePage());
        m_pages->addWidget(buildPresetsPage());
        outer->addWidget(scroll, 1);
        connect(m_tabs, &SegmentedButton::segmentSelected, this, [this](const QString& id) {
            const QStringList ids{QStringLiteral("typography"), QStringLiteral("colour"), QStringLiteral("shape"), QStringLiteral("presets")};
            m_pages->setCurrentIndex(qMax(0, ids.indexOf(id)));
        });
        m_tabs->setCurrentSegment(QStringLiteral("typography"));

        auto* footer = new QHBoxLayout;
        footer->setSpacing(8);
        m_resetElement = new OutlinedButton(QStringLiteral("restart_alt"), tr("Reset element"), this);
        m_resetElement->setObjectName(QStringLiteral("appearanceEditorResetElement"));
        m_resetElement->setAccessibleName(tr("Reset this element to the shipped look"));
        connect(m_resetElement, &QAbstractButton::clicked, this, [this] {
            ElementOverrides::instance()->reset(m_key);
            loadFromOverride();
        });
        footer->addWidget(m_resetElement, 1);
        m_resetAll = new TextButton(QStringLiteral("delete_sweep"), tr("Reset all"), this);
        m_resetAll->setObjectName(QStringLiteral("appearanceEditorResetAll"));
        m_resetAll->setAccessibleName(tr("Reset every element to the shipped look"));
        connect(m_resetAll, &QAbstractButton::clicked, this, [this] {
            ElementOverrides::instance()->resetAll();
            loadFromOverride();
        });
        footer->addWidget(m_resetAll, 0);
        outer->addLayout(footer);
    }

    AppearanceEditor::PropertyRow* AppearanceEditor::addRow(QWidget* page, const QString& id, const QString& label, QWidget* control, const QString& keywords)
    {
        auto* row = new PropertyRow(id, label, control, keywords, page);
        static_cast<QVBoxLayout*>(page->layout())->addWidget(row);
        m_rows << row;
        return row;
    }

    namespace
    {
        QWidget* newPage(QWidget* parent)
        {
            auto* page = new QWidget(parent);
            page->setAutoFillBackground(false);
            auto* column = new QVBoxLayout(page);
            column->setContentsMargins(0, 4, 4, 4);
            column->setSpacing(RowSpacing);
            return page;
        }

        void finishPage(QWidget* page)
        {
            static_cast<QVBoxLayout*>(page->layout())->addStretch(1);
        }

        QWidget* sliderWithEntry(Slider* slider, QLineEdit* entry, QWidget* parent)
        {
            auto* box = new QWidget(parent);
            auto* row = new QHBoxLayout(box);
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(8);
            row->addWidget(slider, 1);
            entry->setFixedWidth(64);
            row->addWidget(entry, 0);
            return box;
        }
    } // namespace

    QWidget* AppearanceEditor::buildTypographyPage()
    {
        QWidget* page = newPage(m_pages);

        m_fontFamily = new Select;
        m_fontFamily->setObjectName(QStringLiteral("appearanceEditorFontFamily"));
        m_fontFamily->setSearchIdentity(QStringLiteral("appearance.editor.font-family"), tr("Appearance editor font family search"));
        m_fontFamily->setSearchPlaceholder(tr("Search installed fonts"));
        m_fontFamily->addItem(tr("Inherit"), QString());
        const auto families = QFontDatabase::families();
        for (const QString& family : families) {
            m_fontFamily->addItem(family, family);
            m_fontFamily->setItemFont(m_fontFamily->count() - 1, QFont(family));
        }
        connect(m_fontFamily, &Select::currentIndexChanged, this, [this](int) {
            if (m_updating) return;
            const QString family = m_fontFamily->currentData().toString();
            writeOverride([family](ElementOverrides::Override& value) {
                if (family.isEmpty()) value.fontFamily.reset(); else value.fontFamily = family;
            });
        });
        addRow(page, QStringLiteral("fontFamily"), tr("Font family"), m_fontFamily, tr("typeface face installed"));

        m_fontSize = new Slider(Qt::Horizontal);
        m_fontSize->setObjectName(QStringLiteral("appearanceEditorFontSize"));
        m_fontSize->setRange(FontSizeMin, FontSizeMax);
        m_fontSize->setValueLabelSuffix(QStringLiteral(" pt"));
        m_fontSizeEntry = new QLineEdit;
        m_fontSizeEntry->setObjectName(QStringLiteral("appearanceEditorFontSizeEntry"));
        m_fontSizeEntry->setAccessibleName(tr("Font size in points, typed"));
        connect(m_fontSize, &QSlider::valueChanged, this, [this](int value) {
            if (!m_fontSizeEntry->hasFocus()) m_fontSizeEntry->setText(QString::number(value));
            if (m_updating) return;
            writeOverride([value](ElementOverrides::Override& v) { v.fontSize = value; });
        });
        connect(m_fontSizeEntry, &QLineEdit::editingFinished, this, [this] {
            bool ok = false;
            const int value = m_fontSizeEntry->text().trimmed().toInt(&ok);
            if (ok) m_fontSize->setValue(qBound(FontSizeMin, value, FontSizeMax));
        });
        addRow(page, QStringLiteral("fontSize"), tr("Font size"), sliderWithEntry(m_fontSize, m_fontSizeEntry, page), tr("points px scale"));

        m_fontWeight = new SegmentedButton;
        m_fontWeight->setObjectName(QStringLiteral("appearanceEditorFontWeight"));
        m_fontWeight->addSegment(QStringLiteral("300"), tr("Light"));
        m_fontWeight->addSegment(QStringLiteral("400"), tr("Regular"));
        m_fontWeight->addSegment(QStringLiteral("500"), tr("Medium"));
        m_fontWeight->addSegment(QStringLiteral("700"), tr("Bold"));
        connect(m_fontWeight, &SegmentedButton::segmentSelected, this, [this](const QString& id) {
            if (m_updating) return;
            const int weight = id.toInt();
            writeOverride([weight](ElementOverrides::Override& v) { v.fontWeight = weight; });
        });
        addRow(page, QStringLiteral("fontWeight"), tr("Weight"), m_fontWeight, tr("bold light medium regular"));

        auto* styles = new QWidget(page);
        auto* stylesRow = new QHBoxLayout(styles);
        stylesRow->setContentsMargins(0, 0, 0, 0);
        stylesRow->setSpacing(12);
        auto addSwitch = [this, stylesRow, styles](Switch*& target, const QString& name, const QString& caption, auto setter) {
            auto* box = new QWidget(styles);
            auto* column = new QVBoxLayout(box);
            column->setContentsMargins(0, 0, 0, 0);
            column->setSpacing(2);
            auto* label = new QLabel(caption, box);
            label->setAlignment(Qt::AlignHCenter);
            column->addWidget(label);
            target = new Switch(box);
            target->setObjectName(QStringLiteral("appearanceEditor") + name);
            target->setAccessibleName(caption);
            connect(target, &QAbstractButton::toggled, this, [this, setter](bool on) {
                if (m_updating) return;
                writeOverride([on, setter](ElementOverrides::Override& v) { setter(v, on); });
            });
            column->addWidget(target, 0, Qt::AlignHCenter);
            stylesRow->addWidget(box);
        };
        addSwitch(m_italic, QStringLiteral("Italic"), tr("Italic"), [](ElementOverrides::Override& v, bool on) { v.italic = on; });
        addSwitch(m_underline, QStringLiteral("Underline"), tr("Underline"), [](ElementOverrides::Override& v, bool on) { v.underline = on; });
        addSwitch(m_strikeout, QStringLiteral("Strikeout"), tr("Strikethrough"), [](ElementOverrides::Override& v, bool on) { v.strikeout = on; });
        addSwitch(m_overline, QStringLiteral("Overline"), tr("Overline"), [](ElementOverrides::Override& v, bool on) { v.overline = on; });
        addRow(page, QStringLiteral("textStyles"), tr("Text styles"), styles, tr("italic oblique underline strikethrough overline"));

        m_capitalization = new Select;
        m_capitalization->setObjectName(QStringLiteral("appearanceEditorCapitalization"));
        m_capitalization->setSearchIdentity(QStringLiteral("appearance.editor.capitalization"), tr("Appearance editor capitalization search"));
        m_capitalization->addItem(tr("As written"), int(QFont::MixedCase));
        m_capitalization->addItem(tr("ALL CAPS"), int(QFont::AllUppercase));
        m_capitalization->addItem(tr("all lowercase"), int(QFont::AllLowercase));
        m_capitalization->addItem(tr("Small Caps"), int(QFont::SmallCaps));
        m_capitalization->addItem(tr("Capitalize Each Word"), int(QFont::Capitalize));
        connect(m_capitalization, &Select::currentIndexChanged, this, [this](int) {
            if (m_updating) return;
            const int mode = m_capitalization->currentData().toInt();
            writeOverride([mode](ElementOverrides::Override& v) { if (mode == 0) v.capitalization.reset(); else v.capitalization = mode; });
        });
        addRow(page, QStringLiteral("capitalization"), tr("Capitalization"), m_capitalization, tr("uppercase lowercase small caps"));

        m_letterSpacing = new Slider(Qt::Horizontal);
        m_letterSpacing->setObjectName(QStringLiteral("appearanceEditorLetterSpacing"));
        m_letterSpacing->setRange(-2, 12);
        m_letterSpacing->setValueLabelSuffix(QStringLiteral(" px"));
        connect(m_letterSpacing, &QSlider::valueChanged, this, [this](int value) {
            if (m_updating) return;
            writeOverride([value](ElementOverrides::Override& v) { v.letterSpacing = value; });
        });
        addRow(page, QStringLiteral("letterSpacing"), tr("Character spacing"), m_letterSpacing, tr("letter tracking kerning"));

        m_lineHeight = new Slider(Qt::Horizontal);
        m_lineHeight->setObjectName(QStringLiteral("appearanceEditorLineHeight"));
        m_lineHeight->setRange(80, 300);
        m_lineHeight->setValueLabelSuffix(QStringLiteral(" %"));
        connect(m_lineHeight, &QSlider::valueChanged, this, [this](int value) {
            if (m_updating) return;
            writeOverride([value](ElementOverrides::Override& v) { v.lineHeight = value / 100.0; });
        });
        addRow(page, QStringLiteral("lineHeight"), tr("Line height"), m_lineHeight, tr("leading spacing"));

        finishPage(page);
        return page;
    }

    QWidget* AppearanceEditor::buildColourPage()
    {
        QWidget* page = newPage(m_pages);

        m_background = new ColorPicker;
        m_background->setObjectName(QStringLiteral("appearanceEditorBackground"));
        connect(m_background, &ColorPicker::colorChanged, this, [this](const QColor& color) {
            if (m_updating) return;
            m_background->addRecentColor(color);
            m_foreground->setReferenceColor(color);
            writeOverride([color](ElementOverrides::Override& v) { v.background = color; v.rainbow.reset(); });
        });
        connect(m_background, &ColorPicker::rainbowChanged, this, [this](bool on, int level) {
            if (m_updating) return;
            writeOverride([on, level](ElementOverrides::Override& v) {
                if (on) { v.rainbow = true; v.rainbowLevel = level; } else { v.rainbow.reset(); v.rainbowLevel.reset(); }
            });
        });
        addRow(page, QStringLiteral("background"), tr("Background colour"), m_background, tr("fill surface rainbow hex rgb hsl oklch contrast"));

        m_foreground = new ColorPicker;
        m_foreground->setObjectName(QStringLiteral("appearanceEditorForeground"));
        connect(m_foreground, &ColorPicker::colorChanged, this, [this](const QColor& color) {
            if (m_updating) return;
            m_foreground->addRecentColor(color);
            m_background->setReferenceColor(color);
            writeOverride([color](ElementOverrides::Override& v) { v.foreground = color; });
        });
        m_foreground->rainbowSwitch()->setEnabled(false);
        m_foreground->rainbowSwitch()->setToolTip(tr("The rainbow is a background choice; text keeps one colour so it stays readable."));
        addRow(page, QStringLiteral("foreground"), tr("Text colour"), m_foreground, tr("ink text color contrast"));

        finishPage(page);
        return page;
    }

    QWidget* AppearanceEditor::buildShapePage()
    {
        QWidget* page = newPage(m_pages);
        auto slider = [this, page](Slider*& target, const QString& name, const QString& id, const QString& label, int min, int max, const QString& suffix, const QString& keywords, auto setter) {
            target = new Slider(Qt::Horizontal);
            target->setObjectName(QStringLiteral("appearanceEditor") + name);
            target->setRange(min, max);
            target->setValueLabelSuffix(suffix);
            connect(target, &QSlider::valueChanged, this, [this, setter](int value) {
                if (m_updating) return;
                writeOverride([value, setter](ElementOverrides::Override& v) { setter(v, value); });
            });
            addRow(page, id, label, target, keywords);
        };
        slider(m_radius, QStringLiteral("Radius"), QStringLiteral("radius"), tr("Corner radius"), 0, 80, QStringLiteral(" px"), tr("shape rounded corners"),
               [](ElementOverrides::Override& v, int value) { v.radius = value; });
        slider(m_height, QStringLiteral("Height"), QStringLiteral("height"), tr("Element height"), 0, 160, QStringLiteral(" px"), tr("size tall minimum"),
               [](ElementOverrides::Override& v, int value) { if (value == 0) v.height.reset(); else v.height = qMax(24, value); });
        slider(m_spacing, QStringLiteral("Spacing"), QStringLiteral("spacing"), tr("Inner spacing"), 0, 48, QStringLiteral(" px"), tr("padding inset margin"),
               [](ElementOverrides::Override& v, int value) { v.spacing = value; });
        slider(m_borderWidth, QStringLiteral("BorderWidth"), QStringLiteral("borderWidth"), tr("Border width"), 0, 8, QStringLiteral(" px"), tr("outline stroke"),
               [](ElementOverrides::Override& v, int value) { if (value == 0) v.borderWidth.reset(); else v.borderWidth = value; });
        m_borderColor = new ColorPicker;
        m_borderColor->setObjectName(QStringLiteral("appearanceEditorBorderColor"));
        m_borderColor->rainbowSwitch()->setEnabled(false);
        connect(m_borderColor, &ColorPicker::colorChanged, this, [this](const QColor& color) {
            if (m_updating) return;
            writeOverride([color](ElementOverrides::Override& v) { v.borderColor = color; });
        });
        addRow(page, QStringLiteral("borderColor"), tr("Border colour"), m_borderColor, tr("outline stroke colour"));
        slider(m_elevation, QStringLiteral("Elevation"), QStringLiteral("elevation"), tr("Elevation"), 0, 5, QString(), tr("shadow depth level"),
               [](ElementOverrides::Override& v, int value) { if (value == 0) v.elevation.reset(); else v.elevation = value; });
        slider(m_opacity, QStringLiteral("Opacity"), QStringLiteral("opacity"), tr("Opacity"), 0, 100, QStringLiteral(" %"), tr("transparency alpha"),
               [](ElementOverrides::Override& v, int value) { if (value >= 100) v.opacity.reset(); else v.opacity = value / 100.0; });
        finishPage(page);
        return page;
    }

    QWidget* AppearanceEditor::buildPresetsPage()
    {
        QWidget* page = newPage(m_pages);

        auto* clipboardRow = new QWidget(page);
        auto* clip = new QHBoxLayout(clipboardRow);
        clip->setContentsMargins(0, 0, 0, 0);
        clip->setSpacing(8);
        m_copyStyle = new OutlinedButton(QStringLiteral("content_copy"), tr("Copy style"), clipboardRow);
        m_copyStyle->setObjectName(QStringLiteral("appearanceEditorCopyStyle"));
        connect(m_copyStyle, &QAbstractButton::clicked, this, [this] {
            m_clipboard = currentOverride();
            m_pasteStyle->setEnabled(true);
            m_presetStatus->setText(tr("Style of %1 copied.").arg(m_key));
        });
        clip->addWidget(m_copyStyle, 1);
        m_pasteStyle = new OutlinedButton(QStringLiteral("content_paste"), tr("Paste style"), clipboardRow);
        m_pasteStyle->setObjectName(QStringLiteral("appearanceEditorPasteStyle"));
        m_pasteStyle->setEnabled(false);
        connect(m_pasteStyle, &QAbstractButton::clicked, this, [this] {
            if (!m_clipboard) return;
            ElementOverrides::instance()->set(m_key, *m_clipboard);
            loadFromOverride();
            m_presetStatus->setText(tr("Style pasted onto %1.").arg(m_key));
        });
        clip->addWidget(m_pasteStyle, 1);
        addRow(page, QStringLiteral("clipboard"), tr("Copy and paste between elements"), clipboardRow, tr("clipboard duplicate"));

        auto* saveRow = new QWidget(page);
        auto* save = new QHBoxLayout(saveRow);
        save->setContentsMargins(0, 0, 0, 0);
        save->setSpacing(8);
        m_presetName = new QLineEdit(saveRow);
        m_presetName->setObjectName(QStringLiteral("appearanceEditorPresetName"));
        m_presetName->setPlaceholderText(tr("Preset name"));
        m_presetName->setAccessibleName(tr("Name for the saved preset"));
        save->addWidget(m_presetName, 1);
        m_savePreset = new FilledButton(QStringLiteral("save"), tr("Save"), saveRow);
        m_savePreset->setObjectName(QStringLiteral("appearanceEditorSavePreset"));
        connect(m_savePreset, &QAbstractButton::clicked, this, [this] {
            const QString name = m_presetName->text().trimmed().left(60);
            if (name.isEmpty()) {
                m_presetStatus->setText(tr("Give the preset a name first."));
                return;
            }
            const auto value = currentOverride();
            if (value.isEmpty()) {
                m_presetStatus->setText(tr("This element has no customisation to save yet."));
                return;
            }
            m_presets.insert(name, value);
            savePresets();
            refreshPresetSelect();
            m_presetSelect->setCurrentIndex(m_presetSelect->findData(name));
            m_presetStatus->setText(tr("Preset %1 saved.").arg(name));
        });
        save->addWidget(m_savePreset, 0);
        addRow(page, QStringLiteral("savePreset"), tr("Save the current style as a preset"), saveRow, tr("named theme"));

        auto* applyRow = new QWidget(page);
        auto* apply = new QHBoxLayout(applyRow);
        apply->setContentsMargins(0, 0, 0, 0);
        apply->setSpacing(8);
        m_presetSelect = new Select(applyRow);
        m_presetSelect->setObjectName(QStringLiteral("appearanceEditorPresetSelect"));
        m_presetSelect->setAccessibleName(tr("Saved presets"));
        m_presetSelect->setSearchIdentity(QStringLiteral("appearance.editor.presets"), tr("Appearance preset search"));
        apply->addWidget(m_presetSelect, 1);
        m_applyPreset = new OutlinedButton(QStringLiteral("check"), tr("Apply"), applyRow);
        m_applyPreset->setObjectName(QStringLiteral("appearanceEditorApplyPreset"));
        connect(m_applyPreset, &QAbstractButton::clicked, this, [this] {
            const QString name = m_presetSelect->currentData().toString();
            if (!m_presets.contains(name)) return;
            ElementOverrides::instance()->set(m_key, m_presets.value(name));
            loadFromOverride();
            m_presetStatus->setText(tr("Preset %1 applied to %2.").arg(name, m_key));
        });
        apply->addWidget(m_applyPreset, 0);
        m_deletePreset = new OutlinedButton(QStringLiteral("delete"), tr("Delete"), applyRow);
        m_deletePreset->setObjectName(QStringLiteral("appearanceEditorDeletePreset"));
        connect(m_deletePreset, &QAbstractButton::clicked, this, [this] {
            const QString name = m_presetSelect->currentData().toString();
            if (m_presets.remove(name)) {
                savePresets();
                refreshPresetSelect();
                m_presetStatus->setText(tr("Preset %1 deleted.").arg(name));
            }
        });
        apply->addWidget(m_deletePreset, 0);
        addRow(page, QStringLiteral("applyPreset"), tr("Apply a saved preset"), applyRow, tr("named theme load"));

        auto* fileRow = new QWidget(page);
        auto* file = new QHBoxLayout(fileRow);
        file->setContentsMargins(0, 0, 0, 0);
        file->setSpacing(8);
        m_exportPresets = new OutlinedButton(QStringLiteral("download"), tr("Export JSON"), fileRow);
        m_exportPresets->setObjectName(QStringLiteral("appearanceEditorExportPresets"));
        connect(m_exportPresets, &QAbstractButton::clicked, this, [this] {
            QApplication::clipboard()->setText(exportPresets());
            m_presetStatus->setText(tr("Every preset copied to the clipboard as JSON (%n preset(s)).", "", m_presets.size()));
        });
        file->addWidget(m_exportPresets, 1);
        m_importPresets = new OutlinedButton(QStringLiteral("upload"), tr("Import JSON"), fileRow);
        m_importPresets->setObjectName(QStringLiteral("appearanceEditorImportPresets"));
        connect(m_importPresets, &QAbstractButton::clicked, this, [this] {
            QString error;
            if (importPresets(QApplication::clipboard()->text(), &error)) {
                m_presetStatus->setText(tr("Presets imported from the clipboard (%n preset(s)).", "", m_presets.size()));
            } else {
                m_presetStatus->setText(tr("Nothing imported: %1").arg(error));
            }
        });
        file->addWidget(m_importPresets, 1);
        addRow(page, QStringLiteral("presetFile"), tr("Export and import presets (clipboard JSON)"), fileRow, tr("share backup file"));

        m_presetStatus = new QLabel(page);
        m_presetStatus->setObjectName(QStringLiteral("appearanceEditorPresetStatus"));
        m_presetStatus->setWordWrap(true);
        static_cast<QVBoxLayout*>(page->layout())->addWidget(m_presetStatus);
        finishPage(page);
        return page;
    }

    // ------------------------------------------------------------ behaviour

    void AppearanceEditor::editElement(QWidget* target, const QString& key)
    {
        if (!target) {
            return;
        }
        if (m_target && m_target->window()) {
            m_target->window()->removeEventFilter(this);
        }
        m_target = target;
        m_key = key.isEmpty() ? AppearanceApplier::keyFor(target) : key;
        if (m_key.isEmpty()) {
            return;
        }
        m_returnFocus = QApplication::focusWidget();
        m_subtitle->setText(tr("%1 · %2").arg(m_key, QString::fromLatin1(target->metaObject()->className())));
        setAccessibleName(tr("Edit appearance of %1").arg(m_key));
        loadFromOverride();
        target->window()->installEventFilter(this);
        show();
        placeBesideTarget();
        raise();
        activateWindow();
        m_search->lineEdit()->setFocus();
        emit targetChanged(m_key);
    }

    QString AppearanceEditor::currentKey() const
    {
        return m_key;
    }

    QWidget* AppearanceEditor::currentTarget() const
    {
        return m_target;
    }

    void AppearanceEditor::setCurrentTab(const QString& id)
    {
        m_tabs->setCurrentSegment(id);
        const QStringList ids{QStringLiteral("typography"), QStringLiteral("colour"), QStringLiteral("shape"), QStringLiteral("presets")};
        m_pages->setCurrentIndex(qMax(0, ids.indexOf(id)));
    }

    QString AppearanceEditor::currentTab() const
    {
        return m_tabs->currentSegment();
    }

    ElementOverrides::Override AppearanceEditor::currentOverride() const
    {
        return ElementOverrides::instance()->get(m_key);
    }

    void AppearanceEditor::writeOverride(const std::function<void(ElementOverrides::Override&)>& change)
    {
        if (m_key.isEmpty()) {
            return;
        }
        auto value = ElementOverrides::instance()->get(m_key);
        change(value);
        ElementOverrides::instance()->set(m_key, value);
        if (value.isEmpty()) {
            AppearanceApplier::instance()->apply(m_key);
        }
        m_resetElement->setEnabled(!value.isEmpty());
    }

    void AppearanceEditor::loadFromOverride()
    {
        m_updating = true;
        const auto value = currentOverride();
        const QFont base = m_target ? m_target->property(BaseFontProperty).isValid() ? m_target->property(BaseFontProperty).value<QFont>() : m_target->font()
                                    : theme()->font(TypeRole::BodyMedium);
        const int familyIndex = value.fontFamily ? m_fontFamily->findData(*value.fontFamily) : 0;
        m_fontFamily->setCurrentIndex(qMax(0, familyIndex));
        m_fontSize->setValue(value.fontSize.value_or(qMax(FontSizeMin, base.pointSize() > 0 ? base.pointSize() : 10)));
        m_fontSizeEntry->setText(QString::number(m_fontSize->value()));
        const int weight = value.fontWeight.value_or(base.weight());
        m_fontWeight->setCurrentSegment(weight <= 300 ? QStringLiteral("300") : weight < 500 ? QStringLiteral("400") : weight < 700 ? QStringLiteral("500") : QStringLiteral("700"));
        m_italic->setChecked(value.italic.value_or(base.italic()));
        m_underline->setChecked(value.underline.value_or(base.underline()));
        m_strikeout->setChecked(value.strikeout.value_or(base.strikeOut()));
        m_overline->setChecked(value.overline.value_or(base.overline()));
        m_capitalization->setCurrentIndex(qMax(0, m_capitalization->findData(value.capitalization.value_or(int(base.capitalization())))));
        m_letterSpacing->setValue(qRound(value.letterSpacing.value_or(base.letterSpacing())));
        m_lineHeight->setValue(qRound(value.lineHeight.value_or(1.0) * 100));
        const QColor background = value.background.value_or(theme()->color(Role::SurfaceContainer));
        const QColor foreground = value.foreground.value_or(theme()->color(Role::OnSurface));
        m_background->setColor(background);
        m_background->setReferenceColor(foreground);
        m_background->setRainbow(value.rainbow.value_or(false));
        m_background->setRainbowLevel(value.rainbowLevel.value_or(3));
        m_foreground->setColor(foreground);
        m_foreground->setReferenceColor(background);
        m_radius->setValue(value.radius.value_or(Shape::Medium));
        m_height->setValue(value.height.value_or(0));
        m_spacing->setValue(value.spacing.value_or(0));
        m_borderWidth->setValue(value.borderWidth.value_or(0));
        m_borderColor->setColor(value.borderColor.value_or(theme()->color(Role::Outline)));
        m_elevation->setValue(value.elevation.value_or(0));
        m_opacity->setValue(qRound(value.opacity.value_or(1.0) * 100));
        m_resetElement->setEnabled(!value.isEmpty());
        m_updating = false;
    }

    void AppearanceEditor::applyFilter()
    {
        const QString needle = m_search->text().trimmed();
        QRegularExpression pattern;
        const QRegularExpression* use = nullptr;
        if (m_search->isRegexEnabled() && !needle.isEmpty()) {
            pattern = QRegularExpression(needle, QRegularExpression::CaseInsensitiveOption);
            if (!pattern.isValid()) {
                return; // an unparsable pattern changes nothing
            }
            use = &pattern;
        }
        int shown = 0;
        for (PropertyRow* row : std::as_const(m_rows)) {
            const bool visible = row->matches(needle, use);
            row->setVisible(visible);
            shown += visible ? 1 : 0;
        }
        m_search->lineEdit()->setAccessibleDescription(shown == 0 ? tr("No properties match") : tr("%n propert(y/ies) shown", "", shown));
    }

    QStringList AppearanceEditor::visiblePropertyRows() const
    {
        QStringList ids;
        for (PropertyRow* row : m_rows) {
            if (!row->isHidden()) {
                ids << row->id();
            }
        }
        return ids;
    }

    void AppearanceEditor::placeBesideTarget()
    {
        if (!m_target) {
            return;
        }
        const QRect anchor(m_target->mapToGlobal(QPoint(0, 0)), m_target->size());
        QScreen* screen = m_target->screen() ? m_target->screen() : QApplication::primaryScreen();
        const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
        const QSize mine = size();
        QPoint at(anchor.right() + 12, anchor.top());
        if (at.x() + mine.width() > available.right()) {
            at.setX(anchor.left() - 12 - mine.width());
        }
        if (at.x() < available.left()) {
            at.setX(qBound(available.left(), anchor.left(), available.right() - mine.width()));
            at.setY(anchor.bottom() + 12);
        }
        at.setY(qBound(available.top(), at.y(), qMax(available.top(), available.bottom() - mine.height())));
        move(at);
    }

    void AppearanceEditor::closeEditor()
    {
        hide();
        if (m_target && m_target->window()) {
            m_target->window()->removeEventFilter(this);
        }
        if (m_returnFocus) {
            m_returnFocus->setFocus(Qt::OtherFocusReason);
        } else if (m_target) {
            m_target->setFocus(Qt::OtherFocusReason);
        }
    }

    bool AppearanceEditor::eventFilter(QObject* watched, QEvent* event)
    {
        if (m_target && watched == m_target->window() && (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
            placeBesideTarget();
        }
        return QWidget::eventFilter(watched, event);
    }

    void AppearanceEditor::keyPressEvent(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_Escape) {
            closeEditor();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void AppearanceEditor::showEvent(QShowEvent* event)
    {
        QWidget::showEvent(event);
        applyFilter();
    }

    void AppearanceEditor::hideEvent(QHideEvent* event)
    {
        QWidget::hideEvent(event);
    }

    void AppearanceEditor::paintEvent(QPaintEvent*)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        paintSurface(&painter, rect(), EditorRadius, theme()->color(Role::SurfaceContainerLow), theme()->color(Role::OutlineVariant));
    }

    void AppearanceEditor::applyTheme()
    {
        m_title->setFont(theme()->font(TypeRole::TitleMedium));
        m_subtitle->setFont(theme()->font(TypeRole::Mono));
        QPalette muted = m_subtitle->palette();
        muted.setColor(QPalette::WindowText, theme()->color(Role::OnSurfaceVariant));
        m_subtitle->setPalette(muted);
        for (PropertyRow* row : std::as_const(m_rows)) {
            row->label()->setFont(theme()->font(TypeRole::LabelMedium));
            row->label()->setPalette(muted);
        }
        if (m_presetStatus) {
            m_presetStatus->setFont(theme()->font(TypeRole::BodySmall));
        }
        update();
    }

    // -------------------------------------------------------------- presets

    void AppearanceEditor::loadPresets()
    {
        m_presets.clear();
        QString error;
        importPresets(config()->get(Config::GUI_AppearancePresets).toString(), &error);
    }

    void AppearanceEditor::savePresets() const
    {
        config()->set(Config::GUI_AppearancePresets, exportPresets());
    }

    QString AppearanceEditor::exportPresets() const
    {
        QJsonObject root;
        root.insert(QStringLiteral("schemaVersion"), 1);
        QJsonObject presets;
        for (auto it = m_presets.cbegin(); it != m_presets.cend(); ++it) {
            presets.insert(it.key(), it.value().toJson());
        }
        root.insert(QStringLiteral("presets"), presets);
        return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }

    bool AppearanceEditor::importPresets(const QString& json, QString* error)
    {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (error) *error = tr("the text is not a JSON object");
            return false;
        }
        const QJsonObject root = document.object();
        QJsonObject presets = root.value(QStringLiteral("presets")).toObject();
        if (root.contains(QStringLiteral("schemaVersion")) && root.value(QStringLiteral("schemaVersion")).toInt() != 1) {
            if (error) *error = tr("unsupported schema version");
            return false;
        }
        if (!root.contains(QStringLiteral("presets"))) {
            // A bare map of name -> override is accepted for hand-written files.
            presets = root;
        }
        if (presets.size() > 200) {
            if (error) *error = tr("more than 200 presets");
            return false;
        }
        int imported = 0;
        for (auto it = presets.begin(); it != presets.end(); ++it) {
            if (!it.value().isObject() || it.key().trimmed().isEmpty() || it.key().size() > 60) continue;
            const auto value = ElementOverrides::Override::fromJson(it.value().toObject());
            if (value.isEmpty()) continue;
            m_presets.insert(it.key().trimmed(), value);
            ++imported;
        }
        if (imported == 0 && !presets.isEmpty()) {
            if (error) *error = tr("no valid presets inside");
            return false;
        }
        refreshPresetSelect();
        return true;
    }

    QStringList AppearanceEditor::presetNames() const
    {
        QStringList names = m_presets.keys();
        names.sort(Qt::CaseInsensitive);
        return names;
    }

    void AppearanceEditor::refreshPresetSelect()
    {
        if (!m_presetSelect) {
            return;
        }
        const QString current = m_presetSelect->currentData().toString();
        m_presetSelect->clear();
        const QStringList names = presetNames();
        for (const QString& name : names) {
            m_presetSelect->addItem(name, name);
        }
        const int index = m_presetSelect->findData(current);
        if (index >= 0) m_presetSelect->setCurrentIndex(index);
        m_applyPreset->setEnabled(!names.isEmpty());
        m_deletePreset->setEnabled(!names.isEmpty());
    }

    // ------------------------------------------------------------ accessors

    SearchBar* AppearanceEditor::propertySearch() const { return m_search; }
    ColorPicker* AppearanceEditor::backgroundPicker() const { return m_background; }
    ColorPicker* AppearanceEditor::foregroundPicker() const { return m_foreground; }
    Select* AppearanceEditor::fontFamily() const { return m_fontFamily; }
    Slider* AppearanceEditor::fontSize() const { return m_fontSize; }
    QLineEdit* AppearanceEditor::fontSizeEntry() const { return m_fontSizeEntry; }
    SegmentedButton* AppearanceEditor::fontWeight() const { return m_fontWeight; }
    Switch* AppearanceEditor::italic() const { return m_italic; }
    Switch* AppearanceEditor::underline() const { return m_underline; }
    Switch* AppearanceEditor::strikeout() const { return m_strikeout; }
    Switch* AppearanceEditor::overline() const { return m_overline; }
    Select* AppearanceEditor::capitalization() const { return m_capitalization; }
    Slider* AppearanceEditor::letterSpacing() const { return m_letterSpacing; }
    Slider* AppearanceEditor::lineHeight() const { return m_lineHeight; }
    Slider* AppearanceEditor::radius() const { return m_radius; }
    Slider* AppearanceEditor::height() const { return m_height; }
    Slider* AppearanceEditor::spacing() const { return m_spacing; }
    Slider* AppearanceEditor::borderWidth() const { return m_borderWidth; }
    Slider* AppearanceEditor::elevation() const { return m_elevation; }
    Slider* AppearanceEditor::opacity() const { return m_opacity; }
    QAbstractButton* AppearanceEditor::resetElementButton() const { return m_resetElement; }
    QAbstractButton* AppearanceEditor::resetAllButton() const { return m_resetAll; }
    QAbstractButton* AppearanceEditor::copyStyleButton() const { return m_copyStyle; }
    QAbstractButton* AppearanceEditor::pasteStyleButton() const { return m_pasteStyle; }
    QAbstractButton* AppearanceEditor::savePresetButton() const { return m_savePreset; }
    Select* AppearanceEditor::presetSelect() const { return m_presetSelect; }
    QAbstractButton* AppearanceEditor::applyPresetButton() const { return m_applyPreset; }
    QLineEdit* AppearanceEditor::presetName() const { return m_presetName; }

} // namespace Material
