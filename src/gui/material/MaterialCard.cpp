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

#include "MaterialCard.h"

#include "MaterialElevation.h"

#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
        /** Gap between the header labels and the caller's content. */
        constexpr int HeaderSpacing = 10;

        /** The fill a variant starts with; setFillRole() overrides it. */
        Role variantFillRole(Card::Variant variant)
        {
            return variant == Card::Variant::Filled ? Role::SurfaceContainerLow : Role::SurfaceContainerLowest;
        }
    } // namespace

    Card::Card(QWidget* parent)
        // Qualified: QFrame::Shape shadows Material::Shape inside this class.
        : Card(Variant::Outlined, Material::Shape::ExtraLarge, parent)
    {
    }

    Card::Card(Variant variant, int radius, QWidget* parent)
        : QFrame(parent)
        , m_variant(variant)
        , m_radius(radius)
        , m_fillRole(variantFillRole(variant))
    {
        // The surface is painted by hand, so the frame itself draws nothing.
        setFrameShape(QFrame::NoFrame);

        m_rootLayout = new QVBoxLayout(this);
        m_rootLayout->setSpacing(2);

        m_contentLayout = new QVBoxLayout;
        m_contentLayout->setContentsMargins(0, 0, 0, 0);
        m_contentLayout->setSpacing(8);
        m_rootLayout->addLayout(m_contentLayout);

        connect(theme(), &Theme::changed, this, &Card::applyTheme);
        applyTheme();
    }

    Card::~Card() = default;

    Card::Variant Card::variant() const
    {
        return m_variant;
    }

    void Card::setVariant(Variant variant)
    {
        if (variant == m_variant) {
            return;
        }
        // Follow the variant's fill unless the caller picked one of their own.
        if (m_fillRole == variantFillRole(m_variant)) {
            m_fillRole = variantFillRole(variant);
        }
        m_variant = variant;
        applyTheme();
    }

    int Card::radius() const
    {
        return m_radius;
    }

    void Card::setRadius(int radius)
    {
        if (radius == m_radius) {
            return;
        }
        m_radius = radius;
        update();
    }

    QString Card::titleText() const
    {
        return m_titleLabel ? m_titleLabel->text() : QString();
    }

    void Card::setTitleText(const QString& text)
    {
        if (text.isEmpty() && !m_titleLabel) {
            return;
        }
        if (!m_titleLabel) {
            m_titleLabel = new QLabel(this);
            m_titleLabel->setWordWrap(true);
            m_titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
            m_rootLayout->insertWidget(0, m_titleLabel);
        }
        m_titleLabel->setText(text);
        m_titleLabel->setVisible(!text.isEmpty());
        applyTheme();
    }

    QString Card::noteText() const
    {
        return m_noteLabel ? m_noteLabel->text() : QString();
    }

    void Card::setNoteText(const QString& text)
    {
        if (text.isEmpty() && !m_noteLabel) {
            return;
        }
        if (!m_noteLabel) {
            m_noteLabel = new QLabel(this);
            m_noteLabel->setWordWrap(true);
            m_noteLabel->setTextInteractionFlags(Qt::NoTextInteraction);
            m_rootLayout->insertWidget(m_titleLabel ? 1 : 0, m_noteLabel);
        }
        m_noteLabel->setText(text);
        m_noteLabel->setVisible(!text.isEmpty());
        applyTheme();
    }

    Role Card::fillRole() const
    {
        return m_fillRole;
    }

    void Card::setFillRole(Role role)
    {
        if (role == m_fillRole) {
            return;
        }
        m_fillRole = role;
        update();
    }

    int Card::elevationLevel() const
    {
        return m_elevationLevel;
    }

    void Card::setElevationLevel(int level)
    {
        level = qBound(1, level, 3);
        if (level == m_elevationLevel) {
            return;
        }
        m_elevationLevel = level;
        applyTheme();
    }

    QVBoxLayout* Card::contentLayout() const
    {
        return m_contentLayout;
    }

    void Card::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event);

        QColor fill;
        QColor border;
        switch (m_variant) {
        case Variant::Filled:
        case Variant::Elevated:
            fill = theme()->color(m_fillRole);
            break;
        case Variant::Outlined:
            border = theme()->color(Role::OutlineVariant);
            break;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        paintSurface(&painter, rect(), m_radius, fill, border);
    }

    void Card::applyTheme()
    {
        const int padding = theme()->pagePadding();
        m_rootLayout->setContentsMargins(padding, padding, padding, padding);

        if (m_titleLabel) {
            m_titleLabel->setFont(theme()->font(TypeRole::TitleSmall));
            m_titleLabel->setStyleSheet(
                QStringLiteral("background: transparent; color: %1;").arg(theme()->hex(Role::OnSurface)));
        }
        if (m_noteLabel) {
            m_noteLabel->setFont(theme()->font(TypeRole::LabelMedium));
            m_noteLabel->setStyleSheet(
                QStringLiteral("background: transparent; color: %1;").arg(theme()->hex(Role::OnSurfaceVariant)));
        }

        const bool hasHeader =
            (m_titleLabel && !m_titleLabel->text().isEmpty()) || (m_noteLabel && !m_noteLabel->text().isEmpty());
        m_contentLayout->setContentsMargins(0, hasHeader ? HeaderSpacing : 0, 0, 0);

        // A painted shadow would be clipped by our own rect, so it rides along
        // as an effect instead.
        if (m_variant == Variant::Elevated) {
            setGraphicsEffect(elevation(m_elevationLevel, this));
        } else {
            setGraphicsEffect(nullptr);
        }

        update();
    }

} // namespace Material
