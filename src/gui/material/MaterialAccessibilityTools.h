/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef KEEPASSXC_MATERIALACCESSIBILITYTOOLS_H
#define KEEPASSXC_MATERIALACCESSIBILITYTOOLS_H

#include <QObject>
#include <QPointer>
#include <QList>

class QApplication;
class QWidget;
class QFocusFrame;

namespace Material
{
    // Profile-local controls shared by the shell, Qt dialogs and modal overlays.
    // This controller never reads a database, focused text or clipboard content.
    class AccessibilityTools : public QObject
    {
        Q_OBJECT
    public:
        static AccessibilityTools* install(QApplication* application);
        static void open(QWidget* parent);
        static void openSettings(QWidget* parent);
        static void restoreButton();
        static void reset();

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        explicit AccessibilityTools(QApplication* application);
        void attach(QWidget* surface);
        void refresh();
        void updateFocus(QWidget* widget);
        QList<QPointer<QWidget>> m_surfaces;
        QPointer<QFocusFrame> m_focusFrame;
    };
}
#endif
