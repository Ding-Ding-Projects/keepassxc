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

#include "MaterialSearchBar.h"

#include "MaterialButtons.h"
#include "MaterialChip.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"
#include "MaterialSearchRegistry.h"
#include "MaterialVoice.h"

#include <QApplication>
#include <QAction>
#include <QEvent>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>

namespace Material
{
    namespace
    {
        constexpr int FocusRingWidth = 2;
        constexpr int ProminentWidthHint = 420;
        constexpr int SurfaceWidthHint = 340;

        int barHeight(SearchBar::Variant variant)
        {
            return variant == SearchBar::Variant::Prominent ? Layout::SearchBarHeight : Layout::SurfaceSearchHeight;
        }

        /** Distance from the left edge to the search glyph. */
        int leadingPadding(SearchBar::Variant variant)
        {
            return variant == SearchBar::Variant::Prominent ? 18 : 16;
        }

        /**
         * Gap between the glyph, the input and the trailing controls. The design
         * leaves the same gap again to the right of the last control, so this is
         * also the trailing padding.
         */
        int contentSpacing(SearchBar::Variant variant)
        {
            return variant == SearchBar::Variant::Prominent ? 8 : 10;
        }

        /** Size of the painted leading search glyph. */
        int glyphSize(SearchBar::Variant variant)
        {
            return variant == SearchBar::Variant::Prominent ? 22 : 20;
        }

        /** Diameter of the trailing round controls. */
        int controlSize(SearchBar::Variant variant)
        {
            return variant == SearchBar::Variant::Prominent ? 36 : 32;
        }

        TypeRole inputType(SearchBar::Variant variant)
        {
            return variant == SearchBar::Variant::Prominent ? TypeRole::BodyLarge : TypeRole::BodyMedium;
        }

        /**
         * The builder button's glyph. The prominent bar already says "regex" with
         * its toggle chip, so its button carries the neutral `build` wrench; the
         * surface bars have only this one control and name it outright.
         */
        QString builderSymbol(SearchBar::Variant variant)
        {
            return variant == SearchBar::Variant::Prominent ? QStringLiteral("build")
                                                            : QStringLiteral("regular_expression");
        }

        int builderSymbolSize(SearchBar::Variant variant)
        {
            return variant == SearchBar::Variant::Prominent ? 20 : 18;
        }

        /**
         * Only the 52px vault bar offers the Regex toggle; every 44px surface bar
         * in the design carries the builder button alone.
         */
        bool showsRegexChip(SearchBar::Variant variant)
        {
            return variant == SearchBar::Variant::Prominent;
        }

        // Both variants stand off their card on the same fill; only the height,
        // the padding and the trailing controls differ.
        constexpr Role FillRole = Role::SurfaceContainerHigh;
    } // namespace

    SearchBar::SearchBar(QWidget* parent)
        : SearchBar(Variant::Prominent, parent)
    {
    }

    SearchBar::SearchBar(Variant variant, QWidget* parent)
        : QWidget(parent)
        , m_variant(variant)
    {
        auto* layout = new QHBoxLayout(this);

        m_lineEdit = new QLineEdit(this);
        m_lineEdit->setFrame(false);
        m_lineEdit->setClearButtonEnabled(true);
        m_lineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
        layout->addWidget(m_lineEdit, 1);

        m_regexChip = new Chip(QStringLiteral("regular_expression"), tr("Regex"), Chip::Kind::Filter, this);
        m_regexChip->setToolTip(tr("Use regular expression"));
        layout->addWidget(m_regexChip, 0);

        m_builderButton = new IconButton(this);
        m_builderButton->setToolTip(tr("Open the regular expression builder"));
        layout->addWidget(m_builderButton, 0);

        connect(m_lineEdit, &QLineEdit::textChanged, this, &SearchBar::textChanged);
        connect(m_lineEdit, &QLineEdit::returnPressed, this, &SearchBar::returnPressed);
        connect(m_regexChip, &Chip::toggled, this, &SearchBar::regexToggled);
        connect(m_builderButton, &IconButton::clicked, this, [this] {
            SearchRegistry::instance()->setCurrent(this);
            emit builderRequested();
        });
        connect(theme(), &Theme::changed, this, &SearchBar::applyTheme);
        connect(Voice::notifier(), &Voice::Notifier::changed, this, [this] {
            if (!m_searchLabelKey.isEmpty()) setIdentity(m_searchId, m_searchLabel);
            if (!m_placeholderKey.isEmpty()) setPlaceholder(QString());
        });

        // The focus ring belongs to the pill, so repaint whenever the input
        // gains or loses focus.
        connect(qApp, &QApplication::focusChanged, this, [this](QWidget* previous, QWidget* current) {
            if (previous == m_lineEdit || current == m_lineEdit) {
                update();
            }
            if (current == m_lineEdit) {
                SearchRegistry::instance()->setCurrent(this);
            }
        });

        setFocusProxy(m_lineEdit);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        applyTheme();
    }

    SearchBar::~SearchBar()
    {
        SearchRegistry::instance()->unregisterBar(this);
    }

    SearchBar::Variant SearchBar::variant() const
    {
        return m_variant;
    }

    void SearchBar::setVariant(Variant variant)
    {
        if (variant == m_variant) {
            return;
        }
        m_variant = variant;
        applyTheme();
        updateGeometry();
    }

    QString SearchBar::text() const
    {
        return m_lineEdit->text();
    }

    void SearchBar::setText(const QString& text)
    {
        m_lineEdit->setText(text);
    }

    void SearchBar::clear()
    {
        m_lineEdit->clear();
    }

    void SearchBar::setPlaceholder(const QString& placeholder)
    {
        m_placeholder = m_placeholderKey.isEmpty() ? placeholder : Voice::say(m_placeholderKey);
        applyPlaceholder();
    }

    QString SearchBar::placeholder() const
    {
        return m_placeholder;
    }

    void SearchBar::applyPlaceholder()
    {
        // A placeholder longer than the field would be cut mid-glyph; the
        // full text stays available through placeholder() and the accessible
        // description, and the box shows as much as fits with an ellipsis.
        const QMargins margins = m_lineEdit->textMargins();
        const int available = m_lineEdit->width() - margins.left() - margins.right() - 8;
        const QString shown = available > 0 ? m_lineEdit->fontMetrics().elidedText(m_placeholder, Qt::ElideRight, available)
                                            : m_placeholder;
        m_lineEdit->setPlaceholderText(shown);
        m_lineEdit->setAccessibleDescription(m_placeholder);
    }

    bool SearchBar::setIdentity(const QString& id, const QString& label)
    {
        if (id.isEmpty() || label.isEmpty()) return false;
        const bool identityChanged = m_searchId != id;
        if (identityChanged) SearchRegistry::instance()->unregisterBar(this);
        m_searchId = id;
        m_searchLabel = m_searchLabelKey.isEmpty() ? label : Voice::say(m_searchLabelKey);
        setAccessibleName(m_searchLabel);
        m_lineEdit->setAccessibleName(m_searchLabel);
        if (!identityChanged) return SearchRegistry::instance()->bar(id) == this;
        return SearchRegistry::instance()->registerBar(this);
    }

    QString SearchBar::searchId() const { return m_searchId; }
    void SearchBar::setCopyKeys(const QString& placeholderKey, const QString& labelKey)
    {
        m_placeholderKey = placeholderKey;
        m_searchLabelKey = labelKey;
        setPlaceholder(m_placeholder);
        if (!m_searchId.isEmpty()) setIdentity(m_searchId, m_searchLabel);
    }
    QWidget* SearchBar::guidanceWidget(const QString& key)
    {
        if (m_guidanceWidget) return m_guidanceWidget;
        m_guidanceKey = key;
        auto* host = new QWidget(parentWidget());
        host->setObjectName(m_searchId + QStringLiteral(".guidance"));
        m_guidanceWidget = host;
        auto* layout = new QVBoxLayout(host);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* entry = new TextButton(host);
        entry->setObjectName(QStringLiteral("searchGuidanceEntry"));
        entry->setCheckable(true);
        layout->addWidget(entry);
        auto* panel = new QWidget(host);
        panel->setObjectName(QStringLiteral("searchGuidancePanel"));
        auto* steps = new QVBoxLayout(panel);
        auto* explanation = new QLabel(panel);
        explanation->setObjectName(QStringLiteral("searchGuidanceExplanation"));
        explanation->setWordWrap(true);
        explanation->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
        auto* scroll = new QScrollArea(panel);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setMinimumHeight(100);
        scroll->setMaximumHeight(180);
        scroll->setWidget(explanation);
        steps->addWidget(scroll);
        auto* done = new TextButton(panel);
        done->setObjectName(QStringLiteral("searchGuidanceDone"));
        steps->addWidget(done);
        layout->addWidget(panel);
        panel->hide();
        auto refresh = [this, entry, explanation, done, key] {
            entry->setText(Voice::say(QStringLiteral("search.guidance.open")) + QStringLiteral(": ") + searchLabel());
            entry->setAccessibleName(entry->text());
            explanation->setText(Voice::say(key)
                                 + (m_guidanceControl.isEmpty() ? QString() : QStringLiteral("\n\n")
                                    + Voice::say(QStringLiteral("search.guidance.control.") + m_guidanceControl)));
            explanation->setAccessibleName(explanation->text());
            done->setText(Voice::say(QStringLiteral("search.guidance.done")));
            done->setAccessibleName(done->text());
            m_lineEdit->setToolTip(Voice::say(QStringLiteral("search.guidance.control.input")));
            m_regexChip->setToolTip(Voice::say(QStringLiteral("search.guidance.control.regex")));
            m_builderButton->setToolTip(Voice::say(QStringLiteral("search.guidance.control.builder")));
            entry->setToolTip(Voice::say(QStringLiteral("search.guidance.control.open")));
            done->setToolTip(Voice::say(QStringLiteral("search.guidance.control.done")));
        };
        refresh();
        const QPointer<SearchBar> owner(this);
        connect(Voice::notifier(), &Voice::Notifier::changed, host, [owner, refresh] {
            if (owner) refresh();
        });
        connect(entry, &QAbstractButton::toggled, this, [this, panel, done](bool open) {
            panel->setVisible(open);
            if (open) done->setFocus(Qt::OtherFocusReason);
            else m_lineEdit->setFocus(Qt::OtherFocusReason);
        });
        connect(done, &QAbstractButton::clicked, entry, [entry] { entry->setChecked(false); });
        // Actions belong to the field, so the command palette's live QAction
        // inventory discovers help for the exact category and child control.
        const QStringList controls{QStringLiteral("input"), QStringLiteral("clear"), QStringLiteral("regex"),
                                   QStringLiteral("builder"), QStringLiteral("open"), QStringLiteral("done")};
        for (const auto& control : controls) {
            auto* help = new QAction(this);
            help->setProperty("materialSearchHelp", true);
            help->setObjectName(m_searchId + QStringLiteral(".help.") + control);
            auto refreshHelp = [this, help, control] {
                help->setText(searchLabel() + QStringLiteral(": ")
                              + Voice::say(QStringLiteral("search.guidance.control.") + control + QStringLiteral(".title")));
                help->setToolTip(Voice::say(QStringLiteral("search.guidance.control.") + control));
                updateHelpAvailability();
            };
            refreshHelp();
            connect(Voice::notifier(), &Voice::Notifier::changed, help, refreshHelp);
            connect(help, &QAction::triggered, this, [this, entry, refresh, control] {
                if (!m_guidanceWidget || !isVisible()) return;
                m_guidanceControl = control;
                refresh();
                entry->setChecked(true);
                m_guidanceWidget->show();
            });
        }
        connect(host, &QObject::destroyed, this, [this] { updateHelpAvailability(); });
        connect(this, &QObject::destroyed, host, [host, panel] {
            panel->hide();
            host->setEnabled(false);
        });
        return host;
    }

    QString SearchBar::guidanceKey() const { return m_guidanceKey; }

    void SearchBar::updateHelpAvailability()
    {
        for (auto* action : findChildren<QAction*>(QString(), Qt::FindDirectChildrenOnly)) {
            if (!action->property("materialSearchHelp").toBool()) continue;
            const bool available = isVisible() && m_guidanceWidget;
            action->setEnabled(available);
            action->setStatusTip(available ? QString() : Voice::say(QStringLiteral("search.guidance.control.unavailable")));
        }
    }

    bool SearchBar::event(QEvent* event)
    {
        const bool handled = QWidget::event(event);
        if (event->type() == QEvent::Show || event->type() == QEvent::Hide) updateHelpAvailability();
        return handled;
    }
    QString SearchBar::searchLabel() const { return m_searchLabel; }
    QString SearchBar::regexFlags() const { return m_regexFlags; }
    void SearchBar::setRegexFlags(const QString& flags)
    {
        if (m_regexFlags == flags) return;
        m_regexFlags = flags;
        // Existing search owners read flags when their textChanged consumer runs.
        // Reapply an unchanged pattern as well, without changing the line edit.
        emit textChanged(text());
        emit regexFlagsChanged(flags);
    }

    bool SearchBar::isRegexEnabled() const
    {
        return m_regexChip->isChecked();
    }

    void SearchBar::setRegexEnabled(bool enabled)
    {
        m_regexChip->setChecked(enabled);
    }

    bool SearchBar::showRegexControls() const
    {
        return m_showRegexControls;
    }

    void SearchBar::setShowRegexControls(bool show)
    {
        if (show == m_showRegexControls) {
            return;
        }
        m_showRegexControls = show;
        applyTheme();
        updateGeometry();
    }

    QLineEdit* SearchBar::lineEdit() const
    {
        return m_lineEdit;
    }

    QSize SearchBar::sizeHint() const
    {
        const int width = m_variant == Variant::Prominent ? ProminentWidthHint : SurfaceWidthHint;
        return {width, barHeight(m_variant)};
    }

    QSize SearchBar::minimumSizeHint() const
    {
        int width = 2 * leadingPadding(m_variant) + glyphSize(m_variant) + 2 * contentSpacing(m_variant) + 60;
        if (m_showRegexControls) {
            width += controlSize(m_variant) + contentSpacing(m_variant);
            if (showsRegexChip(m_variant)) {
                width += m_regexChip->sizeHint().width() + contentSpacing(m_variant);
            }
        }
        return {width, barHeight(m_variant)};
    }

    void SearchBar::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        paintSurface(&painter, rect(), Shape::Full, theme()->color(FillRole));

        if (m_lineEdit->hasFocus()) {
            // Inset by half the pen so the ring stays inside the pill.
            const qreal inset = FocusRingWidth / 2.0;
            painter.setPen(QPen(theme()->color(Role::Primary), FocusRingWidth));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(roundedPath(QRectF(rect()).adjusted(inset, inset, -inset, -inset), Shape::Full));
        }

        const int glyphExtent = glyphSize(m_variant);
        const QPixmap glyph =
            Icons::pixmap(QStringLiteral("search"), glyphExtent, theme()->color(Role::OnSurfaceVariant));
        painter.drawPixmap(QRect(leadingPadding(m_variant), (height() - glyphExtent) / 2, glyphExtent, glyphExtent),
                           glyph);
    }

    void SearchBar::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        applyPlaceholder();
        // The pill radius and the glyph both follow the height.
        update();
    }

    void SearchBar::applyTheme()
    {
        const int leading = leadingPadding(m_variant);
        const int spacing = contentSpacing(m_variant);
        // The design leaves the same gap to the right of the last control as it
        // does between them; with nothing there the field keeps its own padding.
        const int trailing = m_showRegexControls ? spacing : leading;

        setFixedHeight(barHeight(m_variant));

        auto* box = qobject_cast<QHBoxLayout*>(layout());
        box->setSpacing(spacing);
        // The glyph is painted, not laid out; the input starts after it.
        box->setContentsMargins(leading + glyphSize(m_variant) + spacing, 0, trailing, 0);

        m_regexChip->setFixedHeight(controlSize(m_variant));
        m_builderButton->setDiameter(controlSize(m_variant));
        m_builderButton->setSymbol(builderSymbol(m_variant));
        m_builderButton->setSymbolSize(builderSymbolSize(m_variant));

        m_lineEdit->setFont(theme()->font(inputType(m_variant)));
        m_lineEdit->setStyleSheet(QStringLiteral("QLineEdit { background: transparent; border: none; padding: 0; "
                                                 "selection-background-color: %1; color: %2; }")
                                      .arg(theme()->hex(Role::Primary), theme()->hex(Role::OnSurface)));

        QPalette palette = m_lineEdit->palette();
        palette.setColor(QPalette::Text, theme()->color(Role::OnSurface));
        palette.setColor(QPalette::PlaceholderText, theme()->color(Role::OnSurfaceVariant));
        m_lineEdit->setPalette(palette);

        m_regexChip->setVisible(m_showRegexControls && showsRegexChip(m_variant));
        m_builderButton->setVisible(m_showRegexControls);

        update();
    }

} // namespace Material
