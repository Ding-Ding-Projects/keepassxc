/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef KEEPASSXC_MATERIALMOTION_H
#define KEEPASSXC_MATERIALMOTION_H

#include <QAbstractNativeEventFilter>
#include <QEasingCurve>
#include <QObject>
#include <QPointer>
#include <QVariantAnimation>
#include <functional>

class QWidget;

namespace Material
{
    /** One event-driven motion policy. Platform accessibility always has a veto. */
    class MotionPolicy : public QObject, public QAbstractNativeEventFilter
    {
        Q_OBJECT
    public:
        static MotionPolicy* instance();
        explicit MotionPolicy(std::function<bool()> systemPreference, QObject* parent = nullptr);
        bool reducedMotion() const;
        bool lowStimulation() const;
        bool systemReducedMotion() const;
        int duration(int requested) const;
        static bool effectiveReducedMotion(bool system, bool user, bool lowStimulation);
        static QEasingCurve standardCurve();
        void refresh();
        bool nativeEventFilter(const QByteArray& type, void* message, qintptr* result) override;
    signals:
        void changed();
    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;
    private:
        MotionPolicy();
        std::function<bool()> m_systemPreference;
        bool m_systemReduced = false;
        bool m_reduced = false;
        bool m_lowStimulation = false;
    };

    /** A scalar-only transition. No widget pixels, content or geometry are retained. */
    class MotionTransition : public QObject
    {
        Q_OBJECT
    public:
        explicit MotionTransition(QWidget* owner, MotionPolicy* policy = nullptr);
        qreal value() const;
        bool isRunning() const;
        void animateTo(qreal target, int duration = 140);
        void snapTo(qreal value);
        void finish();
    signals:
        void valueChanged(qreal value);
        void settled();
    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;
    private:
        void setValue(qreal value);
        QPointer<QWidget> m_owner;
        QPointer<MotionPolicy> m_policy;
        QVariantAnimation m_animation;
        qreal m_value = 0.0;
        qreal m_target = 0.0;
        bool m_pending = false;
        quint64 m_generation = 0;
    };

    /** Shared state-layer ramps for button families. Focus remains immediately visible. */
    class MotionState : public QObject
    {
        Q_OBJECT
    public:
        static MotionState* attach(QWidget* widget);
        static qreal opacity(const QWidget* widget, qreal fallback = 0.0);
        static qreal selection(const QWidget* widget, bool fallback);
    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;
    private:
        explicit MotionState(QWidget* widget);
        void refresh(bool immediate = false);
        QWidget* m_widget;
        MotionTransition* m_layer;
        MotionTransition* m_selection;
        bool m_hovered = false;
    };
}
#endif
