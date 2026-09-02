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

#include "MaterialIcons.h"

#include "core/Entry.h"
#include "core/EntryAttributes.h"

#include <QGuiApplication>
#include <QHash>
#include <QPainter>
#include <QScreen>
#include <QSvgRenderer>

#include <iterator>

// Q_INIT_RESOURCE declares its helper in the enclosing namespace, so the
// generated symbol set has to be registered from file scope.
static void initGeneratedSymbols()
{
    Q_INIT_RESOURCE(material_icons);
}

namespace Material
{
    namespace Icons
    {
        namespace
        {
            /** A Material Symbols name and the bundled icon that stands in for it. */
            struct Mapping
            {
                const char* symbol;
                const char* icon;
            };

            /**
             * Symbols served by the icon theme KeePassXC already ships. The icon
             * column is the path below `share/icons/application/scalable`; several
             * symbols deliberately share one glyph where the design uses them for
             * the same idea (key / vpn_key, code / terminal).
             */
            constexpr Mapping BundledSymbols[] = {
                {"add", "actions/document-new"},
                {"arrow_downward", "actions/arrow-collapse-down"},
                {"arrow_upward", "actions/move-up"},
                {"attach_file", "actions/paperclip"},
                {"attachment", "actions/paperclip"},
                {"backup", "actions/document-save-copy"},
                {"bug_report", "actions/bugreport"},
                {"build", "actions/hammer-wrench"},
                {"casino", "actions/password-generator"},
                {"close", "actions/dialog-close"},
                {"cloud", "actions/system-software-update"},
                {"code", "apps/utilities-terminal"},
                {"content_copy", "actions/attributes-copy"},
                {"content_paste", "actions/clipboard-text"},
                {"create_new_folder", "actions/group-new"},
                {"dashboard", "actions/reports"},
                {"database", "actions/database-search"},
                {"delete", "actions/trash"},
                {"dns", "apps/preferences-system-network-sharing"},
                {"download", "actions/favicon-download"},
                {"drive_file_move", "actions/group-edit"},
                {"edit", "actions/document-edit"},
                {"edit_document", "categories/preferences-other"},
                {"error", "status/dialog-error"},
                {"event_busy", "actions/entry-expire"},
                {"fingerprint", "actions/fingerprint"},
                {"folder_open", "actions/document-open"},
                {"health_and_safety", "actions/health"},
                {"help", "actions/system-help"},
                {"history", "actions/view-history"},
                {"info", "status/dialog-information"},
                {"insights", "actions/statistics"},
                {"key", "actions/database-change-key"},
                {"keyboard", "actions/auto-type"},
                {"keyboard_double_arrow_down", "actions/chevron-double-down"},
                {"keyboard_double_arrow_right", "actions/chevron-double-right"},
                {"label", "actions/tag"},
                {"language", "actions/web"},
                {"lightbulb", "actions/getting-started"},
                {"link", "actions/web"},
                {"lock", "actions/object-locked"},
                {"lock_open", "actions/object-unlocked"},
                {"logout", "actions/application-exit"},
                {"menu_book", "actions/user-guide"},
                {"merge", "actions/database-merge"},
                {"monitor_heart", "actions/health"},
                {"monitoring", "actions/statistics"},
                {"open_in_new", "actions/document-export"},
                {"passkey", "actions/passkey"},
                {"password", "actions/database-change-key"},
                {"qr_code", "actions/qrcode"},
                {"refresh", "actions/refresh"},
                {"restore", "actions/entry-restore"},
                {"rocket_launch", "actions/getting-started"},
                {"save", "actions/document-save"},
                {"schedule", "actions/totp"},
                {"science", "actions/getting-started"},
                {"search", "actions/system-search"},
                {"sell", "actions/tag"},
                {"settings", "actions/configure"},
                {"shield_lock", "actions/passkey"},
                {"sort", "actions/sort-alphabetical-ascending"},
                {"sort_by_alpha", "actions/sort-alphabetical-ascending"},
                {"storage", "actions/database-search"},
                {"sync", "actions/remote-sync"},
                {"tag", "actions/tag"},
                {"task", "actions/clipboard-text"},
                {"terminal", "apps/utilities-terminal"},
                {"timer", "actions/totp"},
                {"tune", "actions/configure"},
                {"update", "actions/refresh"},
                {"upload", "actions/document-import"},
                {"visibility", "actions/password-show-on"},
                {"visibility_off", "actions/password-show-off"},
                {"vpn_key", "actions/database-change-key"},
                {"warning", "status/dialog-warning"},
                {"web", "actions/web"},
            };

            /**
             * Symbols with no reasonable counterpart in the bundled set. These are
             * drawn by `share/icons/material`, a handful of plain 24x24 paths kept
             * deliberately small; the icon column is the file name below it.
             */
            constexpr Mapping GeneratedSymbols[] = {
                {"account_balance",      "credit_card"},
                {"account_circle",       "person"},
                {"account_tree",         "account_tree"},
                {"add_link",             "add_link"},
                {"add_photo_alternate",  "add_photo_alternate"},
                {"analytics",            "analytics"},
                {"article",              "description"},
                {"badge",                "badge"},
                {"block",                "block"},
                {"blur_on",              "blur_on"},
                {"bolt",                 "bolt"},
                {"cable",                "cable"},
                {"calendar_month",       "calendar_month"},
                {"category",             "category"},
                {"check",                "check"},
                {"check_circle",         "check_circle"},
                {"checklist",            "checklist"},
                {"chevron_left",         "chevron_left"},
                {"chevron_right",        "chevron_right"},
                {"cleaning_services",    "cleaning_services"},
                {"cloud_download",       "cloud_download"},
                {"cloud_sync",           "cloud_sync"},
                {"compress",             "compress"},
                {"construction",         "construction"},
                {"content_paste_off",    "content_paste_off"},
                {"credit_card",          "credit_card"},
                {"dark_mode",            "dark_mode"},
                {"data_object",          "data_object"},
                {"date_range",           "calendar_month"},
                {"delete_forever",       "delete_forever"},
                {"delete_sweep",         "delete_sweep"},
                {"description",          "description"},
                {"desktop_windows",      "desktop_windows"},
                {"difference",           "difference"},
                {"dock_to_right",        "dock_to_right"},
                {"done",                 "check"},
                {"edit_note",            "edit_note"},
                {"email",                "mail"},
                {"emoji_symbols",        "emoji_symbols"},
                {"enhanced_encryption",  "enhanced_encryption"},
                {"event",                "calendar_month"},
                {"expand_less",          "expand_less"},
                {"expand_more",          "expand_more"},
                {"extension",            "extension"},
                {"fast_forward",         "fast_forward"},
                {"file_download",        "file_download"},
                {"file_upload",          "file_upload"},
                {"filter_alt",           "filter_alt"},
                {"filter_list",          "filter_list"},
                {"folder",               "folder"},
                {"folder_off",           "folder_off"},
                {"folder_shared",        "folder_shared"},
                {"folder_zip",           "folder"},
                {"format_color_fill",    "format_color_fill"},
                {"format_color_text",    "format_color_text"},
                {"format_quote",         "format_quote"},
                {"format_size",          "format_size"},
                {"functions",            "functions"},
                {"gpp_bad",              "gpp_bad"},
                {"grid_view",            "grid_view"},
                {"group",                "group"},
                {"group_add",            "group_add"},
                {"groups",               "group"},
                {"health_metrics",       "health_metrics"},
                {"hourglass_top",        "hourglass_top"},
                {"html",                 "html"},
                {"http",                 "http"},
                {"image",                "image"},
                {"import_export",        "import_export"},
                {"input",                "input"},
                {"inventory",            "inventory"},
                {"inventory_2",          "inventory_2"},
                {"key_vertical",         "key_vertical"},
                {"keyboard_alt",         "keyboard_alt"},
                {"keyboard_command_key", "keyboard_command_key"},
                {"keyboard_return",      "keyboard_return"},
                {"lan",                  "lan"},
                {"light_mode",           "light_mode"},
                {"list",                 "list"},
                {"lock_clock",           "lock_clock"},
                {"login",                "login"},
                {"looks_one",            "looks_one"},
                {"mail",                 "mail"},
                {"mail_lock",            "mail_lock"},
                {"manage_search",        "manage_search"},
                {"memory",               "memory"},
                {"menu",                 "menu"},
                {"minimize",             "minimize"},
                {"more_horiz",           "more_horiz"},
                {"more_vert",            "more_vert"},
                {"notes",                "description"},
                {"notifications",        "notifications"},
                {"notifications_off",    "notifications_off"},
                {"numbers",              "numbers"},
                {"open_with",            "open_with"},
                {"palette",              "palette"},
                {"person",               "person"},
                {"person_off",           "person_off"},
                {"photo",                "photo"},
                {"pin",                  "pin"},
                {"play_arrow",           "play_arrow"},
                {"power",                "power"},
                {"preview",              "preview"},
                {"public",               "public"},
                {"push_pin",             "push_pin"},
                {"qr_code_2",            "qr_code_2"},
                {"query_stats",          "query_stats"},
                {"receipt_long",         "receipt_long"},
                {"regular_expression",   "regular_expression"},
                {"remove",               "minimize"},
                {"crop_square",          "crop_square"},
                {"filter_none",          "filter_none"},
                {"replay",               "replay"},
                {"restart_alt",          "restart_alt"},
                {"restore_page",         "restore_page"},
                {"rule",                 "rule"},
                {"save_alt",             "save_alt"},
                {"save_as",              "save_as"},
                {"screen_lock_portrait", "screen_lock_portrait"},
                {"screenshot_monitor",   "screenshot_monitor"},
                {"sd_card",              "sd_card"},
                {"search_off",           "search_off"},
                {"security",             "security"},
                {"share",                "share"},
                {"shield",               "shield"},
                {"short_text",           "description"},
                {"smart_button",         "smart_button"},
                {"south_west",           "south_west"},
                {"space_bar",            "space_bar"},
                {"speed",                "speed"},
                {"star",                 "star"},
                {"sticky_note_2",        "sticky_note_2"},
                {"straighten",           "straighten"},
                {"style",                "style"},
                {"swap_vert",            "swap_vert"},
                {"switch_account",       "switch_account"},
                {"table_view",           "table_view"},
                {"text_fields",          "text_fields"},
                {"text_format",          "text_format"},
                {"title",                "title"},
                {"touch_app",            "touch_app"},
                {"translate",            "translate"},
                {"usb",                  "usb"},
                {"verified",             "verified"},
                {"verified_user",        "verified_user"},
                {"vertical_align_top",   "vertical_align_top"},
                {"view_agenda",          "view_agenda"},
                {"volunteer_activism",   "volunteer_activism"},
                {"window",               "window"},
            };

            /** One entrySymbol() rule: a key read off the entry, and the glyph it names. */
            struct Rule
            {
                const char* key;
                const char* symbol;
            };

            /**
             * URL schemes. A scheme states the protocol instead of guessing at
             * the site, so it outranks the host below. Web schemes are
             * deliberately absent: https says nothing about what the site is.
             *
             * Nothing in the design exercises this table - its SSH entry is
             * stored as the bare host `jump.internal` - so it stands on
             * KeePassXC's own terms rather than on the design: the URL field
             * takes whatever the user types, and an entry whose URL begins
             * `sftp://` or `rdp://` has already said what it is for.
             */
            constexpr Rule SchemeSymbols[] = {
                {"ftp", "folder_shared"},
                {"ftps", "folder_shared"},
                {"mailto", "mail"},
                {"nfs", "folder_shared"},
                {"rdp", "desktop_windows"},
                {"scp", "terminal"},
                {"sftp", "terminal"},
                {"smb", "folder_shared"},
                {"ssh", "terminal"},
                {"telnet", "terminal"},
                {"vnc", "desktop_windows"},
            };

            /**
             * Host labels that say in plain words what the site is, matched
             * against a whole dot-separated label so that "digital.example.com"
             * is not read as a code host. This is reading the URL, not knowing
             * the brand behind it: there is no list of banks or shops here, so
             * a name that states nothing keeps the neutral glyph.
             *
             * A label only earns a rule when the glyph it names can be told
             * apart from the default. `vpn` had one until it turned out that
             * vpn_key and key are a single drawing in the bundled set, which
             * left the rule repainting the very glyph it replaced.
             */
            constexpr Rule HostLabelSymbols[] = {
                {"bank", "account_balance"},
                {"banking", "account_balance"},
                {"billing", "credit_card"},
                {"bitbucket", "code"},
                {"cloud", "cloud"},
                {"git", "code"},
                {"gitea", "code"},
                {"github", "code"},
                {"gitlab", "code"},
                {"imap", "mail"},
                {"jira", "task"},
                {"mail", "mail"},
                {"pay", "credit_card"},
                {"payments", "credit_card"},
                {"smtp", "mail"},
                {"ssh", "terminal"},
                {"webmail", "mail"},
            };

            /** A KeePass icon number and the glyph standing for the same idea. */
            struct IconNumberRule
            {
                int number;
                const char* symbol;
            };

            /**
             * The KeePass icon set is named after the KDE icons it came from,
             * so only the numbers whose picture is plain are mapped; the rest,
             * number 0 included, keep the default. Two kinds are left out on
             * purpose: the numbers that draw a mascot or a vendor mark (Tux,
             * the apple, the Windows W), which say nothing about the entry, and
             * the numbers whose only honest glyph is a key, which would merely
             * repaint the default. The comment on each line is the file name in
             * share/icons/database.
             */
            constexpr IconNumberRule IconNumberSymbols[] = {
                {2, "warning"}, // C02_MessageBox_Warning
                {3, "dns"}, // C03_Server
                {5, "translate"}, // C05_Edu_Languages
                {8, "cable"}, // C08_Socket
                {9, "badge"}, // C09_Identity
                {11, "photo"}, // C11_Camera
                {18, "desktop_windows"}, // C18_Display
                {19, "mail"}, // C19_Mail_Generic
                {21, "calendar_month"}, // C21_KOrganizer, a calendar
                {24, "lan"}, // C24_Connect_Established
                {25, "mail"}, // C25_Folder_Mail
                {26, "save"}, // C26_FileSave
                {29, "terminal"}, // C29_KGPG_Term
                {30, "terminal"}, // C30_Konsole
                {34, "settings"}, // C34_Configure
                {39, "history"}, // C39_History
                {40, "mail"}, // C40_Mail_Find
                {42, "memory"}, // C42_KCMMemory
                {43, "delete"}, // C43_EditTrash
                {44, "sticky_note_2"}, // C44_KNotes
                {46, "help"}, // C46_Help
                {48, "folder"}, // C48_Folder
                {49, "folder_open"}, // C49_Folder_Blue_Open
                // Named as the folder it is: folder_zip draws that same folder
                // here, so the archive would be a claim the artwork cannot make.
                {50, "folder"}, // C50_Folder_Tar
                {51, "lock_open"}, // C51_Decrypted
                {52, "lock"}, // C52_Encrypted
                {53, "check_circle"}, // C53_Apply
                {55, "image"}, // C55_Thumbnail
                {56, "person"}, // C56_KAddressBook
                {57, "description"}, // C57_View_Text
                {59, "code"}, // C59_Package_Development
                {61, "settings"}, // C61_Services
                {66, "account_balance"}, // C66_Money
                {67, "verified"}, // C67_Certificate
            };

            /**
             * The custom attribute an entry may carry to name its own glyph.
             * Honoured only when the name resolves, so a typo falls through to
             * the rules instead of drawing nothing.
             */
            const QString SymbolAttribute = QStringLiteral("KPXC_SYMBOL");

            /** Sizes baked into every QIcon, covering the design's glyph sizes. */
            constexpr int IconSizes[] = {16, 18, 20, 24, 32, 48};

            /** Material's disabled content opacity. */
            constexpr qreal DisabledOpacity = 0.38;

            struct Entry
            {
                QString icon;
                QString path;
            };

            const QHash<QString, Entry>& symbolTable()
            {
                static const QHash<QString, Entry> table = [] {
                    initGeneratedSymbols();
                    QHash<QString, Entry> map;
                    map.reserve(static_cast<int>(std::size(BundledSymbols) + std::size(GeneratedSymbols)));
                    for (const auto& mapping : BundledSymbols) {
                        const QString icon = QString::fromLatin1(mapping.icon);
                        map.insert(QString::fromLatin1(mapping.symbol),
                                   {icon.section(QLatin1Char('/'), -1),
                                    QStringLiteral(":/icons/application/scalable/%1.svg").arg(icon)});
                    }
                    for (const auto& mapping : GeneratedSymbols) {
                        const QString icon = QString::fromLatin1(mapping.icon);
                        map.insert(QString::fromLatin1(mapping.symbol),
                                   {icon, QStringLiteral(":/material/%1.svg").arg(icon)});
                    }
                    return map;
                }();
                return table;
            }

            QHash<QString, QPixmap>& pixmapCache()
            {
                static QHash<QString, QPixmap> cache;
                return cache;
            }

            QHash<QString, QIcon>& iconCache()
            {
                static QHash<QString, QIcon> cache;
                return cache;
            }

            qreal deviceRatio()
            {
                const auto* screen = QGuiApplication::primaryScreen();
                return screen ? screen->devicePixelRatio() : 1.0;
            }

            QString cacheKey(const QString& path, const QColor& tint, int size, qreal dpr)
            {
                return QStringLiteral("%1|%2|%3|%4")
                    .arg(path, tint.isValid() ? tint.name(QColor::HexArgb) : QStringLiteral("-"))
                    .arg(size)
                    .arg(dpr);
            }

            /** Rasterise @p path at @p size logical pixels and recolour it to @p tint. */
            QPixmap render(const QString& path, int size, const QColor& tint, qreal dpr)
            {
                QSvgRenderer renderer(path);
                if (!renderer.isValid()) {
                    return {};
                }

                QPixmap pixmap(QSize(qRound(size * dpr), qRound(size * dpr)));
                pixmap.setDevicePixelRatio(dpr);
                pixmap.fill(Qt::transparent);

                const QRectF box(0, 0, size, size);
                QPainter painter(&pixmap);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
                renderer.render(&painter, box);
                if (tint.isValid()) {
                    // The source SVGs are monochrome, so keeping their alpha and
                    // replacing the colour is enough to re-tint them.
                    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                    painter.fillRect(box, tint);
                }
                painter.end();
                return pixmap;
            }

            QPixmap cachedPixmap(const QString& path, int size, const QColor& tint, qreal dpr)
            {
                const QString key = cacheKey(path, tint, size, dpr);
                auto& cache = pixmapCache();
                const auto it = cache.constFind(key);
                if (it != cache.constEnd()) {
                    return it.value();
                }
                return *cache.insert(key, render(path, size, tint, dpr));
            }

            QPixmap faded(const QPixmap& source, qreal opacity)
            {
                QPixmap result(source.size());
                result.setDevicePixelRatio(source.devicePixelRatio());
                result.fill(Qt::transparent);

                QPainter painter(&result);
                painter.setOpacity(opacity);
                painter.drawPixmap(0, 0, source);
                painter.end();
                return result;
            }

            QIcon buildIcon(const QString& path, const QColor& tint)
            {
                const qreal dpr = deviceRatio();
                QIcon icon;
                for (int size : IconSizes) {
                    const QPixmap pixmap = cachedPixmap(path, size, tint, dpr);
                    if (pixmap.isNull()) {
                        return {};
                    }
                    icon.addPixmap(pixmap, QIcon::Normal);
                    icon.addPixmap(faded(pixmap, DisabledOpacity), QIcon::Disabled);
                }
                return icon;
            }

            QString resourcePath(const QString& name)
            {
                return symbolTable().value(name).path;
            }

            /** The glyph @p key names in @p rules, empty when it names none. */
            template <typename Rules> QString ruleSymbol(const Rules& rules, const QString& key)
            {
                if (key.isEmpty()) {
                    return {};
                }
                for (const auto& rule : rules) {
                    if (key == QLatin1String(rule.key)) {
                        return QString::fromLatin1(rule.symbol);
                    }
                }
                return {};
            }

            /**
             * Whether @p host is an address rather than a name. No top-level
             * domain is numeric, so an all-digits-and-dots host is an IPv4
             * literal; an IPv6 literal is bracketed. A machine addressed
             * directly is a server, which is what the design's `dns` row is.
             */
            bool isAddressLiteral(const QString& host)
            {
                if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']'))) {
                    return true;
                }
                if (!host.contains(QLatin1Char('.'))) {
                    return false;
                }
                for (const QChar character : host) {
                    if (!character.isDigit() && character != QLatin1Char('.')) {
                        return false;
                    }
                }
                return true;
            }

            /**
             * The glyph @p url names, empty when it names none.
             *
             * The URL field holds whatever the user typed, so the scheme is
             * taken as the part before "://" and everything after it is treated
             * as the authority; QUrl would read a bare "host:port" as a scheme.
             */
            QString urlSymbol(const QString& url)
            {
                QString rest = url.trimmed();
                if (rest.isEmpty()) {
                    return {};
                }

                QString scheme;
                const int mark = rest.indexOf(QLatin1String("://"));
                if (mark > 0) {
                    scheme = rest.left(mark).toLower();
                    rest = rest.mid(mark + 3);
                } else if (rest.startsWith(QLatin1String("mailto:"), Qt::CaseInsensitive)) {
                    scheme = QStringLiteral("mailto");
                    rest = rest.section(QLatin1Char(':'), 1);
                }
                const QString byScheme = ruleSymbol(SchemeSymbols, scheme);
                if (!byScheme.isEmpty()) {
                    return byScheme;
                }

                // Authority down to the bare host: drop the path, the user info
                // and the port, in that order.
                rest = rest.section(QLatin1Char('/'), 0, 0).section(QLatin1Char('@'), -1).toLower();
                if (rest.startsWith(QLatin1Char('['))) {
                    const int close = rest.indexOf(QLatin1Char(']'));
                    rest = close > 0 ? rest.left(close + 1) : rest;
                } else {
                    rest = rest.section(QLatin1Char(':'), 0, 0);
                }
                if (rest.isEmpty()) {
                    return {};
                }
                if (isAddressLiteral(rest)) {
                    return QStringLiteral("dns");
                }

                for (const QString& label : rest.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
                    const QString byLabel = ruleSymbol(HostLabelSymbols, label);
                    if (!byLabel.isEmpty()) {
                        return byLabel;
                    }
                }
                return {};
            }

            /** The glyph @p entry's KeePass icon number names, empty when it names none. */
            QString iconNumberSymbol(const ::Entry* entry)
            {
                // A custom icon is a picture only its owner can read, and it
                // leaves the number at whatever it happened to be, so there is
                // nothing honest to derive from the number in that case.
                if (!entry->iconUuid().isNull()) {
                    return {};
                }
                for (const auto& rule : IconNumberSymbols) {
                    if (entry->iconNumber() == rule.number) {
                        return QString::fromLatin1(rule.symbol);
                    }
                }
                return {};
            }
        } // namespace

        QIcon symbol(const QString& name)
        {
            return symbol(name, theme()->color(Role::OnSurfaceVariant));
        }

        QIcon symbol(const QString& name, Role tint)
        {
            return symbol(name, theme()->color(tint));
        }

        QIcon symbol(const QString& name, const QColor& tint)
        {
            const QString path = resourcePath(name);
            if (path.isEmpty()) {
                return {};
            }

            // Size zero marks the multi-resolution icon, keeping it out of the
            // way of the single renderings below.
            const QString key = cacheKey(path, tint, 0, deviceRatio());
            auto& cache = iconCache();
            const auto it = cache.constFind(key);
            if (it != cache.constEnd()) {
                return it.value();
            }
            return *cache.insert(key, buildIcon(path, tint));
        }

        QPixmap pixmap(const QString& name, int size, const QColor& tint)
        {
            const QString path = resourcePath(name);
            if (path.isEmpty() || size <= 0) {
                return {};
            }
            return cachedPixmap(path, size, tint, deviceRatio());
        }

        QString resolve(const QString& name)
        {
            return symbolTable().value(name).icon;
        }

        bool hasSymbol(const QString& name)
        {
            return symbolTable().contains(name);
        }

        // ::Entry is the KeePassXC entry; the Entry above is this file's own
        // symbol table row.
        QString entrySymbol(const ::Entry* entry)
        {
            if (!entry) {
                return defaultEntrySymbol();
            }

            const QString stated = entry->attributes()->value(SymbolAttribute).trimmed();
            if (hasSymbol(stated)) {
                return stated;
            }

            const QString byUrl = urlSymbol(entry->url());
            if (!byUrl.isEmpty()) {
                return byUrl;
            }

            // Entry's own predicate rather than one attribute name, so a row
            // calls an entry a passkey exactly when the rest of the app does.
            if (entry->hasPasskey()) {
                return QStringLiteral("passkey");
            }
            // Weak, but true: an entry with a one-time code and no URL worth
            // reading is at least known to be a second-factor account.
            if (entry->hasTotp()) {
                return QStringLiteral("lock_clock");
            }

            const QString byNumber = iconNumberSymbol(entry);
            if (!byNumber.isEmpty()) {
                return byNumber;
            }

            return defaultEntrySymbol();
        }

        QString defaultEntrySymbol()
        {
            return QStringLiteral("key");
        }

        void clearCache()
        {
            pixmapCache().clear();
            iconCache().clear();
        }

    } // namespace Icons

} // namespace Material
