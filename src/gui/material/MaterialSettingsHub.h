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

#ifndef KEEPASSXC_MATERIALSETTINGSHUB_H
#define KEEPASSXC_MATERIALSETTINGSHUB_H

#include "MaterialChip.h"

#include "core/Config.h"

#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>
#include <QVariant>
#include <QWidget>

namespace Material
{
    class SearchBar;
    class SettingsScreen;
    class SpecSheet;

    /**
     * The settings destination.
     *
     * One 266px sidebar over three kinds of surface:
     *
     *  - Overview, the design's Appearance / Language / Voice / Behaviour /
     *    Integrations cards, which is MaterialSettingsScreen;
     *  - the seven spec sheets - General, Security, Browser Integration, SSH
     *    Agent, Secret Service, KeeShare and Passkeys - whose rows are bound to
     *    the real Config keys, so clicking a row writes the setting and the
     *    control pill reports the new value;
     *  - the stock ApplicationSettingsWidget, kept reachable as the classic
     *    editor so nothing becomes unreachable if a spec sheet row is missing.
     *
     * Every surface carries its own search bar. Typing filters the rows on the
     * page and says in plain words how many matches sit on the other pages.
     */
    class SettingsHub : public QWidget
    {
        Q_OBJECT

    public:
        explicit SettingsHub(QWidget* parent = nullptr);
        ~SettingsHub() override;

        SettingsScreen* overview() const;
        SpecSheet* specSheet() const;

        /** Adopt the stock settings editor as the classic editor page. */
        void setClassicEditor(QWidget* editor);
        QWidget* classicEditor() const;

        QString currentPage() const;
        void setCurrentPage(const QString& id);
        /** Show the stock editor, e.g. because a row is managed by a real page. */
        void showClassicEditor();

        /** The search bar of the surface on screen, or nullptr for the editor. */
        SearchBar* activeSearchBar() const;

    signals:
        /** The overview's interface font row was activated. */
        void interfaceFontRequested();
        /** An overview integration row was activated: "browser", "ssh-agent", ... */
        void integrationActivated(const QString& id);
        /** A search bar asked for the regex builder; the host opens it. */
        void builderRequested(Material::SearchBar* bar);

    private:
        /** How a bound row edits its configuration value. */
        enum class Control
        {
            Toggle, // booleans: the pill is On or Off
            Number, // integers: a spin box sheet
            Choice, // enumerations: a list of options
            Text, // free text: a line edit sheet
            Path, // a file or folder, with a browse button
            Managed // owned by a real settings page; the row opens the editor
        };

        struct Option
        {
            QVariant value;
            QString label;
        };

        struct Binding
        {
            QString pageId;
            QString rowKey;
            QString label;
            QString sub;
            Config::ConfigKey key = Config::Deleted;
            Control control = Control::Toggle;
            int minimum = 0;
            int maximum = 0;
            QString suffix;
            QList<Option> options;
        };

        void buildOverview();
        void buildGeneralPage();
        void buildSecurityPage();
        void buildBrowserPage();
        void buildSshAgentPage();
        void buildKeeSharePage();
        void buildPasskeysPage();

        void addToggle(const QString& pageId,
                       const QString& section,
                       const QString& symbol,
                       const QString& label,
                       const QString& sub,
                       Config::ConfigKey key);
        void addNumber(const QString& pageId,
                       const QString& section,
                       const QString& symbol,
                       const QString& label,
                       const QString& sub,
                       Config::ConfigKey key,
                       int minimum,
                       int maximum,
                       const QString& suffix = {});
        void addChoice(const QString& pageId,
                       const QString& section,
                       const QString& symbol,
                       const QString& label,
                       const QString& sub,
                       Config::ConfigKey key,
                       const QList<Option>& options);
        void addText(const QString& pageId,
                     const QString& section,
                     const QString& symbol,
                     const QString& label,
                     const QString& sub,
                     Config::ConfigKey key,
                     Control control = Control::Text);
        void addManaged(const QString& pageId,
                        const QString& section,
                        const QString& symbol,
                        const QString& label,
                        const QString& sub,
                        Config::ConfigKey key);

        /** Index into m_bindings for a row, or -1. */
        int indexOf(const QString& pageId, const QString& rowKey) const;
        /** The pill a binding's current value calls for. */
        void pillFor(const Binding& binding, PillKind* kind, QString* text) const;
        void refreshRow(const Binding& binding);
        void refreshAll();
        void handleRow(const QString& pageId, const QString& rowKey);
        void editBinding(const Binding& binding);

        SettingsScreen* m_overview = nullptr;
        SpecSheet* m_sheet = nullptr;
        QPointer<QWidget> m_classic;
        QList<Binding> m_bindings;
        QHash<QString, int> m_index;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSETTINGSHUB_H
