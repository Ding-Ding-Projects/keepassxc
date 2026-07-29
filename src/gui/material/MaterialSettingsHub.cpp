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

#include "MaterialSettingsHub.h"

#include "MaterialButtons.h"
#include "MaterialElevation.h"
#include "MaterialNotifier.h"
#include "MaterialOverlay.h"
#include "MaterialSearchBar.h"
#include "MaterialSettingsScreen.h"
#include "MaterialSpecSheet.h"
#include "MaterialTheme.h"

#include "core/Translator.h"
#include "gui/FileDialog.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QSpinBox>
#include <QVBoxLayout>

#include <functional>

namespace Material
{
    namespace
    {
        constexpr int EditorWidth = 520;
        constexpr int EditorPadding = 24;
        /** Longest control pill before the value is shortened from the left. */
        constexpr int MaxPillText = 42;

        /** The separator between a page id and a row key in the binding index. */
        const QLatin1Char IndexSeparator('\x1f');

        /** The rounded-28 panel the value editor sits on. */
        class EditorPanel : public QWidget
        {
        public:
            explicit EditorPanel(QWidget* parent = nullptr)
                : QWidget(parent)
            {
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                paintSurface(&painter, rect(), Shape::ExtraLarge, theme()->color(Role::SurfaceContainerLowest));
            }
        };

        void styleLabel(QLabel* label, TypeRole type, Role color)
        {
            label->setFont(theme()->font(type));
            label->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(theme()->hex(color)));
        }

        /** A long value keeps its tail, which is the part that identifies it. */
        QString shortenValue(const QString& value)
        {
            if (value.length() <= MaxPillText) {
                return value;
            }
            return QStringLiteral("…") + value.right(MaxPillText - 1);
        }

        /**
         * The value editor sheet.
         *
         * One overlay for the three editable control kinds: a spin box for
         * numbers, a line edit - with a browse button for paths - for text, and
         * a column of option rows for enumerations. It reports the new value
         * through commit() and deletes itself once it has closed.
         */
        class SettingEditor : public Overlay
        {
        public:
            explicit SettingEditor(QWidget* parent)
                : Overlay(parent)
            {
                m_panel = new EditorPanel;

                auto* root = new QVBoxLayout(m_panel);
                root->setContentsMargins(EditorPadding, EditorPadding, EditorPadding, EditorPadding);
                root->setSpacing(8);

                m_headline = new QLabel(m_panel);
                m_headline->setWordWrap(true);
                root->addWidget(m_headline);

                m_supporting = new QLabel(m_panel);
                m_supporting->setWordWrap(true);
                root->addWidget(m_supporting);

                m_body = new QVBoxLayout;
                m_body->setContentsMargins(0, 10, 0, 0);
                m_body->setSpacing(8);
                root->addLayout(m_body);

                m_actions = new QHBoxLayout;
                m_actions->setContentsMargins(0, 14, 0, 0);
                m_actions->setSpacing(8);
                m_actions->addStretch(1);
                root->addLayout(m_actions);

                setSheetWidth(EditorWidth);
                setSheetWidget(m_panel);

                connect(this, &Overlay::closed, this, &QObject::deleteLater);
            }

            void setHeading(const QString& headline, const QString& supporting)
            {
                m_headline->setText(headline);
                m_supporting->setText(supporting);
                m_supporting->setVisible(!supporting.isEmpty());
                styleLabel(m_headline, TypeRole::TitleLarge, Role::OnSurface);
                styleLabel(m_supporting, TypeRole::BodyMedium, Role::OnSurfaceVariant);
            }

            void buildNumber(int value, int minimum, int maximum, const QString& suffix)
            {
                auto* spin = new QSpinBox(m_panel);
                spin->setRange(minimum, maximum);
                spin->setValue(qBound(minimum, value, maximum));
                spin->setSuffix(suffix.isEmpty() ? QString() : QStringLiteral(" ") + suffix);
                spin->setMinimumHeight(Layout::ButtonHeight);
                spin->setFont(theme()->font(TypeRole::BodyLarge));
                m_body->addWidget(spin);

                addCancel();
                auto* save = new FilledButton(QStringLiteral("check"), tr("Save"), m_panel);
                save->setMinimumHeight(Layout::ButtonHeight);
                m_actions->addWidget(save);
                connect(save, &QPushButton::clicked, this, [this, spin] {
                    const QVariant chosen = spin->value();
                    closeOverlay();
                    if (commit) {
                        commit(chosen);
                    }
                });
            }

            void buildText(const QString& value, bool isPath, bool directory)
            {
                auto* edit = new QLineEdit(value, m_panel);
                edit->setMinimumHeight(Layout::ButtonHeight);
                edit->setFont(theme()->font(isPath ? TypeRole::Mono : TypeRole::BodyLarge));
                edit->setClearButtonEnabled(true);

                auto* row = new QHBoxLayout;
                row->setContentsMargins(0, 0, 0, 0);
                row->setSpacing(8);
                row->addWidget(edit, 1);

                if (isPath) {
                    auto* browse = new OutlinedButton(QStringLiteral("folder_open"), tr("Browse…"), m_panel);
                    browse->setMinimumHeight(Layout::ButtonHeight);
                    row->addWidget(browse);
                    connect(browse, &QPushButton::clicked, this, [this, edit, directory] {
                        const QString picked =
                            directory ? fileDialog()->getExistingDirectory(this, tr("Select a folder"), edit->text())
                                      : fileDialog()->getOpenFileName(this, tr("Select a file"), edit->text());
                        if (!picked.isEmpty()) {
                            edit->setText(picked);
                        }
                    });
                }
                m_body->addLayout(row);

                addCancel();
                auto* save = new FilledButton(QStringLiteral("check"), tr("Save"), m_panel);
                save->setMinimumHeight(Layout::ButtonHeight);
                m_actions->addWidget(save);
                connect(save, &QPushButton::clicked, this, [this, edit] {
                    const QVariant chosen = edit->text();
                    closeOverlay();
                    if (commit) {
                        commit(chosen);
                    }
                });
                edit->setFocus();
            }

            void buildChoice(const QVariant& current, const QList<QPair<QVariant, QString>>& options)
            {
                for (const auto& option : options) {
                    const bool active = option.first.toString() == current.toString();
                    auto* row = new ButtonBase(active ? QStringLiteral("check") : QStringLiteral("chevron_right"),
                                               option.second,
                                               m_panel);
                    row->setRadius(Shape::Medium);
                    row->setMinimumHeight(Layout::ButtonHeight + 6);
                    row->setCursor(Qt::PointingHandCursor);
                    row->setRoles(active ? Role::SecondaryContainer : Role::SurfaceContainer,
                                  active ? Role::OnSecondaryContainer : Role::OnSurface);
                    m_body->addWidget(row);

                    const QVariant chosen = option.first;
                    connect(row, &QPushButton::clicked, this, [this, chosen] {
                        closeOverlay();
                        if (commit) {
                            commit(chosen);
                        }
                    });
                }
                addCancel();
            }

            std::function<void(const QVariant&)> commit;

        private:
            void addCancel()
            {
                auto* cancel = new TextButton(QString(), tr("Cancel"), m_panel);
                cancel->setMinimumHeight(Layout::ButtonHeight);
                m_actions->addWidget(cancel);
                connect(cancel, &QPushButton::clicked, this, &Overlay::closeOverlay);
            }

            QWidget* m_panel = nullptr;
            QLabel* m_headline = nullptr;
            QLabel* m_supporting = nullptr;
            QVBoxLayout* m_body = nullptr;
            QHBoxLayout* m_actions = nullptr;
        };

    } // namespace

    SettingsHub::SettingsHub(QWidget* parent)
        : QWidget(parent)
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        m_sheet = new SpecSheet(this);
        root->addWidget(m_sheet, 1);

        buildOverview();

        m_sheet->addSidebarSection(tr("SPEC SHEETS"));
        buildGeneralPage();
        buildSecurityPage();
        buildBrowserPage();
        buildSshAgentPage();
        buildSecretServicePage();
        buildKeeSharePage();
        buildPasskeysPage();

        connect(m_sheet, &SpecSheet::rowActivated, this, &SettingsHub::handleRow);
        connect(m_sheet, &SpecSheet::builderRequested, this, [this](const QString& pageId) {
            if (auto* page = m_sheet->page(pageId)) {
                emit builderRequested(page->searchBar());
            }
        });
        connect(config(), &Config::changed, this, [this](Config::ConfigKey) { refreshAll(); });

        refreshAll();
    }

    SettingsHub::~SettingsHub() = default;

    void SettingsHub::buildOverview()
    {
        m_overview = new SettingsScreen;
        connect(m_overview, &SettingsScreen::interfaceFontRequested, this, &SettingsHub::interfaceFontRequested);
        connect(m_overview, &SettingsScreen::integrationActivated, this, &SettingsHub::integrationActivated);
        if (auto* search = m_overview->searchBar()) {
            connect(search, &SearchBar::builderRequested, this, [this, search] { emit builderRequested(search); });
        }

        m_sheet->addSidebarSection(tr("OVERVIEW"));
        m_sheet->addWidgetPage(
            QStringLiteral("overview"), QStringLiteral("dashboard"), tr("Design overview"), m_overview);
    }

    SettingsScreen* SettingsHub::overview() const
    {
        return m_overview;
    }

    SpecSheet* SettingsHub::specSheet() const
    {
        return m_sheet;
    }

    void SettingsHub::setClassicEditor(QWidget* editor)
    {
        if (!editor || m_classic) {
            return;
        }
        m_classic = editor;
        m_sheet->addSidebarSection(tr("EVERYTHING ELSE"));
        m_sheet->addWidgetPage(
            QStringLiteral("classic"), QStringLiteral("edit_document"), tr("Classic editor"), editor);
    }

    QWidget* SettingsHub::classicEditor() const
    {
        return m_classic;
    }

    QString SettingsHub::currentPage() const
    {
        return m_sheet->currentPage();
    }

    void SettingsHub::setCurrentPage(const QString& id)
    {
        m_sheet->setCurrentPage(id);
    }

    void SettingsHub::showClassicEditor()
    {
        if (m_classic) {
            m_sheet->setCurrentPage(QStringLiteral("classic"));
        }
    }

    SearchBar* SettingsHub::activeSearchBar() const
    {
        const QString current = m_sheet->currentPage();
        if (current == QLatin1String("overview")) {
            return m_overview ? m_overview->searchBar() : nullptr;
        }
        if (auto* page = m_sheet->page(current)) {
            return page->searchBar();
        }
        return nullptr;
    }

    // ------------------------------------------------------------- row binding

    void SettingsHub::addToggle(const QString& pageId,
                                const QString& section,
                                const QString& symbol,
                                const QString& label,
                                const QString& sub,
                                Config::ConfigKey key)
    {
        Binding binding;
        binding.pageId = pageId;
        binding.rowKey = section + QLatin1Char('/') + label;
        binding.label = label;
        binding.sub = sub;
        binding.key = key;
        binding.control = Control::Toggle;

        PillKind kind = PillKind::Off;
        QString text;
        pillFor(binding, &kind, &text);
        m_sheet->addRow(pageId, section, symbol, label, sub, kind, text);

        m_index.insert(pageId + IndexSeparator + binding.rowKey, m_bindings.size());
        m_bindings.append(binding);
    }

    void SettingsHub::addNumber(const QString& pageId,
                                const QString& section,
                                const QString& symbol,
                                const QString& label,
                                const QString& sub,
                                Config::ConfigKey key,
                                int minimum,
                                int maximum,
                                const QString& suffix)
    {
        Binding binding;
        binding.pageId = pageId;
        binding.rowKey = section + QLatin1Char('/') + label;
        binding.label = label;
        binding.sub = sub;
        binding.key = key;
        binding.control = Control::Number;
        binding.minimum = minimum;
        binding.maximum = maximum;
        binding.suffix = suffix;

        PillKind kind = PillKind::Value;
        QString text;
        pillFor(binding, &kind, &text);
        m_sheet->addRow(pageId, section, symbol, label, sub, kind, text);

        m_index.insert(pageId + IndexSeparator + binding.rowKey, m_bindings.size());
        m_bindings.append(binding);
    }

    void SettingsHub::addChoice(const QString& pageId,
                                const QString& section,
                                const QString& symbol,
                                const QString& label,
                                const QString& sub,
                                Config::ConfigKey key,
                                const QList<Option>& options)
    {
        Binding binding;
        binding.pageId = pageId;
        binding.rowKey = section + QLatin1Char('/') + label;
        binding.label = label;
        binding.sub = sub;
        binding.key = key;
        binding.control = Control::Choice;
        binding.options = options;

        PillKind kind = PillKind::Value;
        QString text;
        pillFor(binding, &kind, &text);
        m_sheet->addRow(pageId, section, symbol, label, sub, kind, text);

        m_index.insert(pageId + IndexSeparator + binding.rowKey, m_bindings.size());
        m_bindings.append(binding);
    }

    void SettingsHub::addText(const QString& pageId,
                              const QString& section,
                              const QString& symbol,
                              const QString& label,
                              const QString& sub,
                              Config::ConfigKey key,
                              Control control)
    {
        Binding binding;
        binding.pageId = pageId;
        binding.rowKey = section + QLatin1Char('/') + label;
        binding.label = label;
        binding.sub = sub;
        binding.key = key;
        binding.control = control;

        PillKind kind = PillKind::Mono;
        QString text;
        pillFor(binding, &kind, &text);
        m_sheet->addRow(pageId, section, symbol, label, sub, kind, text);

        m_index.insert(pageId + IndexSeparator + binding.rowKey, m_bindings.size());
        m_bindings.append(binding);
    }

    void SettingsHub::addManaged(const QString& pageId,
                                 const QString& section,
                                 const QString& symbol,
                                 const QString& label,
                                 const QString& sub,
                                 Config::ConfigKey key)
    {
        Binding binding;
        binding.pageId = pageId;
        binding.rowKey = section + QLatin1Char('/') + label;
        binding.label = label;
        binding.sub = sub;
        binding.key = key;
        binding.control = Control::Managed;

        PillKind kind = PillKind::Mono;
        QString text;
        pillFor(binding, &kind, &text);
        m_sheet->addRow(pageId, section, symbol, label, sub, kind, text);

        m_index.insert(pageId + IndexSeparator + binding.rowKey, m_bindings.size());
        m_bindings.append(binding);
    }

    int SettingsHub::indexOf(const QString& pageId, const QString& rowKey) const
    {
        return m_index.value(pageId + IndexSeparator + rowKey, -1);
    }

    void SettingsHub::pillFor(const Binding& binding, PillKind* kind, QString* text) const
    {
        const QVariant value = config()->get(binding.key);
        switch (binding.control) {
        case Control::Toggle:
            *kind = value.toBool() ? PillKind::On : PillKind::Off;
            *text = value.toBool() ? tr("On") : tr("Off");
            return;
        case Control::Number: {
            const int number = value.toInt();
            *kind = PillKind::Value;
            *text = binding.suffix.isEmpty() ? QString::number(number)
                                             : QStringLiteral("%1 %2").arg(number).arg(binding.suffix);
            return;
        }
        case Control::Choice: {
            *kind = PillKind::Value;
            *text = value.toString();
            for (const auto& option : binding.options) {
                if (option.value.toString() == value.toString()) {
                    *text = option.label;
                    return;
                }
            }
            if (text->isEmpty()) {
                *text = tr("Default");
            }
            return;
        }
        case Control::Text:
        case Control::Path: {
            *kind = PillKind::Mono;
            const QString raw = value.toString();
            *text = raw.isEmpty() ? tr("Not set") : shortenValue(raw);
            return;
        }
        case Control::Managed: {
            *kind = PillKind::Mono;
            const QString raw = value.toString();
            const bool unset = raw.isEmpty() || raw == QLatin1String("0") || raw == QLatin1String("false");
            *text = unset ? tr("Not set") : tr("Configured");
            return;
        }
        }
    }

    void SettingsHub::refreshRow(const Binding& binding)
    {
        auto* page = m_sheet->page(binding.pageId);
        if (!page) {
            return;
        }
        auto* row = page->row(binding.rowKey);
        if (!row) {
            return;
        }
        PillKind kind = PillKind::Off;
        QString text;
        pillFor(binding, &kind, &text);
        row->setPill(kind, text);
    }

    void SettingsHub::refreshAll()
    {
        for (const auto& binding : m_bindings) {
            refreshRow(binding);
        }
    }

    void SettingsHub::handleRow(const QString& pageId, const QString& rowKey)
    {
        const int index = indexOf(pageId, rowKey);
        if (index < 0) {
            return;
        }
        const Binding binding = m_bindings.at(index);

        if (binding.control == Control::Toggle) {
            const bool next = !config()->get(binding.key).toBool();
            config()->set(binding.key, next);
            refreshRow(binding);
            return;
        }
        if (binding.control == Control::Managed) {
            if (m_classic) {
                showClassicEditor();
                Notify::info(binding.label,
                             tr("This value is written by the settings page that owns it. "
                                "The classic editor is open on that page."));
            } else {
                Notify::warning(binding.label, tr("This value is written by the page that owns it."));
            }
            return;
        }
        editBinding(binding);
    }

    void SettingsHub::editBinding(const Binding& binding)
    {
        auto* editor = new SettingEditor(window());
        editor->setHeading(binding.label, binding.sub);

        const Config::ConfigKey key = binding.key;
        QPointer<SettingsHub> guard(this);
        editor->commit = [guard, key](const QVariant& value) {
            config()->set(key, value);
            if (guard) {
                guard->refreshAll();
            }
        };

        switch (binding.control) {
        case Control::Number:
            editor->buildNumber(config()->get(key).toInt(), binding.minimum, binding.maximum, binding.suffix);
            break;
        case Control::Choice: {
            QList<QPair<QVariant, QString>> options;
            options.reserve(binding.options.size());
            for (const auto& option : binding.options) {
                options.append({option.value, option.label});
            }
            editor->buildChoice(config()->get(key), options);
            break;
        }
        case Control::Path: {
            // A pattern ending in a separator names a folder; anything else a file.
            const QString current = config()->get(key).toString();
            const bool directory = current.isEmpty() ? false : QFileInfo(current).isDir();
            editor->buildText(current, true, directory);
            break;
        }
        case Control::Text:
        default:
            editor->buildText(config()->get(key).toString(), false, false);
            break;
        }

        editor->openOverlay();
    }

    // ---------------------------------------------------------------- the pages

    void SettingsHub::buildGeneralPage()
    {
        const QString page = QStringLiteral("general");
        m_sheet->addPage(page, QStringLiteral("tune"), tr("General"));

        const QString startup = tr("Startup");
        addToggle(page,
                  startup,
                  QStringLiteral("looks_one"),
                  tr("Single instance"),
                  tr("A second launch raises the running window instead of opening another."),
                  Config::SingleInstance);
        addToggle(page,
                  startup,
                  QStringLiteral("history"),
                  tr("Reopen previous databases"),
                  tr("Load whatever was open when KeePassXC last closed."),
                  Config::OpenPreviousDatabasesOnStartup);
        addToggle(page,
                  startup,
                  QStringLiteral("rocket_launch"),
                  tr("Launch at system startup"),
                  tr("Register KeePassXC with the operating system's startup list."),
                  Config::GUI_LaunchAtStartup);
        addToggle(page,
                  startup,
                  QStringLiteral("minimize"),
                  tr("Start minimised"),
                  tr("Open straight into the tray or the task bar."),
                  Config::GUI_MinimizeOnStartup);
        addToggle(page,
                  startup,
                  QStringLiteral("database"),
                  tr("Remember recent databases"),
                  tr("Keep the list of databases offered on the welcome screen."),
                  Config::RememberLastDatabases);
        addNumber(page,
                  startup,
                  QStringLiteral("storage"),
                  tr("Recent database count"),
                  tr("How many entries the recent database list keeps."),
                  Config::NumberOfRememberedLastDatabases,
                  1,
                  30);
        addToggle(page,
                  startup,
                  QStringLiteral("vpn_key"),
                  tr("Remember key files and hardware keys"),
                  tr("Reoffer the key file and security key a database was last unlocked with."),
                  Config::RememberLastKeyFiles);
        addText(page,
                startup,
                QStringLiteral("edit_document"),
                tr("Default database file name"),
                tr("The name proposed when a new database is created."),
                Config::DefaultDatabaseFileName);

        const QString saving = tr("Saving and backups");
        addToggle(page,
                  saving,
                  QStringLiteral("save"),
                  tr("Save after every change"),
                  tr("Write the database as soon as anything is edited."),
                  Config::AutoSaveAfterEveryChange);
        addToggle(page,
                  saving,
                  QStringLiteral("save"),
                  tr("Save on exit"),
                  tr("Write pending changes when KeePassXC closes."),
                  Config::AutoSaveOnExit);
        addToggle(page,
                  saving,
                  QStringLiteral("save"),
                  tr("Save non-data changes"),
                  tr("Persist column widths, sort order and other view state."),
                  Config::AutoSaveNonDataChanges);
        addToggle(page,
                  saving,
                  QStringLiteral("sync"),
                  tr("Reload on external change"),
                  tr("Pick up edits another program made to the database file."),
                  Config::AutoReloadOnChange);
        addToggle(page,
                  saving,
                  QStringLiteral("backup"),
                  tr("Back up before saving"),
                  tr("Keep a copy of the previous database next to the new one."),
                  Config::BackupBeforeSave);
        addText(page,
                saving,
                QStringLiteral("folder"),
                tr("Backup path pattern"),
                tr("Where backups go. {DB_FILENAME} and {TIME} are substituted."),
                Config::BackupFilePathPattern,
                Control::Path);
        addToggle(page,
                  saving,
                  QStringLiteral("task"),
                  tr("Use atomic saves"),
                  tr("Write to a temporary file and rename it over the database."),
                  Config::UseAtomicSaves);
        addToggle(page,
                  saving,
                  QStringLiteral("task"),
                  tr("Use direct write saves"),
                  tr("Write in place, for file systems that refuse the rename."),
                  Config::UseDirectWriteSaves);

        const QString appearance = tr("Appearance");
        addChoice(page,
                  appearance,
                  QStringLiteral("light_mode"),
                  tr("Application theme"),
                  tr("Light, dark, or whatever the desktop asks for."),
                  Config::GUI_ApplicationTheme,
                  {{QStringLiteral("auto"), tr("Follow the system")},
                   {QStringLiteral("light"), tr("Light")},
                   {QStringLiteral("dark"), tr("Dark")},
                   {QStringLiteral("classic"), tr("Classic")}});
        addChoice(page,
                  appearance,
                  QStringLiteral("star"),
                  tr("Colour seed"),
                  tr("The key colour every Material role is derived from."),
                  Config::GUI_MaterialSeed,
                  {{QStringLiteral("keepass"), tr("KeePass blue")},
                   {QStringLiteral("purple"), tr("Purple")},
                   {QStringLiteral("green"), tr("Green")},
                   {QStringLiteral("amber"), tr("Amber")}});
        addChoice(page,
                  appearance,
                  QStringLiteral("filter_list"),
                  tr("Density"),
                  tr("Row height for every list, tree and table."),
                  Config::GUI_MaterialDensity,
                  {{QStringLiteral("compact"), tr("Compact")},
                   {QStringLiteral("comfortable"), tr("Comfortable")},
                   {QStringLiteral("spacious"), tr("Spacious")}});
        addToggle(page,
                  appearance,
                  QStringLiteral("expand_less"),
                  tr("Compact mode"),
                  tr("Tighten the stock widgets that are not part of the Material shell."),
                  Config::GUI_CompactMode);
        addNumber(page,
                  appearance,
                  QStringLiteral("short_text"),
                  tr("Font size offset"),
                  tr("Points added to every font in the interface."),
                  Config::GUI_FontSizeOffset,
                  -4,
                  8,
                  tr("pt"));

        QList<Option> languages;
        languages.append({QStringLiteral("system"), tr("Follow the system")});
        const auto available = Translator::availableLanguages();
        for (const auto& language : available) {
            if (language.first == QLatin1String("system")) {
                continue;
            }
            languages.append({language.first, language.second});
        }
        addChoice(page,
                  appearance,
                  QStringLiteral("language"),
                  tr("Interface language"),
                  tr("Takes effect after KeePassXC restarts."),
                  Config::GUI_Language,
                  languages);
        addToggle(page,
                  appearance,
                  QStringLiteral("code"),
                  tr("Monospace notes"),
                  tr("Show entry notes in a fixed width font."),
                  Config::GUI_MonospaceNotes);
        addToggle(page,
                  appearance,
                  QStringLiteral("password"),
                  tr("Colour passwords"),
                  tr("Tint digits and symbols so a password can be read aloud."),
                  Config::GUI_ColorPasswords);
        addToggle(page,
                  appearance,
                  QStringLiteral("lightbulb"),
                  tr("Dim sum surprise"),
                  tr("Let the interface offer the occasional dish."),
                  Config::GUI_DimSumSurprise);

        const QString window = tr("Window and tray");
        addToggle(page,
                  window,
                  QStringLiteral("notifications"),
                  tr("Show tray icon"),
                  tr("Keep KeePassXC in the system notification area."),
                  Config::GUI_ShowTrayIcon);
        addChoice(page,
                  window,
                  QStringLiteral("dark_mode"),
                  tr("Tray icon appearance"),
                  tr("How the tray icon is drawn."),
                  Config::GUI_TrayIconAppearance,
                  {{QStringLiteral("monochrome"), tr("Monochrome")},
                   {QStringLiteral("monochrome-light"), tr("Monochrome (light)")},
                   {QStringLiteral("monochrome-dark"), tr("Monochrome (dark)")},
                   {QStringLiteral("colorful"), tr("Colourful")}});
        addToggle(page,
                  window,
                  QStringLiteral("minimize"),
                  tr("Minimise to tray"),
                  tr("Hide the window rather than shrinking it to the task bar."),
                  Config::GUI_MinimizeToTray);
        addToggle(page,
                  window,
                  QStringLiteral("close"),
                  tr("Minimise instead of closing"),
                  tr("The window close button hides KeePassXC rather than quitting it."),
                  Config::GUI_MinimizeOnClose);
        addToggle(page,
                  window,
                  QStringLiteral("arrow_upward"),
                  tr("Always on top"),
                  tr("Keep the window above every other window."),
                  Config::GUI_AlwaysOnTop);
        addToggle(page,
                  window,
                  QStringLiteral("menu_book"),
                  tr("Hide the menu bar"),
                  tr("The Material shell replaces it; the command palette still runs every action."),
                  Config::GUI_HideMenubar);
        addToggle(page,
                  window,
                  QStringLiteral("build"),
                  tr("Hide the tool bar"),
                  tr("The Material app bar replaces it."),
                  Config::GUI_HideToolbar);
        addToggle(page,
                  window,
                  QStringLiteral("drive_file_move"),
                  tr("Movable tool bar"),
                  tr("Let the stock tool bar be dragged to another edge."),
                  Config::GUI_MovableToolbar);
        addChoice(page,
                  window,
                  QStringLiteral("short_text"),
                  tr("Tool button style"),
                  tr("Whether stock tool buttons carry a label."),
                  Config::GUI_ToolButtonStyle,
                  {{static_cast<int>(Qt::ToolButtonIconOnly), tr("Icon only")},
                   {static_cast<int>(Qt::ToolButtonTextOnly), tr("Text only")},
                   {static_cast<int>(Qt::ToolButtonTextBesideIcon), tr("Text beside icon")},
                   {static_cast<int>(Qt::ToolButtonTextUnderIcon), tr("Text under icon")},
                   {static_cast<int>(Qt::ToolButtonFollowStyle), tr("Follow the style")}});
        addToggle(page,
                  window,
                  QStringLiteral("folder"),
                  tr("Hide the group panel"),
                  tr("Drop the group tree from the vault."),
                  Config::GUI_HideGroupPanel);
        addToggle(page,
                  window,
                  QStringLiteral("visibility_off"),
                  tr("Hide the preview panel"),
                  tr("Drop the entry detail pane from the vault."),
                  Config::GUI_HidePreviewPanel);

        const QString entryList = tr("Entry list");
        addToggle(page,
                  entryList,
                  QStringLiteral("person"),
                  tr("Hide usernames"),
                  tr("Mask the username column in the entry list."),
                  Config::GUI_HideUsernames);
        addToggle(page,
                  entryList,
                  QStringLiteral("password"),
                  tr("Hide passwords"),
                  tr("Mask the password column in the entry list."),
                  Config::GUI_HidePasswords);
        addToggle(page,
                  entryList,
                  QStringLiteral("search"),
                  tr("Search on Enter only"),
                  tr("Wait for Return rather than searching as you type."),
                  Config::GUI_SearchWaitForEnter);
        addToggle(page,
                  entryList,
                  QStringLiteral("event_busy"),
                  tr("Report expired entries on unlock"),
                  tr("Raise a message when a database is opened with expiring entries."),
                  Config::GUI_ShowExpiredEntriesOnDatabaseUnlock);
        addNumber(page,
                  entryList,
                  QStringLiteral("calendar_month"),
                  tr("Expiry warning window"),
                  tr("How far ahead the expiry report looks."),
                  Config::GUI_ShowExpiredEntriesOnDatabaseUnlockOffsetDays,
                  0,
                  365,
                  tr("days"));
        addToggle(page,
                  entryList,
                  QStringLiteral("folder_open"),
                  tr("Search the selected group only"),
                  tr("Limit a search to the group picked in the tree."),
                  Config::SearchLimitGroup);

        const QString entries = tr("Entry handling");
        addToggle(page,
                  entries,
                  QStringLiteral("open_in_new"),
                  tr("Minimise when opening a URL"),
                  tr("Get out of the browser's way once a link is launched."),
                  Config::MinimizeOnOpenUrl);
        addToggle(page,
                  entries,
                  QStringLiteral("web"),
                  tr("Open URL on double click"),
                  tr("The legacy switch the URL action below replaced."),
                  Config::OpenURLOnDoubleClick);
        addChoice(page,
                  entries,
                  QStringLiteral("link"),
                  tr("URL double click action"),
                  tr("What a double click on the URL column does."),
                  Config::URLDoubleClickAction,
                  {{0, tr("Open the URL in a browser")}, {1, tr("Copy the URL")}, {2, tr("Edit the entry")}});
        addToggle(page,
                  entries,
                  QStringLiteral("visibility_off"),
                  tr("Hide the window on copy"),
                  tr("Get out of the way when something is copied."),
                  Config::HideWindowOnCopy);
        addToggle(page,
                  entries,
                  QStringLiteral("minimize"),
                  tr("Minimise on copy"),
                  tr("How the window hides itself after a copy."),
                  Config::MinimizeOnCopy);
        addToggle(page,
                  entries,
                  QStringLiteral("arrow_downward"),
                  tr("Drop to background on copy"),
                  tr("Send the window behind the others instead of minimising."),
                  Config::DropToBackgroundOnCopy);
        addToggle(page,
                  entries,
                  QStringLiteral("lock_open"),
                  tr("Minimise after unlocking"),
                  tr("Hide the window once a database has been opened."),
                  Config::MinimizeAfterUnlock);
        addToggle(page,
                  entries,
                  QStringLiteral("casino"),
                  tr("Generate a password for new entries"),
                  tr("Prefill the password field when an entry is created."),
                  Config::AutoGeneratePasswordForNewEntries);
        addToggle(page,
                  entries,
                  QStringLiteral("folder"),
                  tr("Inherit the group icon"),
                  tr("A new entry starts with the icon of the group it lands in."),
                  Config::UseGroupIconOnEntryCreation);
        addNumber(page,
                  entries,
                  QStringLiteral("download"),
                  tr("Favicon download timeout"),
                  tr("How long a website icon download is given."),
                  Config::FaviconDownloadTimeout,
                  1,
                  60,
                  tr("seconds"));

        const QString autoType = tr("Auto-Type");
        addToggle(page,
                  autoType,
                  QStringLiteral("keyboard"),
                  tr("Match the window title"),
                  tr("Offer entries whose title appears in the target window title."),
                  Config::AutoTypeEntryTitleMatch);
        addToggle(page,
                  autoType,
                  QStringLiteral("web"),
                  tr("Match the entry URL"),
                  tr("Offer entries whose URL appears in the target window title."),
                  Config::AutoTypeEntryURLMatch);
        addToggle(page,
                  autoType,
                  QStringLiteral("event_busy"),
                  tr("Hide expired entries"),
                  tr("Keep expired entries out of the Auto-Type picker."),
                  Config::AutoTypeHideExpiredEntry);
        addNumber(page,
                  autoType,
                  QStringLiteral("timer"),
                  tr("Delay between keystrokes"),
                  tr("Slow Auto-Type down for applications that drop characters."),
                  Config::AutoTypeDelay,
                  0,
                  2000,
                  tr("ms"));
        addNumber(page,
                  autoType,
                  QStringLiteral("schedule"),
                  tr("Delay before the first keystroke"),
                  tr("Time given to the target window to take focus."),
                  Config::AutoTypeStartDelay,
                  0,
                  10000,
                  tr("ms"));
        addNumber(page,
                  autoType,
                  QStringLiteral("history"),
                  tr("Remember the last typed entry for"),
                  tr("How long the global shortcut repeats the previous entry."),
                  Config::GlobalAutoTypeRetypeTime,
                  0,
                  300,
                  tr("seconds"));
        addManaged(page,
                   autoType,
                   QStringLiteral("keyboard"),
                   tr("Global Auto-Type shortcut key"),
                   tr("Recorded by the shortcut widget in the classic editor."),
                   Config::GlobalAutoTypeKey);
        addManaged(page,
                   autoType,
                   QStringLiteral("keyboard"),
                   tr("Global Auto-Type modifiers"),
                   tr("Recorded together with the shortcut key."),
                   Config::GlobalAutoTypeModifiers);
        addToggle(page,
                  autoType,
                  QStringLiteral("dns"),
                  tr("Prefer desktop portals"),
                  tr("Use the desktop portal for Auto-Type where one is offered."),
                  Config::AutoTypePreferDesktopPortals);
        addToggle(page,
                  autoType,
                  QStringLiteral("link"),
                  tr("Keep the portal session open"),
                  tr("Reuse one portal connection instead of asking every time."),
                  Config::AutoTypeDesktopPortalPersistConnection);
        addToggle(page,
                  autoType,
                  QStringLiteral("content_paste"),
                  tr("Type through the clipboard"),
                  tr("Paste rather than synthesise keystrokes under the portal."),
                  Config::AutoTypeDesktopPortalUseClipboard);
        addChoice(page,
                  autoType,
                  QStringLiteral("history"),
                  tr("Portal session lifetime"),
                  tr("How long the desktop remembers the granted portal session."),
                  Config::AutoTypeDesktopPortalPersistMode,
                  {{0, tr("Never remember the session")},
                   {1, tr("Remember until exit")},
                   {2, tr("Remember until the desktop revokes it")}});
        addToggle(page,
                  autoType,
                  QStringLiteral("check"),
                  tr("Global shortcuts configured"),
                  tr("Records that the desktop's own shortcut editor has been visited."),
                  Config::GUI_XDPGlobalShortcutsConfigured);

        const QString generator = tr("Password generator");
        addChoice(page,
                  generator,
                  QStringLiteral("casino"),
                  tr("Generator mode"),
                  tr("Characters or a passphrase."),
                  Config::PasswordGenerator_Type,
                  {{0, tr("Password")}, {1, tr("Passphrase")}});
        addNumber(page,
                  generator,
                  QStringLiteral("looks_one"),
                  tr("Password length"),
                  tr("Characters drawn for a generated password."),
                  Config::PasswordGenerator_Length,
                  1,
                  128,
                  tr("characters"));
        addToggle(page,
                  generator,
                  QStringLiteral("short_text"),
                  tr("Lower case letters"),
                  tr("Include a-z."),
                  Config::PasswordGenerator_LowerCase);
        addToggle(page,
                  generator,
                  QStringLiteral("short_text"),
                  tr("Upper case letters"),
                  tr("Include A-Z."),
                  Config::PasswordGenerator_UpperCase);
        addToggle(page,
                  generator,
                  QStringLiteral("looks_one"),
                  tr("Numbers"),
                  tr("Include 0-9."),
                  Config::PasswordGenerator_Numbers);
        addToggle(page,
                  generator,
                  QStringLiteral("code"),
                  tr("Special characters"),
                  tr("Include the common punctuation block."),
                  Config::PasswordGenerator_SpecialChars);
        addToggle(page,
                  generator,
                  QStringLiteral("language"),
                  tr("Extended ASCII"),
                  tr("Include the upper half of the Latin-1 table."),
                  Config::PasswordGenerator_EASCII);
        addToggle(page,
                  generator,
                  QStringLiteral("code"),
                  tr("Braces"),
                  tr("Include {[()]}."),
                  Config::PasswordGenerator_Braces);
        addToggle(page,
                  generator,
                  QStringLiteral("code"),
                  tr("Punctuation"),
                  tr("Include .,:;."),
                  Config::PasswordGenerator_Punctuation);
        addToggle(page,
                  generator,
                  QStringLiteral("code"),
                  tr("Quotes"),
                  tr("Include \" and '."),
                  Config::PasswordGenerator_Quotes);
        addToggle(page,
                  generator,
                  QStringLiteral("remove"),
                  tr("Dashes and slashes"),
                  tr("Include -/\\_|."),
                  Config::PasswordGenerator_Dashes);
        addToggle(page,
                  generator,
                  QStringLiteral("add"),
                  tr("Math symbols"),
                  tr("Include <*+!?=."),
                  Config::PasswordGenerator_Math);
        addToggle(page,
                  generator,
                  QStringLiteral("tag"),
                  tr("Logograms"),
                  tr("Include ^°&@#$%."),
                  Config::PasswordGenerator_Logograms);
        addToggle(page,
                  generator,
                  QStringLiteral("build"),
                  tr("Advanced mode"),
                  tr("Show the per-block character switches in the generator."),
                  Config::PasswordGenerator_AdvancedMode);
        addToggle(page,
                  generator,
                  QStringLiteral("visibility_off"),
                  tr("Exclude look-alike characters"),
                  tr("Drop l, 1, I, O, 0 and friends."),
                  Config::PasswordGenerator_ExcludeAlike);
        addToggle(page,
                  generator,
                  QStringLiteral("check"),
                  tr("Use every selected group"),
                  tr("Guarantee at least one character from each enabled set."),
                  Config::PasswordGenerator_EnsureEvery);
        addText(page,
                generator,
                QStringLiteral("add"),
                tr("Additional characters"),
                tr("Characters added to the pool."),
                Config::PasswordGenerator_AdditionalChars);
        addText(page,
                generator,
                QStringLiteral("remove"),
                tr("Excluded characters"),
                tr("Characters removed from the pool."),
                Config::PasswordGenerator_ExcludedChars);
        addNumber(page,
                  generator,
                  QStringLiteral("looks_one"),
                  tr("Word count"),
                  tr("Words drawn for a generated passphrase."),
                  Config::PasswordGenerator_WordCount,
                  1,
                  40,
                  tr("words"));
        addText(page,
                generator,
                QStringLiteral("short_text"),
                tr("Word separator"),
                tr("What goes between the words of a passphrase."),
                Config::PasswordGenerator_WordSeparator);
        addText(page,
                generator,
                QStringLiteral("menu_book"),
                tr("Word list"),
                tr("The list passphrase words are drawn from."),
                Config::PasswordGenerator_WordList,
                Control::Path);
        addChoice(page,
                  generator,
                  QStringLiteral("sort_by_alpha"),
                  tr("Word case"),
                  tr("How passphrase words are capitalised."),
                  Config::PasswordGenerator_WordCase,
                  {{0, tr("lower case")}, {1, tr("UPPER CASE")}, {2, tr("Title Case")}, {3, tr("MiXeD cAsE")}});

        const QString voice = tr("Voice");
        addChoice(page,
                  voice,
                  QStringLiteral("language"),
                  tr("Message language"),
                  tr("Which language the application's own messages are written in."),
                  Config::GUI_VoiceLanguage,
                  {{QStringLiteral("English"), tr("English")},
                   {QStringLiteral("Cantonese"), tr("Cantonese")},
                   {QStringLiteral("Bilingual"), tr("Both")}});
        addNumber(page,
                  voice,
                  QStringLiteral("lightbulb"),
                  tr("English humour level"),
                  tr("1 keeps to the facts, 5 lets the interface enjoy itself."),
                  Config::GUI_FunnyLevelEnglish,
                  1,
                  5);
        addNumber(page,
                  voice,
                  QStringLiteral("lightbulb"),
                  tr("Cantonese humour level"),
                  tr("1 keeps to the facts, 5 lets the interface enjoy itself."),
                  Config::GUI_FunnyLevelCantonese,
                  1,
                  5);
        addToggle(page,
                  voice,
                  QStringLiteral("info"),
                  tr("Voice disclosure shown"),
                  tr("Records that the note about the message voice has been read."),
                  Config::GUI_VoiceDisclosureShown);

        const QString updates = tr("Updates and notices");
        addToggle(page,
                  updates,
                  QStringLiteral("update"),
                  tr("Check for updates"),
                  tr("Ask the project whether a newer release exists."),
                  Config::GUI_CheckForUpdates);
        addToggle(page,
                  updates,
                  QStringLiteral("science"),
                  tr("Include pre-releases"),
                  tr("Offer beta builds as well as stable ones."),
                  Config::GUI_CheckForUpdatesIncludeBetas);
        addToggle(page,
                  updates,
                  QStringLiteral("check"),
                  tr("Update prompt answered"),
                  tr("Records that the first update question has been answered."),
                  Config::UpdateCheckMessageShown);
        addToggle(page,
                  updates,
                  QStringLiteral("warning"),
                  tr("Hide the legacy key file warning"),
                  tr("Stop warning about key files in the old format."),
                  Config::Messages_NoLegacyKeyFileWarning);
        addToggle(page,
                  updates,
                  QStringLiteral("warning"),
                  tr("Hide the pre-release warning"),
                  tr("Stop warning that this build is not meant for production."),
                  Config::Messages_HidePreReleaseWarning);
    }

    void SettingsHub::buildSecurityPage()
    {
        const QString page = QStringLiteral("security");
        m_sheet->addPage(page, QStringLiteral("shield_lock"), tr("Security"));

        const QString timeouts = tr("Timeouts");
        addToggle(page,
                  timeouts,
                  QStringLiteral("content_copy"),
                  tr("Clear the clipboard"),
                  tr("Take a copied secret back out of the clipboard."),
                  Config::Security_ClearClipboard);
        addNumber(page,
                  timeouts,
                  QStringLiteral("timer"),
                  tr("Clipboard timeout"),
                  tr("How long a copied secret stays available."),
                  Config::Security_ClearClipboardTimeout,
                  1,
                  3600,
                  tr("seconds"));
        addToggle(page,
                  timeouts,
                  QStringLiteral("search"),
                  tr("Clear the search"),
                  tr("Empty the search field after a period of inactivity."),
                  Config::Security_ClearSearch);
        addNumber(page,
                  timeouts,
                  QStringLiteral("timer"),
                  tr("Search timeout"),
                  tr("How long a search term is kept."),
                  Config::Security_ClearSearchTimeout,
                  1,
                  3600,
                  tr("seconds"));
        addToggle(page,
                  timeouts,
                  QStringLiteral("lock"),
                  tr("Lock when idle"),
                  tr("Lock every database after a period without input."),
                  Config::Security_LockDatabaseIdle);
        addNumber(page,
                  timeouts,
                  QStringLiteral("schedule"),
                  tr("Idle timeout"),
                  tr("How long the application may sit unused before locking."),
                  Config::Security_LockDatabaseIdleSeconds,
                  10,
                  86400,
                  tr("seconds"));

        const QString locking = tr("Locking");
        addToggle(page,
                  locking,
                  QStringLiteral("minimize"),
                  tr("Lock on minimise"),
                  tr("Lock as soon as the window is hidden."),
                  Config::Security_LockDatabaseMinimize);
        addToggle(page,
                  locking,
                  QStringLiteral("lock"),
                  tr("Lock on screen lock"),
                  tr("Follow the desktop's own screen lock."),
                  Config::Security_LockDatabaseScreenLock);
        addToggle(page,
                  locking,
                  QStringLiteral("group"),
                  tr("Lock on user switch"),
                  tr("Lock when another account takes the session."),
                  Config::Security_LockDatabaseOnUserSwitch);
        addToggle(page,
                  locking,
                  QStringLiteral("keyboard"),
                  tr("Relock after Auto-Type"),
                  tr("Lock again once a global Auto-Type has finished."),
                  Config::Security_RelockAutoType);
        addToggle(page,
                  locking,
                  QStringLiteral("fingerprint"),
                  tr("Quick unlock"),
                  tr("Use the platform credential store to reopen a locked database."),
                  Config::Security_QuickUnlock);

        const QString privacy = tr("Privacy");
        addToggle(page,
                  privacy,
                  QStringLiteral("visibility_off"),
                  tr("Hide passwords"),
                  tr("Mask password fields until they are revealed."),
                  Config::Security_PasswordsHidden);
        addToggle(page,
                  privacy,
                  QStringLiteral("password"),
                  tr("Placeholder for empty passwords"),
                  tr("Draw dots even where there is no password."),
                  Config::Security_PasswordEmptyPlaceholder);
        addToggle(page,
                  privacy,
                  QStringLiteral("notes"),
                  tr("Hide notes"),
                  tr("Mask the notes field until it is revealed."),
                  Config::Security_HideNotes);
        addToggle(page,
                  privacy,
                  QStringLiteral("visibility_off"),
                  tr("Hide the password in the preview panel"),
                  tr("Keep the entry detail pane from showing a password."),
                  Config::Security_HidePasswordPreviewPanel);
        addToggle(page,
                  privacy,
                  QStringLiteral("schedule"),
                  tr("Hide the TOTP in the preview panel"),
                  tr("Keep the entry detail pane from showing a one-time code."),
                  Config::Security_HideTotpPreviewPanel);
        addToggle(page,
                  privacy,
                  QStringLiteral("download"),
                  tr("Fall back to a favicon service"),
                  tr("Ask a third party for a website icon the site did not provide."),
                  Config::Security_IconDownloadFallback);

        const QString confirmations = tr("Confirmations");
        addToggle(page,
                  confirmations,
                  QStringLiteral("help"),
                  tr("Ask before Auto-Type"),
                  tr("Confirm which entry is about to be typed."),
                  Config::Security_AutoTypeAsk);
        addToggle(page,
                  confirmations,
                  QStringLiteral("keyboard"),
                  tr("Skip the confirmation from the main window"),
                  tr("Do not ask again when Auto-Type is started from KeePassXC itself."),
                  Config::Security_AutoTypeSkipMainWindowConfirmation);
        addToggle(page,
                  confirmations,
                  QStringLiteral("delete"),
                  tr("Delete to the recycle bin without asking"),
                  tr("Move entries out of sight with no confirmation."),
                  Config::Security_NoConfirmMoveEntryToRecycleBin);
        addToggle(page,
                  confirmations,
                  QStringLiteral("content_copy"),
                  tr("Copy on double click"),
                  tr("A double click on a column copies its value."),
                  Config::Security_EnableCopyOnDoubleClick);
        addChoice(page,
                  confirmations,
                  QStringLiteral("health_and_safety"),
                  tr("Minimum database password quality"),
                  tr("How strong a database password has to be before it is accepted."),
                  Config::Security_DatabasePasswordMinimumQuality,
                  {{0, tr("No requirement")},
                   {1, tr("Poor")},
                   {2, tr("Weak")},
                   {3, tr("Good")},
                   {4, tr("Excellent")}});
    }

    void SettingsHub::buildBrowserPage()
    {
        const QString page = QStringLiteral("browser");
        m_sheet->addPage(page, QStringLiteral("extension"), tr("Browser Integration"));

        const QString integration = tr("Integration");
        addToggle(page,
                  integration,
                  QStringLiteral("extension"),
                  tr("Enable browser integration"),
                  tr("Serve credentials to the KeePassXC-Browser extension."),
                  Config::Browser_Enabled);
        addToggle(page,
                  integration,
                  QStringLiteral("download"),
                  tr("Install the extension automatically"),
                  tr("Register the native messaging host with every browser found."),
                  Config::Browser_AutoInstallExtension);
        addToggle(page,
                  integration,
                  QStringLiteral("notifications"),
                  tr("Show a notification on access"),
                  tr("Say so when the browser asks for a credential."),
                  Config::Browser_ShowNotification);
        addToggle(page,
                  integration,
                  QStringLiteral("lock_open"),
                  tr("Unlock on request"),
                  tr("Raise the unlock prompt when the browser needs a locked database."),
                  Config::Browser_UnlockDatabase);
        addToggle(page,
                  integration,
                  QStringLiteral("database"),
                  tr("Search every open database"),
                  tr("Look beyond the active database for a match."),
                  Config::Browser_SearchInAllDatabases);

        const QString matching = tr("Matching");
        addToggle(page,
                  matching,
                  QStringLiteral("check"),
                  tr("Return the best match only"),
                  tr("Offer one credential rather than every candidate."),
                  Config::Browser_BestMatchOnly);
        addToggle(page,
                  matching,
                  QStringLiteral("link"),
                  tr("Match the URL scheme"),
                  tr("Treat http and https as different sites."),
                  Config::Browser_MatchUrlScheme);
        addToggle(page,
                  matching,
                  QStringLiteral("event_busy"),
                  tr("Allow expired credentials"),
                  tr("Offer entries that have already expired."),
                  Config::Browser_AllowExpiredCredentials);
        addToggle(page,
                  matching,
                  QStringLiteral("tag"),
                  tr("Support KPH custom fields"),
                  tr("Pass KPH: prefixed attributes to the extension."),
                  Config::Browser_SupportKphFields);

        const QString permissions = tr("Permissions");
        addToggle(page,
                  permissions,
                  QStringLiteral("lock_open"),
                  tr("Never ask before returning credentials"),
                  tr("Skip the access prompt entirely."),
                  Config::Browser_AlwaysAllowAccess);
        addToggle(page,
                  permissions,
                  QStringLiteral("edit"),
                  tr("Never ask before updating credentials"),
                  tr("Skip the update prompt entirely."),
                  Config::Browser_AlwaysAllowUpdate);
        addToggle(page,
                  permissions,
                  QStringLiteral("web"),
                  tr("Answer HTTP basic authentication"),
                  tr("Fill the browser's own authentication dialog."),
                  Config::Browser_HttpAuthPermission);
        addToggle(page,
                  permissions,
                  QStringLiteral("database"),
                  tr("Allow database entry requests"),
                  tr("Let the extension enumerate entries rather than ask per site."),
                  Config::Browser_AllowGetDatabaseEntriesRequest);

        const QString proxy = tr("Proxy application");
        addToggle(page,
                  proxy,
                  QStringLiteral("dns"),
                  tr("Use the proxy application"),
                  tr("Talk to the browser through keepassxc-proxy."),
                  Config::Browser_SupportBrowserProxy);
        addToggle(page,
                  proxy,
                  QStringLiteral("build"),
                  tr("Use a custom proxy"),
                  tr("Point the browser at a proxy binary of your own."),
                  Config::Browser_UseCustomProxy);
        addText(page,
                proxy,
                QStringLiteral("terminal"),
                tr("Custom proxy location"),
                tr("Full path of the proxy executable."),
                Config::Browser_CustomProxyLocation,
                Control::Path);
        addToggle(page,
                  proxy,
                  QStringLiteral("update"),
                  tr("Update the proxy path automatically"),
                  tr("Rewrite the registered path whenever KeePassXC moves."),
                  Config::Browser_UpdateBinaryPath);

        const QString custom = tr("Custom browser");
        addToggle(page,
                  custom,
                  QStringLiteral("web"),
                  tr("Use a custom browser"),
                  tr("Register the host with a browser that is not detected."),
                  Config::Browser_UseCustomBrowser);
        addChoice(page,
                  custom,
                  QStringLiteral("extension"),
                  tr("Custom browser family"),
                  tr("Which native messaging layout the custom browser expects."),
                  Config::Browser_CustomBrowserType,
                  {{-1, tr("Not set")}, {1, tr("Chromium")}, {2, tr("Firefox")}});
        addText(page,
                custom,
                QStringLiteral("folder_open"),
                tr("Custom browser location"),
                tr("Folder the custom browser keeps its native messaging hosts in."),
                Config::Browser_CustomBrowserLocation,
                Control::Path);

        const QString migration = tr("Migration");
        addToggle(page,
                  migration,
                  QStringLiteral("info"),
                  tr("Hide the migration prompt"),
                  tr("Stop offering to migrate legacy browser settings."),
                  Config::Browser_NoMigrationPrompt);
    }

    void SettingsHub::buildSshAgentPage()
    {
        const QString page = QStringLiteral("sshagent");
        m_sheet->addPage(page, QStringLiteral("terminal"), tr("SSH Agent"));

        const QString agent = tr("Agent");
        addToggle(page,
                  agent,
                  QStringLiteral("terminal"),
                  tr("Enable the SSH agent integration"),
                  tr("Hand keys stored in a database to the running agent."),
                  Config::SSHAgent_Enabled);
        addToggle(page,
                  agent,
                  QStringLiteral("code"),
                  tr("Use OpenSSH"),
                  tr("Talk to the OpenSSH agent over its own socket."),
                  Config::SSHAgent_UseOpenSSH);
        addToggle(page,
                  agent,
                  QStringLiteral("build"),
                  tr("Use Pageant"),
                  tr("Talk to Pageant, the PuTTY agent."),
                  Config::SSHAgent_UsePageant);

        const QString overrides = tr("Overrides");
        addText(page,
                overrides,
                QStringLiteral("link"),
                tr("SSH_AUTH_SOCK override"),
                tr("Socket path used instead of the environment variable."),
                Config::SSHAgent_AuthSockOverride);
        addText(page,
                overrides,
                QStringLiteral("fingerprint"),
                tr("Security key provider override"),
                tr("Library used for FIDO backed keys."),
                Config::SSHAgent_SecurityKeyProviderOverride,
                Control::Path);
    }

    void SettingsHub::buildSecretServicePage()
    {
        const QString page = QStringLiteral("secretservice");
        m_sheet->addPage(page, QStringLiteral("dns"), tr("Secret Service"));

        const QString service = tr("Service");
        addToggle(page,
                  service,
                  QStringLiteral("dns"),
                  tr("Enable the Secret Service integration"),
                  tr("Answer org.freedesktop.secrets on the session bus."),
                  Config::FdoSecrets_Enabled);
        addToggle(page,
                  service,
                  QStringLiteral("notifications"),
                  tr("Show a notification on access"),
                  tr("Say so when another application reads a secret."),
                  Config::FdoSecrets_ShowNotification);
        addToggle(page,
                  service,
                  QStringLiteral("lock_open"),
                  tr("Unlock before searching"),
                  tr("Open a locked database rather than reporting no match."),
                  Config::FdoSecrets_UnlockBeforeSearch);

        const QString confirmations = tr("Confirmations");
        addToggle(page,
                  confirmations,
                  QStringLiteral("delete"),
                  tr("Confirm before deleting an item"),
                  tr("Ask before another application removes an entry."),
                  Config::FdoSecrets_ConfirmDeleteItem);
        addToggle(page,
                  confirmations,
                  QStringLiteral("help"),
                  tr("Confirm before revealing an item"),
                  tr("Ask before another application reads an entry."),
                  Config::FdoSecrets_ConfirmAccessItem);
    }

    void SettingsHub::buildKeeSharePage()
    {
        const QString page = QStringLiteral("keeshare");
        m_sheet->addPage(page, QStringLiteral("sync"), tr("KeeShare"));

        const QString sharing = tr("Sharing");
        addToggle(page,
                  sharing,
                  QStringLiteral("notifications"),
                  tr("Quiet successful shares"),
                  tr("Only report a share that went wrong."),
                  Config::KeeShare_QuietSuccess);
        addManaged(page,
                   sharing,
                   QStringLiteral("key"),
                   tr("Own signing certificate"),
                   tr("Generated on the KeeShare page of the classic editor."),
                   Config::KeeShare_Own);
        addManaged(page,
                   sharing,
                   QStringLiteral("group"),
                   tr("Trusted foreign certificates"),
                   tr("The signers whose shares this installation accepts."),
                   Config::KeeShare_Foreign);
        addManaged(page,
                   sharing,
                   QStringLiteral("sync"),
                   tr("Active shares"),
                   tr("Which databases are currently importing or exporting."),
                   Config::KeeShare_Active);
    }

    void SettingsHub::buildPasskeysPage()
    {
        const QString page = QStringLiteral("passkeys");
        m_sheet->addPage(page, QStringLiteral("passkey"), tr("Passkeys"));

        const QString passkeys = tr("Passkeys");
        addToggle(page,
                  passkeys,
                  QStringLiteral("bug_report"),
                  tr("Allow localhost with passkeys"),
                  tr("Permit insecure http://localhost origins, for testing."),
                  Config::Browser_AllowLocalhostWithPasskeys);

        const QString requirements = tr("What passkeys need");
        addToggle(page,
                  requirements,
                  QStringLiteral("extension"),
                  tr("Browser integration"),
                  tr("Passkey requests arrive through the extension; without it nothing is served."),
                  Config::Browser_Enabled);
        addToggle(page,
                  requirements,
                  QStringLiteral("lock_open"),
                  tr("Unlock for a passkey request"),
                  tr("Raise the unlock prompt when a passkey lives in a locked database."),
                  Config::Browser_UnlockDatabase);
        addToggle(page,
                  requirements,
                  QStringLiteral("link"),
                  tr("Match the origin scheme"),
                  tr("Treat http and https origins as different relying parties."),
                  Config::Browser_MatchUrlScheme);
        addToggle(page,
                  requirements,
                  QStringLiteral("database"),
                  tr("Search every open database"),
                  tr("Look beyond the active database for a stored passkey."),
                  Config::Browser_SearchInAllDatabases);

        const QString prompts = tr("Prompts");
        addToggle(page,
                  prompts,
                  QStringLiteral("lock_open"),
                  tr("Never ask before returning a passkey"),
                  tr("Skip the access prompt for passkeys as well as passwords."),
                  Config::Browser_AlwaysAllowAccess);
        addToggle(page,
                  prompts,
                  QStringLiteral("edit"),
                  tr("Never ask before updating a passkey"),
                  tr("Skip the update prompt when a relying party re-registers."),
                  Config::Browser_AlwaysAllowUpdate);
    }

} // namespace Material
