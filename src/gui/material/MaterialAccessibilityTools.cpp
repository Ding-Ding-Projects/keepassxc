/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "MaterialAccessibilityTools.h"
#include "MaterialOverlay.h"
#include "MaterialTheme.h"
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

        QLabel* explanation(const QString& text, QWidget* parent)
        {
            auto* label = new QLabel(text, parent);
            label->setWordWrap(true);
            label->setTextFormat(Qt::PlainText);
            return label;
        }

        QDialog* dialog(QWidget* parent, const QString& title)
        {
            auto* result = new QDialog(parent);
            result->setAttribute(Qt::WA_DeleteOnClose);
            result->setProperty(Managed, true);
            result->setWindowTitle(title);
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
            auto* footer = widget->findChild<QWidget*>(QString::fromLatin1(Footer), Qt::FindDirectChildrenOnly);
            if (footer) {
                footer->setGeometry(0, qMax(0, widget->height() - FooterHeight), widget->width(), FooterHeight);
            }
        }
        return false;
    }

    void AccessibilityTools::attach(QWidget* surface)
    {
        if (surface->property(Managed).toBool()) {
            return;
        }
        if (auto* menu = qobject_cast<QMenu*>(surface)) {
            menu->setProperty(Managed, true);
            auto* action = menu->addAction(tr("Accessibility tools…"));
            action->setObjectName(QStringLiteral("accessibilityMenuAction"));
            action->setVisible(config()->get(Config::GUI_AccessibilityButtonVisible).toBool());
            connect(action, &QAction::triggered, menu, [menu] { open(menu->parentWidget()); });
            connect(config(), &Config::changed, menu, [action](Config::ConfigKey key) {
                if (key == Config::GUI_AccessibilityButtonVisible) {
                    action->setVisible(config()->get(key).toBool());
                }
            });
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
        surface->setContentsMargins(margins.left(), margins.top(), margins.right(), margins.bottom() + FooterHeight);
        auto* footer = new QWidget(surface);
        footer->setObjectName(QString::fromLatin1(Footer));
        footer->setAutoFillBackground(true);
        auto* layout = new QHBoxLayout(footer);
        layout->setContentsMargins(8, 4, 8, 4);
        auto* tools = new QPushButton(tr("&Accessibility"), footer);
        tools->setObjectName(QStringLiteral("accessibilityToolsButton"));
        tools->setAccessibleName(tr("Accessibility tools"));
        tools->setMinimumHeight(44);
        connect(tools, &QPushButton::clicked, surface, [surface] { open(surface); });
        layout->addWidget(tools);
        layout->addStretch();
        // Always available, including while the vault is locked or the entry is hidden.
        auto* settings = new QPushButton(tr("&Settings"), footer);
        settings->setAccessibleName(tr("Settings: Accessibility"));
        settings->setObjectName(QStringLiteral("accessibilitySettingsButton"));
        settings->setMinimumHeight(44);
        connect(settings, &QPushButton::clicked, surface, [surface] { openSettings(surface); });
        layout->addWidget(settings);
        if (surface->isWindow()) {
            auto* restoreAction = new QAction(tr("Settings: Accessibility: Show accessibility button"), surface);
            restoreAction->setObjectName(QStringLiteral("showAccessibilityButtonAction"));
            connect(restoreAction, &QAction::triggered, surface, [] { restoreButton(); });
            surface->addAction(restoreAction);
            auto* settingsAction = new QAction(tr("Settings: Accessibility"), surface);
            settingsAction->setObjectName(QStringLiteral("accessibilitySettingsAction"));
            connect(settingsAction, &QAction::triggered, surface, [surface] { openSettings(surface); });
            surface->addAction(settingsAction);
        }
        footer->setGeometry(0, qMax(0, surface->height() - FooterHeight), surface->width(), FooterHeight);
        tools->setVisible(config()->get(Config::GUI_AccessibilityButtonVisible).toBool());
        footer->show();
        if (!overlay) {
            const QSize minimum = surface->minimumSize();
            surface->setMinimumSize(qMax(minimum.width(), footer->minimumSizeHint().width()),
                                    qMax(minimum.height() + FooterHeight, surface->minimumSizeHint().height()));
        }
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
        auto* settings = dialog(parent, tr("Settings: Accessibility"));
        settings->setObjectName(QStringLiteral("accessibilitySettings"));
        auto* layout = new QVBoxLayout(settings);
        layout->addWidget(explanation(tr("Accessibility preferences are available without unlocking a database."), settings));
        auto* restore = new QPushButton(tr("Show accessibility button"), settings);
        restore->setObjectName(QStringLiteral("showAccessibilityButton"));
        connect(restore, &QPushButton::clicked, settings, [] { restoreButton(); });
        layout->addWidget(restore);
        auto* tools = new QPushButton(tr("Open accessibility tools"), settings);
        connect(tools, &QPushButton::clicked, settings, [settings] { open(settings); });
        layout->addWidget(tools);
        auto* close = new QDialogButtonBox(QDialogButtonBox::Close, settings);
        connect(close, &QDialogButtonBox::rejected, settings, &QDialog::reject);
        layout->addWidget(close);
        settings->show();
        restore->setFocus();
    }

    void AccessibilityTools::open(QWidget* parent)
    {
        auto* panel = dialog(parent, tr("Accessibility tools"));
        panel->setObjectName(QStringLiteral("accessibilityTools"));
        auto* outer = new QVBoxLayout(panel);
        auto* scroll = new QScrollArea(panel);
        scroll->setWidgetResizable(true);
        auto* body = new QWidget(scroll);
        auto* layout = new QVBoxLayout(body);
        auto* fontLabel = new QLabel(tr("Text size (85–140%)"), body);
        auto* scale = new QSlider(Qt::Horizontal, body);
        scale->setObjectName(QStringLiteral("accessibilityTextSize"));
        scale->setAccessibleName(tr("Text size percentage"));
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
        const auto addToggle = [body, layout](const QString& title, const QString& name, Config::ConfigKey key) {
            auto* control = new QCheckBox(title, body);
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
        addToggle(tr("High contrast"), QStringLiteral("accessibilityHighContrast"), Config::GUI_AccessibilityHighContrast);
        addToggle(tr("Reduced motion"), QStringLiteral("accessibilityReducedMotion"), Config::GUI_ReducedMotion);
        addToggle(tr("Stronger keyboard focus"), QStringLiteral("accessibilityStrongFocus"), Config::GUI_AccessibilityStrongFocus);
        addToggle(tr("Larger interaction targets"), QStringLiteral("accessibilityLargeTargets"), Config::GUI_AccessibilityLargeTargets);
        layout->addWidget(explanation(tr("Keyboard navigation: Tab and Shift+Tab move between controls. Space activates a button or checkbox. Arrow keys adjust sliders. Escape closes this panel without hiding the accessibility button."), body));
        auto* speech = explanation(tr("Read aloud is unavailable: this build has no speech engine. Start, pause, resume, stop, voice and rate controls are unavailable. No database values, concealed fields or clipboard text are sent to speech. On Windows, use Windows+Ctrl+Enter to start or stop Narrator. Native operating-system dialogs use the operating system's accessibility controls."), body);
        speech->setObjectName(QStringLiteral("accessibilitySpeechUnavailable"));
        layout->addWidget(speech);
        auto* resetButton = new QPushButton(tr("Reset accessibility preferences"), body);
        resetButton->setObjectName(QStringLiteral("accessibilityReset"));
        connect(resetButton, &QPushButton::clicked, panel, [] { reset(); });
        layout->addWidget(resetButton);
        auto* separator = new QFrame(body);
        separator->setFrameShape(QFrame::HLine);
        layout->addWidget(separator);
        auto* hide = new QPushButton(tr("Hide accessibility button…"), body);
        hide->setObjectName(QStringLiteral("accessibilityHide"));
        connect(hide, &QPushButton::clicked, panel, [panel] {
            auto* confirmation = new QMessageBox(QMessageBox::Question, tr("Hide accessibility button?"),
                tr("Your accessibility preferences will stay active. Restore the button through Settings → Accessibility → Show accessibility button. Settings remains available at the bottom of each window, including locked windows."),
                QMessageBox::Cancel | QMessageBox::Yes, panel);
            confirmation->setProperty(Managed, true);
            confirmation->setObjectName(QStringLiteral("accessibilityHideConfirmation"));
            confirmation->setAttribute(Qt::WA_DeleteOnClose);
            confirmation->button(QMessageBox::Yes)->setText(tr("Hide button"));
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
        connect(close, &QDialogButtonBox::rejected, panel, &QDialog::reject);
        outer->addWidget(close);
        panel->show();
        scale->setFocus();
    }
}
