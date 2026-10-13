/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "MaterialAccessibilityTools.h"
#include "MaterialOverlay.h"
#include "MaterialTheme.h"
#include "MaterialVoice.h"
#include "core/Config.h"
#include "gui/Application.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFocusFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
        constexpr int FooterHeight = 56;
        const char* Managed = "accessibilityManaged";
        const char* Footer = "accessibilityFooter";

        class FocusFrame : public QFocusFrame
        {
        public:
            explicit FocusFrame(QWidget* parent) : QFocusFrame(parent)
            {
                setAttribute(Qt::WA_TransparentForMouseEvents);
                setAttribute(Qt::WA_NoSystemBackground);
            }
        protected:
            void paintEvent(QPaintEvent*) override
            {
                QPainter painter(this);
                painter.setPen(QPen(theme()->color(Role::OnSurface), 4));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(rect().adjusted(2, 2, -3, -3));
            }
        };

        QString copy(const char* key)
        {
            return Voice::say(QString::fromLatin1(key));
        }

        template <typename Control> Control* translated(Control* control, const char* key)
        {
            auto refresh = [control, key] { control->setText(copy(key)); };
            refresh();
            QObject::connect(Voice::notifier(), &Voice::Notifier::changed, control, refresh);
            return control;
        }

        void accessibleName(QWidget* widget, const char* key)
        {
            auto refresh = [widget, key] { widget->setAccessibleName(copy(key)); };
            refresh();
            QObject::connect(Voice::notifier(), &Voice::Notifier::changed, widget, refresh);
        }

        QLabel* explanation(const char* key, QWidget* parent)
        {
            auto* label = translated(new QLabel(parent), key);
            label->setWordWrap(true);
            label->setTextFormat(Qt::PlainText);
            return label;
        }

        QDialog* dialog(QWidget* parent, const char* title)
        {
            auto* result = new QDialog(parent);
            result->setAttribute(Qt::WA_DeleteOnClose);
            result->setProperty(Managed, true);
            auto refresh = [result, title] { result->setWindowTitle(copy(title)); };
            refresh();
            QObject::connect(Voice::notifier(), &Voice::Notifier::changed, result, refresh);
            result->setWindowModality(Qt::WindowModal);
            result->resize(480, 540);
            return result;
        }
    }

    AccessibilityTools* AccessibilityTools::install(QApplication* application)
    {
        auto* existing = application->findChild<AccessibilityTools*>(QStringLiteral("accessibilityToolsController"));
        if (existing) {
            return existing;
        }
        return new AccessibilityTools(application);
    }

    AccessibilityTools::AccessibilityTools(QApplication* application) : QObject(application)
    {
        setObjectName(QStringLiteral("accessibilityToolsController"));
        application->installEventFilter(this);
        connect(application, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) { updateFocus(now); });
        connect(config(), &Config::changed, this, [this](Config::ConfigKey key) {
            if (key == Config::GUI_AccessibilityButtonVisible) {
                refresh();
            } else if (key == Config::GUI_AccessibilityStrongFocus) {
                updateFocus(QApplication::focusWidget());
            } else if (key == Config::GUI_AccessibilityHighContrast || key == Config::GUI_AccessibilityLargeTargets) {
                theme()->reload();
            }
        });
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (widget->isVisible()) {
                attach(widget);
            }
        }
    }

    bool AccessibilityTools::eventFilter(QObject* watched, QEvent* event)
    {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget) {
            return false;
        }
        if (event->type() == QEvent::Show) {
            attach(widget);
        } else if (event->type() == QEvent::Resize && widget->property(Managed).toBool()) {
            layoutFooter(widget);
        } else if (event->type() == QEvent::LayoutRequest && widget->objectName() == QLatin1String(Footer)) {
            QPointer<QWidget> surface = widget->parentWidget();
            QTimer::singleShot(0, this, [this, surface] { if (surface) layoutFooter(surface); });
        }
        return false;
    }

    void AccessibilityTools::attach(QWidget* surface)
    {
        if (auto* menu = qobject_cast<QMenu*>(surface)) {
            // aboutToShow handlers may clear and rebuild the menu on every open.
            for (auto* existing : menu->actions()) {
                if (existing->objectName() == QLatin1String("accessibilityMenuAction")) {
                    return;
                }
            }
            auto* action = translated(new QAction(menu), "accessibility.title");
            menu->addAction(action);
            action->setObjectName(QStringLiteral("accessibilityMenuAction"));
            action->setVisible(config()->get(Config::GUI_AccessibilityButtonVisible).toBool());
            connect(action, &QAction::triggered, menu, [menu] { open(menu->parentWidget()); });
            connect(config(), &Config::changed, action, [action](Config::ConfigKey key) {
                if (key == Config::GUI_AccessibilityButtonVisible) {
                    action->setVisible(config()->get(key).toBool());
                }
            });
            return;
        }
        if (surface->property(Managed).toBool()) {
            return;
        }
        const bool overlay = qobject_cast<Overlay*>(surface);
        if ((!surface->isWindow() && !overlay) || surface->windowType() == Qt::ToolTip
            || surface->windowType() == Qt::Popup || surface->windowType() == Qt::SplashScreen) {
            return;
        }
        surface->setProperty(Managed, true);
        m_surfaces.append(surface);
        const auto margins = surface->contentsMargins();
        surface->setProperty("accessibilityBaseBottom", margins.bottom());
        auto* footer = new QWidget(surface);
        footer->setObjectName(QString::fromLatin1(Footer));
        footer->setAutoFillBackground(true);
        auto* layout = new QHBoxLayout(footer);
        layout->setContentsMargins(8, 4, 8, 4);
        auto* tools = translated(new QPushButton(footer), "accessibility.entry");
        tools->setObjectName(QStringLiteral("accessibilityToolsButton"));
        accessibleName(tools, "accessibility.title");
        tools->setMinimumHeight(44);
        connect(tools, &QPushButton::clicked, surface, [surface] { open(surface); });
        layout->addWidget(tools);
        layout->addStretch();
        // Always available, including while the vault is locked or the entry is hidden.
        auto* settings = translated(new QPushButton(footer), "accessibility.settings");
        accessibleName(settings, "accessibility.settingsTitle");
        settings->setObjectName(QStringLiteral("accessibilitySettingsButton"));
        settings->setMinimumHeight(44);
        connect(settings, &QPushButton::clicked, surface, [surface] { openSettings(surface); });
        layout->addWidget(settings);
        if (surface->isWindow()) {
            auto* restoreAction = translated(new QAction(surface), "accessibility.restoreAction");
            restoreAction->setObjectName(QStringLiteral("showAccessibilityButtonAction"));
            connect(restoreAction, &QAction::triggered, surface, [] { restoreButton(); });
            surface->addAction(restoreAction);
            auto* settingsAction = translated(new QAction(surface), "accessibility.settingsTitle");
            settingsAction->setObjectName(QStringLiteral("accessibilitySettingsAction"));
            connect(settingsAction, &QAction::triggered, surface, [surface] { openSettings(surface); });
            surface->addAction(settingsAction);
        }
        tools->setVisible(config()->get(Config::GUI_AccessibilityButtonVisible).toBool());
        footer->show();
        layoutFooter(surface);
    }

    void AccessibilityTools::layoutFooter(QWidget* surface)
    {
        auto* footer = surface->findChild<QWidget*>(QString::fromLatin1(Footer), Qt::FindDirectChildrenOnly);
        if (!footer) {
            return;
        }
        const int height = qMax(FooterHeight, footer->minimumSizeHint().height());
        const int previous = surface->property("accessibilityFooterHeight").toInt();
        if (height != previous) {
            surface->setProperty("accessibilityFooterHeight", height);
            const auto margins = surface->contentsMargins();
            surface->setContentsMargins(margins.left(), margins.top(), margins.right(),
                                        surface->property("accessibilityBaseBottom").toInt() + height);
            if (!qobject_cast<Overlay*>(surface)) {
                surface->setMinimumSize(qMax(surface->minimumWidth(), footer->minimumSizeHint().width()),
                                        qMax(surface->minimumHeight() + height - previous,
                                             surface->minimumSizeHint().height()));
            }
        }
        footer->setGeometry(0, qMax(0, surface->height() - height), surface->width(), height);
    }

    void AccessibilityTools::refresh()
    {
        for (const auto& surface : m_surfaces) {
            if (!surface) {
                continue;
            }
            auto* footer = surface->findChild<QWidget*>(QString::fromLatin1(Footer), Qt::FindDirectChildrenOnly);
            if (footer) {
                footer->findChild<QPushButton*>(QStringLiteral("accessibilityToolsButton"))
                    ->setVisible(config()->get(Config::GUI_AccessibilityButtonVisible).toBool());
            }
        }
    }

    void AccessibilityTools::updateFocus(QWidget* widget)
    {
        if (m_focusFrame) {
            delete m_focusFrame;
        }
        if (widget && config()->get(Config::GUI_AccessibilityStrongFocus).toBool()) {
            m_focusFrame = new FocusFrame(widget->window());
            m_focusFrame->setWidget(widget);
            m_focusFrame->show();
        }
    }

    void AccessibilityTools::restoreButton()
    {
        config()->set(Config::GUI_AccessibilityButtonVisible, true);
    }

    void AccessibilityTools::reset()
    {
        restoreButton();
        config()->set(Config::GUI_AccessibilityHighContrast, false);
        config()->set(Config::GUI_AccessibilityStrongFocus, false);
        config()->set(Config::GUI_AccessibilityLargeTargets, false);
        config()->set(Config::GUI_ReducedMotion, false);
        config()->set(Config::GUI_FontScale, 1.0);
        Application::applyFontSize();
        theme()->reload();
    }

    void AccessibilityTools::openSettings(QWidget* parent)
    {
        auto* settings = dialog(parent, "accessibility.settingsTitle");
        settings->setObjectName(QStringLiteral("accessibilitySettings"));
        auto* layout = new QVBoxLayout(settings);
        layout->addWidget(explanation("accessibility.lockedHelp", settings));
        auto* restore = translated(new QPushButton(settings), "accessibility.show");
        restore->setObjectName(QStringLiteral("showAccessibilityButton"));
        connect(restore, &QPushButton::clicked, settings, [] { restoreButton(); });
        layout->addWidget(restore);
        auto* tools = translated(new QPushButton(settings), "accessibility.open");
        connect(tools, &QPushButton::clicked, settings, [settings] { open(settings); });
        layout->addWidget(tools);
        auto* close = new QDialogButtonBox(QDialogButtonBox::Close, settings);
        translated(close->button(QDialogButtonBox::Close), "accessibility.close");
        connect(close, &QDialogButtonBox::rejected, settings, &QDialog::reject);
        layout->addWidget(close);
        settings->show();
        restore->setFocus();
    }

    void AccessibilityTools::open(QWidget* parent)
    {
        auto* panel = dialog(parent, "accessibility.title");
        panel->setObjectName(QStringLiteral("accessibilityTools"));
        auto* outer = new QVBoxLayout(panel);
        auto* scroll = new QScrollArea(panel);
        scroll->setWidgetResizable(true);
        auto* body = new QWidget(scroll);
        auto* layout = new QVBoxLayout(body);
        auto* fontLabel = translated(new QLabel(body), "accessibility.textSize");
        auto* scale = new QSlider(Qt::Horizontal, body);
        scale->setObjectName(QStringLiteral("accessibilityTextSize"));
        accessibleName(scale, "accessibility.textSizeName");
        scale->setRange(85, 140);
        scale->setSingleStep(5);
        scale->setValue(qRound(config()->get(Config::GUI_FontScale).toDouble() * 100));
        fontLabel->setBuddy(scale);
        layout->addWidget(fontLabel);
        layout->addWidget(scale);
        connect(scale, &QSlider::valueChanged, panel, [](int value) {
            config()->set(Config::GUI_FontScale, value / 100.0);
            Application::applyFontSize();
            theme()->reload();
        });
        connect(config(), &Config::changed, scale, [scale](Config::ConfigKey key) {
            if (key == Config::GUI_FontScale) {
                const QSignalBlocker blocker(scale);
                scale->setValue(qRound(config()->get(key).toDouble() * 100));
            }
        });
        const auto addToggle = [body, layout](const char* title, const QString& name, Config::ConfigKey key) {
            auto* control = translated(new QCheckBox(body), title);
            control->setObjectName(name);
            control->setMinimumHeight(44);
            control->setChecked(config()->get(key).toBool());
            QObject::connect(control, &QCheckBox::toggled, control, [key](bool value) { config()->set(key, value); });
            QObject::connect(config(), &Config::changed, control, [control, key](Config::ConfigKey changed) {
                if (key == changed) {
                    const QSignalBlocker blocker(control);
                    control->setChecked(config()->get(key).toBool());
                }
            });
            layout->addWidget(control);
        };
        addToggle("accessibility.contrast", QStringLiteral("accessibilityHighContrast"), Config::GUI_AccessibilityHighContrast);
        addToggle("accessibility.motion", QStringLiteral("accessibilityReducedMotion"), Config::GUI_ReducedMotion);
        addToggle("accessibility.focus", QStringLiteral("accessibilityStrongFocus"), Config::GUI_AccessibilityStrongFocus);
        addToggle("accessibility.targets", QStringLiteral("accessibilityLargeTargets"), Config::GUI_AccessibilityLargeTargets);
        layout->addWidget(explanation("accessibility.keyboardHelp", body));
        auto* speech = explanation("accessibility.speechUnavailable", body);
        speech->setObjectName(QStringLiteral("accessibilitySpeechUnavailable"));
        layout->addWidget(speech);
        auto* resetButton = translated(new QPushButton(body), "accessibility.reset");
        resetButton->setObjectName(QStringLiteral("accessibilityReset"));
        connect(resetButton, &QPushButton::clicked, panel, [] { reset(); });
        layout->addWidget(resetButton);
        auto* separator = new QFrame(body);
        separator->setFrameShape(QFrame::HLine);
        layout->addWidget(separator);
        auto* hide = translated(new QPushButton(body), "accessibility.hide");
        hide->setObjectName(QStringLiteral("accessibilityHide"));
        connect(hide, &QPushButton::clicked, panel, [panel] {
            auto* confirmation = new QMessageBox(QMessageBox::Question, copy("accessibility.hideTitle"),
                copy("accessibility.hideHelp"),
                QMessageBox::Cancel | QMessageBox::Yes, panel);
            confirmation->setProperty(Managed, true);
            confirmation->setObjectName(QStringLiteral("accessibilityHideConfirmation"));
            confirmation->setAttribute(Qt::WA_DeleteOnClose);
            translated(confirmation->button(QMessageBox::Yes), "accessibility.confirmHide");
            translated(confirmation->button(QMessageBox::Cancel), "accessibility.cancel");
            connect(Voice::notifier(), &Voice::Notifier::changed, confirmation, [confirmation] {
                confirmation->setWindowTitle(copy("accessibility.hideTitle"));
                confirmation->setText(copy("accessibility.hideHelp"));
            });
            confirmation->setDefaultButton(QMessageBox::Cancel);
            confirmation->setEscapeButton(QMessageBox::Cancel);
            connect(confirmation, &QDialog::finished, panel, [panel](int result) {
                if (result == QMessageBox::Yes) {
                    config()->set(Config::GUI_AccessibilityButtonVisible, false);
                    panel->accept();
                }
            });
            confirmation->open();
            confirmation->button(QMessageBox::Cancel)->setFocus();
        });
        layout->addWidget(hide);
        layout->addStretch();
        scroll->setWidget(body);
        outer->addWidget(scroll);
        auto* close = new QDialogButtonBox(QDialogButtonBox::Close, panel);
        translated(close->button(QDialogButtonBox::Close), "accessibility.close");
        connect(close, &QDialogButtonBox::rejected, panel, &QDialog::reject);
        outer->addWidget(close);
        panel->show();
        scale->setFocus();
    }
}
