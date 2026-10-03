/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "MaterialMotion.h"
#include "core/Config.h"

#include <QAbstractButton>
#include <QApplication>
#include <QEvent>
#include <QSignalBlocker>
#include <QWidget>
#include <cmath>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace Material
{
    MotionPolicy* MotionPolicy::instance()
    {
        static MotionPolicy policy;
        return &policy;
    }

    MotionPolicy::MotionPolicy()
        : MotionPolicy([] {
#ifdef Q_OS_WIN
            BOOL enabled = TRUE;
            return ::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0) && enabled == FALSE;
#else
            return false;
#endif
        })
    {
    }

    MotionPolicy::MotionPolicy(std::function<bool()> systemPreference, QObject* parent)
        : QObject(parent), m_systemPreference(std::move(systemPreference))
    {
        if (qApp) {
            qApp->installEventFilter(this);
            qApp->installNativeEventFilter(this);
        }
        connect(config(), &Config::changed, this, [this](Config::ConfigKey key) {
            if (key == Config::GUI_ReducedMotion || key == Config::GUI_LowStimulation) {
                refresh();
            }
        });
        refresh();
    }

    bool MotionPolicy::effectiveReducedMotion(bool system, bool user, bool lowStimulation)
    {
        return system || user || lowStimulation;
    }

    bool MotionPolicy::reducedMotion() const { return m_reduced; }
    bool MotionPolicy::lowStimulation() const { return m_lowStimulation; }
    bool MotionPolicy::systemReducedMotion() const { return m_systemReduced; }
    int MotionPolicy::duration(int requested) const { return m_reduced ? 0 : qBound(0, requested, 240); }

    QEasingCurve MotionPolicy::standardCurve()
    {
        QEasingCurve curve(QEasingCurve::BezierSpline);
        curve.addCubicBezierSegment(QPointF(0.2, 0.0), QPointF(0.0, 1.0), QPointF(1.0, 1.0));
        return curve;
    }

    void MotionPolicy::refresh()
    {
        const bool system = m_systemPreference && m_systemPreference();
        const bool low = config()->get(Config::GUI_LowStimulation).toBool();
        const bool reduced = effectiveReducedMotion(system, config()->get(Config::GUI_ReducedMotion).toBool(), low);
        if (m_systemReduced == system && m_reduced == reduced && m_lowStimulation == low) {
            return;
        }
        m_systemReduced = system;
        m_reduced = reduced;
        m_lowStimulation = low;
        emit changed();
    }

    bool MotionPolicy::nativeEventFilter(const QByteArray&, void* message, qintptr*)
    {
#ifdef Q_OS_WIN
        const auto* msg = static_cast<MSG*>(message);
        if (msg && msg->message == WM_SETTINGCHANGE) {
            refresh();
        }
#else
        Q_UNUSED(message)
#endif
        return false;
    }

    bool MotionPolicy::eventFilter(QObject*, QEvent* event)
    {
        if (event->type() == QEvent::ApplicationActivate) {
            refresh();
        }
        return false;
    }

    MotionTransition::MotionTransition(QWidget* owner, MotionPolicy* policy)
        : QObject(owner), m_owner(owner), m_policy(policy ? policy : MotionPolicy::instance())
    {
        Q_ASSERT(owner);
        owner->installEventFilter(this);
        m_animation.setEasingCurve(MotionPolicy::standardCurve());
        connect(&m_animation, &QVariantAnimation::valueChanged, this,
                [this](const QVariant& value) { setValue(value.toReal()); });
        connect(&m_animation, &QVariantAnimation::finished, this, &MotionTransition::finish);
        connect(m_policy, &MotionPolicy::changed, this, [this] {
            if (m_policy->reducedMotion()) {
                finish();
            }
        });
    }

    qreal MotionTransition::value() const { return m_value; }
    bool MotionTransition::isRunning() const { return m_animation.state() == QAbstractAnimation::Running; }

    void MotionTransition::setValue(qreal value)
    {
        if (qFuzzyCompare(m_value + 1.0, value + 1.0)) return;
        m_value = value;
        emit valueChanged(value);
    }

    void MotionTransition::snapTo(qreal value)
    {
        if (!std::isfinite(value)) return;
        ++m_generation;
        m_animation.stop();
        m_pending = false;
        m_target = value;
        setValue(value);
    }

    void MotionTransition::animateTo(qreal target, int requested)
    {
        if (!std::isfinite(target) || !m_owner) return;
        if (m_pending && qFuzzyCompare(target + 1.0, m_target + 1.0)) return;
        ++m_generation;
        m_animation.stop();
        m_target = target;
        m_pending = true;
        const int duration = m_policy ? m_policy->duration(requested) : 0;
        if (!duration || !m_owner->isVisible() || !m_owner->isEnabled()
            || qFuzzyCompare(m_value + 1.0, target + 1.0)) {
            finish();
            return;
        }
        // Endpoint changes evaluate at the retained animation time. Publish only
        // the new run, never those intermediate configuration values.
        const qreal current = m_value;
        {
            const QSignalBlocker blocker(&m_animation);
            m_animation.setDuration(duration);
            m_animation.setStartValue(current);
            m_animation.setEndValue(target);
            m_animation.setCurrentTime(0);
        }
        m_animation.start();
    }

    void MotionTransition::finish()
    {
        if (!m_pending) return;
        m_animation.stop();
        m_pending = false;
        const QPointer<MotionTransition> alive(this);
        const quint64 generation = m_generation;
        setValue(m_target);
        if (alive && generation == m_generation) emit settled();
    }

    bool MotionTransition::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == m_owner && (event->type() == QEvent::Hide
            || (event->type() == QEvent::EnabledChange && !m_owner->isEnabled()))) {
            finish();
        }
        return false;
    }

    MotionState* MotionState::attach(QWidget* widget)
    {
        if (!widget) return nullptr;
        auto* state = widget->findChild<MotionState*>(QStringLiteral("materialMotionState"), Qt::FindDirectChildrenOnly);
        return state ? state : new MotionState(widget);
    }

    MotionState::MotionState(QWidget* widget)
        : QObject(widget), m_widget(widget), m_layer(new MotionTransition(widget)),
          m_selection(new MotionTransition(widget))
    {
        setObjectName(QStringLiteral("materialMotionState"));
        widget->setAttribute(Qt::WA_Hover);
        widget->installEventFilter(this);
        connect(m_layer, &MotionTransition::valueChanged, widget, [widget] { widget->update(); });
        connect(m_selection, &MotionTransition::valueChanged, widget, [widget] { widget->update(); });
        if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
            connect(button, &QAbstractButton::pressed, this, [this] { refresh(); });
            connect(button, &QAbstractButton::released, this, [this] { refresh(); });
            connect(button, &QAbstractButton::toggled, this, [this] { refresh(); });
        }
        refresh(true);
    }

    qreal MotionState::opacity(const QWidget* widget, qreal fallback)
    {
        const auto* state = widget ? widget->findChild<MotionState*>(QStringLiteral("materialMotionState"), Qt::FindDirectChildrenOnly) : nullptr;
        return state ? state->m_layer->value() : fallback;
    }

    qreal MotionState::selection(const QWidget* widget, bool fallback)
    {
        const auto* state = widget ? widget->findChild<MotionState*>(QStringLiteral("materialMotionState"), Qt::FindDirectChildrenOnly) : nullptr;
        return state ? state->m_selection->value() : (fallback ? 1.0 : 0.0);
    }

    void MotionState::refresh(bool immediate)
    {
        auto* button = qobject_cast<QAbstractButton*>(m_widget);
        const qreal layer = !m_widget->isEnabled() ? 0.0 : button && button->isDown() ? 0.12
                          : m_widget->hasFocus() ? 0.12 : m_hovered ? 0.08 : 0.0;
        const qreal selected = button && button->isChecked() ? 1.0 : 0.0;
        if (immediate) {
            m_layer->snapTo(layer);
            m_selection->snapTo(selected);
        } else {
            m_layer->animateTo(layer);
            m_selection->animateTo(selected, 160);
        }
    }

    bool MotionState::eventFilter(QObject*, QEvent* event)
    {
        switch (event->type()) {
        case QEvent::Enter: case QEvent::HoverEnter: m_hovered = true; refresh(); break;
        case QEvent::Leave: case QEvent::HoverLeave: m_hovered = false; refresh(); break;
        case QEvent::FocusIn: case QEvent::FocusOut: refresh(); break;
        case QEvent::Hide: m_hovered = false; refresh(true); break;
        case QEvent::Show: case QEvent::EnabledChange: refresh(true); break;
        default: break;
        }
        return false;
    }
}
