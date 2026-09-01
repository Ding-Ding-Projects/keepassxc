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

// This file is generated from the design mockup's SHEETS table
// ("KeePassXC Material.dc.html"). It is the design's own wording, section
// order and control values, transcribed rather than paraphrased, so the
// reference sheets can be diffed against the mockup.
//
// Regenerate with utils/generate_sheet_catalogue.mjs; do not hand-edit.

#include "MaterialSheetCatalogue.h"

#include "MaterialSpecSheet.h"

#include <QCoreApplication>

namespace Material
{
    namespace SheetCatalogue
    {
        namespace
        {
            const Row settings_basic_s0[] = {
                {"looks_one", "Start only a single instance of KeePassXC", "", PillKind::On, "On"},
                {"rocket_launch", "Automatically launch KeePassXC at system startup", "", PillKind::Off, "Off"},
                {"minimize", "Minimize window at application startup", "", PillKind::Off, "Off"},
                {"lock_open", "Minimize window after unlocking database", "", PillKind::Off, "Off"},
                {"folder_open", "Remember previously used databases", "Keeps a recent-files list", PillKind::Value, "5 recent files"},
                {"restore_page", "Load previously open databases on startup", "", PillKind::On, "On"},
                {"key", "Remember database key files and security dongles", "", PillKind::On, "On"},
                {"update", "Check for updates at application startup once per week", "", PillKind::Off, "Off"},
                {"science", "Include beta releases when checking for updates", "Requires update checks", PillKind::Off, "Off"},
                {"event_busy", "On database unlock, show entries that will expire within", "", PillKind::Value, "3 days"},
            };
            const Row settings_basic_s1[] = {
                {"save", "Automatically save after every change", "", PillKind::Off, "Off"},
                {"lock", "Automatically save when locking database", "", PillKind::Off, "Off"},
                {"tune", "Automatically save non-data changes when locking database", "", PillKind::On, "On"},
                {"sync", "Automatically reload the database when modified externally", "", PillKind::On, "On"},
                {"backup", "Backup database file before saving", "", PillKind::Off, "Off"},
                {"folder_zip", "Backup destination format", "{DB_FILENAME} and {TIME:<format>} placeholders", PillKind::Mono, "{DB_FILENAME}.old.kdbx"},
                {"drive_file_move", "Use alternative saving method", "Solves problems with Dropbox, Google Drive, GVFS", PillKind::Value, "Temporary file moved into place"},
                {"warning", "Directly write to database file", "Dangerous — no atomic replace", PillKind::Off, "Off"},
            };
            const Row settings_basic_s2[] = {
                {"delete", "Show confirmation before moving entries to recycle bin", "", PillKind::On, "On"},
                {"casino", "Auto-generate password for new entries", "", PillKind::On, "On"},
                {"content_copy", "Copy data on double clicking field in entry view", "", PillKind::Off, "Off"},
                {"link", "Double-click action for URL", "Open in browser · Copy to clipboard · Edit entry", PillKind::Value, "Open entry URL in browser"},
                {"folder", "Use group icon on entry creation", "", PillKind::On, "On"},
                {"minimize", "Minimize when opening a URL", "", PillKind::Off, "Off"},
                {"visibility_off", "Hide window when copying to clipboard", "Minimize or drop to background", PillKind::Value, "Drop to background"},
                {"image", "Favicon download timeout", "", PillKind::Value, "10 sec"},
            };
            const Row settings_basic_s3[] = {
                {"translate", "Language", "Restart program to activate", PillKind::Value, "System default"},
                {"smart_button", "Toolbar button style", "Icon only · Text beside icon · Text under icon", PillKind::Value, "Text beside icon"},
                {"open_with", "Movable toolbar", "", PillKind::Off, "Off"},
                {"format_size", "Font size", "GUI_FontSizeOffset", PillKind::Value, "Default"},
                {"view_agenda", "Show toolbar", "", PillKind::On, "On"},
                {"menu", "Show menubar", "Alt reveals it when hidden", PillKind::On, "On"},
                {"palette", "Show passwords in color", "", PillKind::On, "On"},
                {"code", "Use monospaced font for notes", "", PillKind::On, "On"},
                {"close", "Minimize instead of app exit", "", PillKind::On, "On"},
                {"dock_to_right", "Show a system tray icon", "", PillKind::On, "On"},
                {"style", "Tray icon type", "Monochrome light · Monochrome dark · Colorful", PillKind::Value, "Monochrome light"},
                {"south_west", "Hide window to system tray when minimized", "", PillKind::On, "On"},
                {"compress", "Compact mode", "View menu · reduces row height and padding", PillKind::Off, "Off"},
                {"vertical_align_top", "Always on top", "Ctrl+Shift+A", PillKind::Off, "Off"},
                {"screenshot_monitor", "Allow screen capture", "", PillKind::Off, "Off"},
                {"keyboard_return", "Search waits for Enter", "GUI_SearchWaitForEnter", PillKind::Off, "Off"},
            };
            const Row settings_basic_s4[] = {
                {"restart_alt", "Reset settings to default…", "", PillKind::Action, "Reset"},
                {"file_download", "Import settings…", "", PillKind::Action, "Import"},
                {"file_upload", "Export settings…", "", PillKind::Action, "Export"},
            };
            const Section settings_basic_sections[] = {
                {"Startup", "Config keys SingleInstance … GUI_ShowExpiredEntriesOnDatabaseUnlock.", settings_basic_s0, 10},
                {"File Management", "Saving, reloading and backup behaviour.", settings_basic_s1, 8},
                {"Entry Management", "", settings_basic_s2, 8},
                {"User Interface", "GUI_* configuration keys.", settings_basic_s3, 16},
                {"Settings file", "Config is a plain INI file; these three actions own it.", settings_basic_s4, 3},
            };

            const Row settings_autotype_s0[] = {
                {"title", "Use entry title to match windows for global Auto-Type", "", PillKind::On, "On"},
                {"link", "Use entry URL to match windows for global Auto-Type", "", PillKind::On, "On"},
                {"filter_alt", "Hide expired entries from Auto-Type", "", PillKind::On, "On"},
            };
            const Row settings_autotype_s1[] = {
                {"help", "Always ask before performing Auto-Type", "", PillKind::On, "On"},
                {"fast_forward", "Skip confirmation for main window Auto-Type actions", "", PillKind::Off, "Off"},
                {"lock_clock", "Re-lock previously locked database after performing Auto-Type", "", PillKind::On, "On"},
            };
            const Row settings_autotype_s2[] = {
                {"timer", "Auto-Type delay between keystrokes", "", PillKind::Value, "25 ms"},
                {"hourglass_top", "Auto-Type start delay", "", PillKind::Value, "500 ms"},
                {"keyboard_alt", "Global Auto-Type shortcut", "GlobalAutoTypeKey + modifiers", PillKind::Mono, "Ctrl+Alt+A"},
                {"replay", "Global Auto-Type retype time", "", PillKind::Value, "15 sec"},
                {"desktop_windows", "Experimental: prefer desktop portals (Wayland)", "Persist connection, clipboard mode and restore token", PillKind::Off, "Off"},
            };
            const Section settings_autotype_sections[] = {
                {"Window matching", "", settings_autotype_s0, 3},
                {"Confirmation and locking", "", settings_autotype_s1, 3},
                {"Typing and platform", "AutoTypeDelay, AutoTypeStartDelay, desktop-portal keys.", settings_autotype_s2, 5},
            };

            const Row settings_security_s0[] = {
                {"content_paste_off", "Clear clipboard after", "1–999 sec", PillKind::Value, "10 sec"},
                {"lock_clock", "Lock databases after inactivity of", "10–43200 sec", PillKind::Value, "240 sec"},
                {"search_off", "Clear search query after", "1–1440 min", PillKind::Value, "5 min"},
            };
            const Row settings_security_s1[] = {
                {"fingerprint", "Enable database quick unlock", "Touch ID / Windows Hello", PillKind::On, "On"},
                {"screen_lock_portrait", "Lock databases when session is locked or lid is closed", "", PillKind::On, "On"},
                {"switch_account", "Lock databases when switching user", "", PillKind::On, "On"},
                {"minimize", "Lock databases after minimizing the window", "", PillKind::Off, "Off"},
            };
            const Row settings_security_s2[] = {
                {"password", "Use placeholder for empty password fields", "", PillKind::On, "On"},
                {"visibility_off", "Hide passwords when editing them", "", PillKind::On, "On"},
                {"preview", "Hide passwords in the entry preview panel", "", PillKind::Off, "Off"},
                {"timer", "Hide TOTP in the entry preview panel", "", PillKind::Off, "Off"},
                {"sticky_note_2", "Hide notes in the entry preview panel", "", PillKind::Off, "Off"},
                {"rule", "Minimum database password quality", "Security_DatabasePasswordMinimumQuality", PillKind::Value, "Good (75 bits)"},
            };
            const Row settings_security_s3[] = {
                {"public", "Use DuckDuckGo service to download website icons", "Falls back when a site serves no favicon", PillKind::Off, "Off"},
            };
            const Section settings_security_sections[] = {
                {"Timeouts", "", settings_security_s0, 3},
                {"Lock Options", "", settings_security_s1, 4},
                {"Convenience", "", settings_security_s2, 6},
                {"Privacy", "", settings_security_s3, 1},
            };

            const Row settings_browser_s0[] = {
                {"power", "Enable browser integration", "Native messaging host", PillKind::On, "On"},
                {"checklist", "Browsers", "Chrome, Chromium, Firefox, Edge, Brave, Vivaldi, Tor Browser", PillKind::Value, "4 installed"},
                {"notifications", "Show notification when credentials are requested", "", PillKind::On, "On"},
                {"lan", "Support KeePassXC-Browser proxy application", "", PillKind::On, "On"},
                {"folder_open", "Use a custom proxy location", "", PillKind::Off, "Off"},
                {"terminal", "Custom proxy location", "Browser_CustomProxyLocation", PillKind::Mono, "/usr/bin/keepassxc-proxy"},
                {"update", "Update native messaging manifest on version change", "", PillKind::On, "On"},
                {"web", "Use a custom browser configuration location", "Snap/Flatpak sandboxes", PillKind::Off, "Off"},
            };
            const Row settings_browser_s1[] = {
                {"star", "Return only best-matching credentials", "", PillKind::Off, "Off"},
                {"lock_open", "Unlock the database when a request arrives", "", PillKind::On, "On"},
                {"http", "Match URL schemes", "https:// must equal https://", PillKind::On, "On"},
                {"event_busy", "Allow expired credentials to be returned", "", PillKind::Off, "Off"},
                {"database", "Search in all open databases", "", PillKind::Off, "Off"},
                {"label", "Support KeePassHTTP KPH: attributes", "", PillKind::On, "On"},
                {"cloud_download", "Allow the browser to request all database entries", "", PillKind::Off, "Off"},
                {"check_circle", "Always allow access without asking", "", PillKind::Off, "Off"},
                {"edit", "Always allow updating existing credentials", "", PillKind::Off, "Off"},
                {"vpn_key", "HTTP Basic Auth permission", "Browser_HttpAuthPermission", PillKind::Off, "Off"},
                {"passkey", "Allow passkeys on localhost", "", PillKind::Off, "Off"},
            };
            const Section settings_browser_sections[] = {
                {"Connection", "", settings_browser_s0, 8},
                {"Credential handling", "", settings_browser_s1, 11},
            };

            const Row settings_sshagent_s0[] = {
                {"power", "Enable SSH Agent integration", "", PillKind::On, "On"},
                {"memory", "Use OpenSSH for Windows instead of Pageant", "", PillKind::Off, "Off"},
                {"window", "Use Pageant", "PuTTY-compatible agent", PillKind::Off, "Off"},
                {"cable", "SSH_AUTH_SOCK override", "", PillKind::Mono, "$XDG_RUNTIME_DIR/ssh-agent.socket"},
                {"usb", "Security key provider override", "FIDO/PIV middleware", PillKind::Mono, "/usr/lib/libsk-libfido2.so"},
                {"cleaning_services", "Clear SSH Agent", "Removes every identity KeePassXC added", PillKind::Action, "Clear now"},
            };
            const Section settings_sshagent_sections[] = {
                {"Agent", "", settings_sshagent_s0, 6},
            };

            const Row settings_keeshare_s0[] = {
                {"login", "Allow import (foreign shares)", "", PillKind::On, "On"},
                {"logout", "Allow export (own shares)", "", PillKind::Off, "Off"},
                {"notifications_off", "Quiet success notifications", "", PillKind::Off, "Off"},
                {"badge", "Signer name", "Written into every exported container", PillKind::Value, "codingmachineedge"},
                {"key", "Signing certificate", "Own certificate and private key", PillKind::Action, "Regenerate"},
                {"verified_user", "Trusted foreign certificates", "", PillKind::Value, "2 trusted"},
            };
            const Section settings_keeshare_sections[] = {
                {"Sharing", "", settings_keeshare_s0, 6},
            };

            const Row settings_shortcuts_s0[] = {
                {"add", "New Database", "", PillKind::Mono, "Ctrl+Shift+N"},
                {"folder_open", "Open Database", "", PillKind::Mono, "Ctrl+O"},
                {"save", "Save Database", "", PillKind::Mono, "Ctrl+S"},
                {"save_as", "Save Database As", "", PillKind::Mono, "Ctrl+Shift+S"},
                {"close", "Close Database", "", PillKind::Mono, "Ctrl+W"},
                {"lock", "Lock Database", "", PillKind::Mono, "Ctrl+L"},
                {"lock", "Lock All Databases", "", PillKind::Mono, "Ctrl+Shift+L"},
                {"tune", "Database Settings", "", PillKind::Mono, "Ctrl+Shift+,"},
                {"health_metrics", "Database Reports", "", PillKind::Mono, "Ctrl+Shift+R"},
                {"logout", "Quit", "", PillKind::Mono, "Ctrl+Q"},
            };
            const Row settings_shortcuts_s1[] = {
                {"add", "New Entry", "", PillKind::Mono, "Ctrl+N"},
                {"edit", "Edit Entry", "", PillKind::Mono, "Ctrl+E"},
                {"content_copy", "Clone Entry", "", PillKind::Mono, "Ctrl+D"},
                {"delete", "Delete Entry", "", PillKind::Mono, "Del"},
                {"restore", "Restore Entry", "", PillKind::Mono, "Ctrl+R"},
                {"arrow_upward", "Move Entry Up", "", PillKind::Mono, "Alt+Up"},
                {"arrow_downward", "Move Entry Down", "", PillKind::Mono, "Alt+Down"},
                {"person", "Copy Username", "", PillKind::Mono, "Ctrl+B"},
                {"password", "Copy Password", "", PillKind::Mono, "Ctrl+C"},
                {"link", "Copy URL", "", PillKind::Mono, "Ctrl+U"},
                {"title", "Copy Title", "", PillKind::Mono, "Ctrl+I"},
                {"timer", "Copy TOTP", "", PillKind::Mono, "Ctrl+T"},
                {"key", "Copy Password and TOTP", "", PillKind::Mono, "Ctrl+Y"},
                {"visibility", "Show TOTP", "", PillKind::Mono, "Ctrl+Shift+T"},
                {"open_in_new", "Open URL", "", PillKind::Mono, "Ctrl+Shift+U"},
                {"image", "Download Favicon", "", PillKind::Mono, "Ctrl+Shift+D"},
                {"keyboard", "Perform Auto-Type (entry default)", "", PillKind::Mono, "Ctrl+Shift+V"},
                {"terminal", "SSH Agent: add key", "", PillKind::Mono, "Ctrl+H"},
                {"terminal", "SSH Agent: remove key", "", PillKind::Mono, "Ctrl+Shift+H"},
            };
            const Row settings_shortcuts_s2[] = {
                {"vertical_align_top", "Always on Top", "", PillKind::Mono, "Ctrl+Shift+A"},
                {"person_off", "Hide Usernames", "", PillKind::Mono, "Ctrl+Shift+B"},
                {"visibility_off", "Hide Passwords", "", PillKind::Mono, "Ctrl+Shift+C"},
                {"settings", "Application Settings", "", PillKind::Mono, "Ctrl+,"},
                {"help", "Keyboard Shortcuts guide", "", PillKind::Mono, "Ctrl+/"},
                {"search", "Focus search", "", PillKind::Mono, "Ctrl+F"},
                {"bolt", "All actions (this build)", "", PillKind::Mono, "Ctrl+Shift+F"},
            };
            const Section settings_shortcuts_sections[] = {
                {"Database", "", settings_shortcuts_s0, 10},
                {"Entries", "", settings_shortcuts_s1, 19},
                {"View and help", "", settings_shortcuts_s2, 7},
            };

            const Row settings_gendefaults_s0[] = {
                {"straighten", "Length", "1–999", PillKind::Value, "20"},
                {"text_fields", "Lower-case letters a-z", "", PillKind::On, "On"},
                {"text_fields", "Upper-case letters A-Z", "", PillKind::On, "On"},
                {"pin", "Numbers 0-9", "", PillKind::On, "On"},
                {"emoji_symbols", "Special characters / * + &", "", PillKind::Off, "Off"},
                {"data_object", "Braces { [ ( ) ] }", "", PillKind::Off, "Off"},
                {"more_horiz", "Punctuation . , : ;", "", PillKind::Off, "Off"},
                {"format_quote", "Quotes \" '", "", PillKind::Off, "Off"},
                {"remove", "Dashes and slashes \\ / | _ -", "", PillKind::Off, "Off"},
                {"functions", "Math symbols < > * + ! ? =", "", PillKind::Off, "Off"},
                {"language", "Logograms # $ % && @ ^ ` ~", "", PillKind::Off, "Off"},
                {"translate", "Extended ASCII", "", PillKind::Off, "Off"},
                {"add", "Also choose from (additional characters)", "", PillKind::Mono, "(empty)"},
                {"block", "Do not include (excluded characters)", "Hex button adds every non-hex letter", PillKind::Mono, "(empty)"},
                {"blur_on", "Exclude look-alike characters", "0 1 l I O | B 8 G 6", PillKind::On, "On"},
                {"checklist", "Pick characters from every group", "", PillKind::On, "On"},
                {"tune", "Advanced mode", "Reveals additional/excluded fields", PillKind::Off, "Off"},
            };
            const Row settings_gendefaults_s1[] = {
                {"numbers", "Word count", "1–40", PillKind::Value, "6"},
                {"space_bar", "Word separator", "", PillKind::Mono, "(space)"},
                {"menu_book", "Wordlist", "Bundled EFF long list, or a custom file", PillKind::Value, "eff_large.wordlist"},
                {"text_format", "Word case", "lower · UPPER · Title Case", PillKind::Value, "lower case"},
                {"category", "Generator type", "Password or Passphrase", PillKind::Value, "Password"},
            };
            const Section settings_gendefaults_sections[] = {
                {"Password mode", "", settings_gendefaults_s0, 17},
                {"Passphrase mode", "", settings_gendefaults_s1, 5},
            };

            const Page settings_pages[] = {
                {"basic", "settings", "General — Basic Settings", "General · Basic Settings", "Every option on the Basic Settings tab of Application Settings, in Material form.", settings_basic_sections, 5},
                {"autotype", "keyboard", "General — Auto-Type", "General · Auto-Type", "Window matching, confirmation and platform behaviour for Auto-Type.", settings_autotype_sections, 3},
                {"security", "security", "Security", "Security", "Timeouts, lock options, convenience and privacy — the Security tab of Application Settings.", settings_security_sections, 4},
                {"browser", "extension", "Browser Integration", "Browser Integration", "Every Browser_* configuration key, as exposed by BrowserSettingsWidget.", settings_browser_sections, 2},
                {"sshagent", "terminal", "SSH Agent", "SSH Agent", "SSHAgent_* keys. Keys are published to the agent only while the database is unlocked.", settings_sshagent_sections, 1},
                {"keeshare", "share", "KeeShare", "KeeShare", "Share groups between databases as signed containers.", settings_keeshare_sections, 1},
                {"shortcuts", "keyboard_command_key", "Shortcuts", "Shortcuts", "Every action in the application, with its editable key binding (ShortcutSettingsPage).", settings_shortcuts_sections, 3},
                {"gendefaults", "casino", "Password Generator defaults", "Password Generator defaults", "PasswordGenerator_* keys — the state the generator opens with.", settings_gendefaults_sections, 2},
            };

            const Row database_general_s0[] = {
                {"badge", "Database name", "", PillKind::Value, "Personal"},
                {"notes", "Description", "", PillKind::Value, "Everyday accounts"},
                {"person", "Default username", "Pre-filled on every new entry", PillKind::Value, "codingmachine.edge"},
                {"palette", "Custom database color", "Shown on the tab and tray icon", PillKind::Value, "KeePassXC blue"},
                {"folder_open", "Database file", "", PillKind::Mono, "~/Vaults/Personal.kdbx"},
            };
            const Row database_general_s1[] = {
                {"history", "Maximum number of history items per entry", "", PillKind::Value, "10"},
                {"sd_card", "Maximum amount of history size per entry", "", PillKind::Value, "6 MiB"},
                {"rule", "Use recycle bin", "Deleted entries go to a group instead of vanishing", PillKind::On, "On"},
                {"delete_sweep", "Empty recycle bin", "", PillKind::Action, "Empty now"},
            };
            const Row database_general_s2[] = {
                {"search", "Enable compression", "Recommended; KDBX gzip", PillKind::On, "On"},
                {"key", "Auto-Type default sequence for this database", "", PillKind::Mono, "{USERNAME}{TAB}{PASSWORD}{ENTER}"},
            };
            const Section database_general_sections[] = {
                {"Database meta data", "", database_general_s0, 5},
                {"History settings", "Per-entry history retention.", database_general_s1, 4},
                {"Additional database settings", "", database_general_s2, 2},
            };

            const Row database_credentials_s0[] = {
                {"password", "Database password", "Entered twice, quality-checked", PillKind::Good, "Set · 128 bits"},
                {"rule", "Minimum quality gate", "Refuses weak database passwords", PillKind::Value, "Good"},
                {"casino", "Generate a passphrase for this database", "", PillKind::Action, "Open generator"},
            };
            const Row database_credentials_s1[] = {
                {"description", "Key file", "A second factor stored on disk or removable media", PillKind::Value, "Not set"},
                {"add", "Generate a new 32-byte key file", "", PillKind::Action, "Generate"},
                {"folder_open", "Browse for an existing key file", "Legacy XML key files are still accepted", PillKind::Action, "Browse…"},
            };
            const Row database_credentials_s2[] = {
                {"usb", "YubiKey / OnlyKey challenge-response", "HMAC-SHA1 slot, optionally touch-required", PillKind::Good, "Slot 2 · touch"},
                {"refresh", "Refresh hardware tokens", "", PillKind::Action, "Refresh"},
                {"help", "Add a second hardware key", "Keep a backup key enrolled", PillKind::Action, "Add"},
            };
            const Section database_credentials_sections[] = {
                {"Password", "", database_credentials_s0, 3},
                {"Key file", "", database_credentials_s1, 3},
                {"Hardware key", "", database_credentials_s2, 3},
            };

            const Row database_encryption_s0[] = {
                {"description", "Format", "KDBX 4.1 · 4.0 · 3.1 (KeePass 2 compatible)", PillKind::Value, "KDBX 4.1"},
                {"warning", "Downgrading loses KDBX4 features", "Custom data, Argon2, per-entry attributes", PillKind::Off, "Not downgraded"},
            };
            const Row database_encryption_s1[] = {
                {"memory", "Key derivation function", "Argon2id · Argon2d · AES-KDF", PillKind::Value, "Argon2id"},
                {"speed", "Transform rounds", "", PillKind::Value, "29"},
                {"sd_card", "Memory usage", "", PillKind::Value, "64 MiB"},
                {"account_tree", "Parallelism", "", PillKind::Value, "4 threads"},
                {"timer", "Benchmark 1-second delay", "", PillKind::Action, "Benchmark"},
            };
            const Row database_encryption_s2[] = {
                {"lock", "Cipher", "AES-256 · Twofish-256 · ChaCha20", PillKind::Value, "AES-256"},
                {"shield", "Inner stream cipher", "ChaCha20 protects in-memory values", PillKind::Value, "ChaCha20"},
            };
            const Section database_encryption_sections[] = {
                {"Database format", "", database_encryption_s0, 2},
                {"Key derivation", "Tune until unlocking takes about one second on this machine.", database_encryption_s1, 5},
                {"Encryption algorithm", "", database_encryption_s2, 2},
            };

            const Row database_remote_s0[] = {
                {"label", "Target name", "", PillKind::Value, "nas-webdav"},
                {"download", "Download command", "", PillKind::Mono, "curl -u $USER -o {TEMP_DATABASE} https://…"},
                {"upload", "Upload command", "", PillKind::Mono, "curl -T {TEMP_DATABASE} -u $USER https://…"},
                {"input", "Input for command", "Password piped to stdin", PillKind::Value, "From credential prompt"},
                {"sync", "Sync on database save", "", PillKind::On, "On"},
                {"play_arrow", "Test this target now", "", PillKind::Action, "Test"},
            };
            const Section database_remote_sections[] = {
                {"Sync targets", "", database_remote_s0, 6},
            };

            const Row database_dbbrowser_s0[] = {
                {"link", "Stored browser connections", "Key name, shared secret, created date", PillKind::Value, "3 connections"},
                {"delete", "Remove selected connection", "", PillKind::Action, "Remove"},
                {"cleaning_services", "Remove all stored permissions from entries", "", PillKind::Action, "Clear all"},
                {"block", "Never ask before accessing credentials", "", PillKind::Off, "Off"},
                {"folder_off", "Exclude a group from browser search", "Set on the group, shown here", PillKind::Value, "Servers & SSH excluded"},
            };
            const Section database_dbbrowser_sections[] = {
                {"Rules", "", database_dbbrowser_s0, 5},
            };

            const Row database_dbkeeshare_s0[] = {
                {"folder_shared", "Share group", "Inactive · Import from · Export to · Synchronize with", PillKind::Value, "Synchronize with"},
                {"description", "Container path", "", PillKind::Mono, "~/Share/team.kdbx"},
                {"password", "Container password", "", PillKind::Value, "Set"},
                {"verified", "Signature status", "Signed by a trusted certificate", PillKind::Good, "Trusted"},
            };
            const Section database_dbkeeshare_sections[] = {
                {"Shared groups", "", database_dbkeeshare_s0, 4},
            };

            const Row database_maintenance_s0[] = {
                {"image", "Custom icons in this database", "", PillKind::Value, "12 icons"},
                {"delete_sweep", "Delete unused icons", "", PillKind::Action, "Purge"},
                {"cloud_download", "Download all favicons for this group", "", PillKind::Action, "Download"},
            };
            const Row database_maintenance_s1[] = {
                {"cleaning_services", "Purge deleted-object records", "Reduces merge metadata", PillKind::Action, "Purge"},
                {"compress", "Rebuild database file", "Rewrites with the current KDF settings", PillKind::Action, "Rebuild"},
            };
            const Section database_maintenance_sections[] = {
                {"Custom icons", "", database_maintenance_s0, 3},
                {"Cleanup", "", database_maintenance_s1, 2},
            };

            const Page database_pages[] = {
                {"general", "settings", "General", "Database · General", "Metadata, history and the recycle bin — DatabaseSettingsWidgetGeneral.", database_general_sections, 3},
                {"credentials", "vpn_key", "Security — Database Credentials", "Security · Database Credentials", "The three key components: password, key file and hardware key (KeyComponentWidget).", database_credentials_sections, 3},
                {"encryption", "enhanced_encryption", "Security — Encryption", "Security · Encryption", "DatabaseSettingsWidgetEncryption — format, KDF and cipher.", database_encryption_sections, 3},
                {"remote", "cloud_sync", "Remote Sync", "Remote Sync", "DatabaseSettingsWidgetRemote — sync through an arbitrary download/upload command.", database_remote_sections, 1},
                {"dbbrowser", "extension", "Browser Integration", "Database · Browser Integration", "Per-database browser rules — DatabaseSettingsWidgetBrowser.", database_dbbrowser_sections, 1},
                {"dbkeeshare", "share", "KeeShare", "Database · KeeShare", "Which groups of this database are shared, and how.", database_dbkeeshare_sections, 1},
                {"maintenance", "construction", "Maintenance", "Maintenance", "DatabaseSettingsWidgetMaintenance — custom icons and unused data.", database_maintenance_sections, 2},
            };

            const Row editor_entry_s0[] = {
                {"title", "Title", "", PillKind::Value, "GitHub"},
                {"person", "Username", "Inherits the database default username", PillKind::Value, "codingmachineedge"},
                {"password", "Password", "Repeat field must match; quality meter below", PillKind::Mono, "••••••••••••••••"},
                {"casino", "Generate / choose a preset", "Password, passphrase or an advanced recipe", PillKind::Action, "Generate"},
                {"link", "URL", "Extra URLs live as KP2A_URL attributes", PillKind::Value, "https://github.com"},
                {"label", "Tags", "Comma separated, autocompleted from the database", PillKind::Value, "work, 2fa"},
                {"event_busy", "Expires", "Preset offsets: 1 day … 1 year", PillKind::Off, "Never"},
                {"sticky_note_2", "Notes", "Monospaced when that option is on", PillKind::Value, "3 lines"},
            };
            const Row editor_entry_s1[] = {
                {"timer", "TOTP secret", "Base32, or scanned from an otpauth:// URI", PillKind::Good, "Configured"},
                {"tune", "Algorithm, digits and step", "SHA-1 / SHA-256 / SHA-512, 6–8 digits, 30 s", PillKind::Value, "SHA-1 · 6 · 30 s"},
                {"qr_code_2", "Show QR code", "Export the secret to a phone", PillKind::Action, "Show"},
                {"delete", "Remove TOTP from this entry", "", PillKind::Action, "Remove"},
            };
            const Section editor_entry_sections[] = {
                {"Fields", "", editor_entry_s0, 8},
                {"One-time passwords", "TotpSetupDialog / TotpDialog / TotpExportSettingsDialog.", editor_entry_s1, 4},
            };

            const Row editor_advanced_s0[] = {
                {"add", "Add attribute", "", PillKind::Action, "Add"},
                {"key", "api-token", "Protected value", PillKind::Good, "Protected"},
                {"label", "recovery-email", "", PillKind::Value, "me@proton.me"},
                {"visibility", "Reveal / protect selected attribute", "", PillKind::Action, "Toggle"},
                {"functions", "Field references", "{REF:P@I:UUID} between entries", PillKind::Value, "1 reference"},
            };
            const Row editor_advanced_s1[] = {
                {"attach_file", "recovery-codes.txt", "1.2 KB", PillKind::Value, "Preview"},
                {"add", "Add files", "", PillKind::Action, "Add"},
                {"save_alt", "Save selected to disk", "", PillKind::Action, "Save"},
                {"open_in_new", "Open with the system handler", "Temporary file, removed on close", PillKind::Action, "Open"},
            };
            const Row editor_advanced_s2[] = {
                {"format_color_text", "Foreground colour", "", PillKind::Value, "Default"},
                {"format_color_fill", "Background colour", "", PillKind::Value, "Default"},
            };
            const Section editor_advanced_sections[] = {
                {"Additional attributes", "EntryAttributesModel. Values can be protected and referenced.", editor_advanced_s0, 5},
                {"Attachments", "EntryAttachmentsWidget — preview, open, save, remove.", editor_advanced_s1, 4},
                {"Foreground and background", "", editor_advanced_s2, 2},
            };

            const Row editor_icon_s0[] = {
                {"grid_view", "Bundled icon", "68 standard KeePass icons", PillKind::Value, "Icon 12"},
                {"image", "Custom icon", "Stored in the database, shared by entries", PillKind::Value, "2 in use"},
                {"cloud_download", "Download favicon from the URL", "Uses the favicon timeout and optional DuckDuckGo fallback", PillKind::Action, "Download"},
                {"add_photo_alternate", "Add a custom icon from file", "", PillKind::Action, "Add"},
                {"delete", "Delete unused custom icons", "", PillKind::Action, "Purge"},
            };
            const Section editor_icon_sections[] = {
                {"Icon source", "", editor_icon_s0, 5},
            };

            const Row editor_eautotype_s0[] = {
                {"power", "Enable Auto-Type for this entry", "", PillKind::On, "On"},
                {"keyboard", "Inherit the default sequence", "", PillKind::On, "On"},
                {"code", "Custom sequence", "", PillKind::Mono, "{USERNAME}{TAB}{PASSWORD}{ENTER}"},
                {"functions", "Placeholders", "{TOTP} {URL} {CLEARFIELD} {DELAY 500} {PICKCHARS}", PillKind::Value, "Documented"},
            };
            const Row editor_eautotype_s1[] = {
                {"window", "Window title pattern", "Wildcards and //regex// accepted", PillKind::Mono, "*GitHub*"},
                {"code", "Sequence for this window", "", PillKind::Mono, "{USERNAME}{TAB}{PASSWORD}"},
                {"add", "Add association", "", PillKind::Action, "Add"},
                {"remove", "Remove association", "", PillKind::Action, "Remove"},
            };
            const Section editor_eautotype_sections[] = {
                {"Sequence", "", editor_eautotype_s0, 4},
                {"Window associations", "AutoTypeAssociationsModel.", editor_eautotype_s1, 4},
            };

            const Row editor_ebrowser_s0[] = {
                {"block", "Skip Auto-Submit for this entry", "", PillKind::Off, "Off"},
                {"visibility_off", "Hide this entry from the browser extension", "", PillKind::Off, "Off"},
                {"rule", "Only return this entry for exact URL matches", "", PillKind::Off, "Off"},
                {"add_link", "Additional URLs", "Matched alongside the main URL", PillKind::Value, "2 URLs"},
            };
            const Row editor_ebrowser_s1[] = {
                {"passkey", "Stored passkey", "Relying party, user handle, credential ID", PillKind::Good, "github.com"},
                {"file_upload", "Export passkey", "", PillKind::Action, "Export"},
                {"file_download", "Import passkey into this entry", "", PillKind::Action, "Import"},
                {"delete", "Remove passkey from entry", "", PillKind::Action, "Remove"},
            };
            const Section editor_ebrowser_sections[] = {
                {"Behaviour", "", editor_ebrowser_s0, 4},
                {"Passkey", "BrowserPasskeys / PasskeyExporter.", editor_ebrowser_s1, 4},
            };

            const Row editor_essh_s0[] = {
                {"power", "Add key to agent when database is opened", "", PillKind::On, "On"},
                {"logout", "Remove key from agent when database is closed", "", PillKind::On, "On"},
                {"timer", "Require user confirmation / lifetime limit", "", PillKind::Value, "300 sec"},
                {"attach_file", "Private key source", "Attachment or external file", PillKind::Value, "id_ed25519 (attachment)"},
                {"fingerprint", "Fingerprint", "", PillKind::Mono, "SHA256:9r4…Kt8"},
                {"content_copy", "Copy public key", "", PillKind::Action, "Copy"},
            };
            const Section editor_essh_sections[] = {
                {"Key", "", editor_essh_s0, 6},
            };

            const Row editor_properties_s0[] = {
                {"fingerprint", "UUID", "", PillKind::Mono, "a91f04c7-2b8e-4f10-9d6a-77c3e1b0f5aa"},
                {"schedule", "Created", "", PillKind::Value, "2019-04-02 11:09"},
                {"edit", "Last modified", "", PillKind::Value, "2026-07-28 07:41"},
                {"visibility", "Last accessed", "", PillKind::Value, "2026-07-28 09:12"},
                {"event_busy", "Expires", "", PillKind::Off, "Never"},
                {"numbers", "Usage count", "", PillKind::Value, "412"},
                {"folder", "Location", "", PillKind::Value, "Personal / Work"},
            };
            const Row editor_properties_s1[] = {
                {"data_object", "Plugin data", "KDBX4 custom data attached to this entry", PillKind::Value, "2 keys"},
            };
            const Section editor_properties_sections[] = {
                {"Identity and times", "", editor_properties_s0, 7},
                {"Custom data", "", editor_properties_s1, 1},
            };

            const Row editor_ehistory_s0[] = {
                {"history", "10 history items", "Oldest 2019-04-02, newest 2026-07-28", PillKind::Value, "10 items"},
                {"difference", "Show a revision", "Opens it read-only", PillKind::Action, "Show"},
                {"restore", "Restore a revision", "Recorded as a new revision", PillKind::Action, "Restore"},
                {"delete", "Delete a revision", "", PillKind::Action, "Delete"},
                {"delete_sweep", "Delete all history for this entry", "", PillKind::Action, "Delete all"},
            };
            const Section editor_ehistory_sections[] = {
                {"Revisions", "", editor_ehistory_s0, 5},
            };

            const Page editor_pages[] = {
                {"entry", "edit_note", "Entry", "Edit entry · Entry", "EditEntryWidgetMain — the fields every entry carries.", editor_entry_sections, 2},
                {"advanced", "tune", "Advanced", "Edit entry · Advanced", "EditEntryWidgetAdvanced — custom attributes, attachments and colours.", editor_advanced_sections, 3},
                {"icon", "photo", "Icon", "Edit entry · Icon", "EditWidgetIcons — the 68 bundled KeePass icons plus custom icons.", editor_icon_sections, 1},
                {"eautotype", "keyboard", "Auto-Type", "Edit entry · Auto-Type", "EditEntryWidgetAutoType — per-entry sequences and window associations.", editor_eautotype_sections, 2},
                {"ebrowser", "extension", "Browser Integration", "Edit entry · Browser Integration", "BrowserEntryConfig — per-entry browser behaviour and passkeys.", editor_ebrowser_sections, 2},
                {"essh", "terminal", "SSH Agent", "Edit entry · SSH Agent", "EditEntryWidgetSSHAgent — publish a private key from this entry.", editor_essh_sections, 1},
                {"properties", "info", "Properties", "Edit entry · Properties", "EditWidgetProperties — the UUID and timestamps KDBX keeps.", editor_properties_sections, 2},
                {"ehistory", "history", "History", "Edit entry · History", "EntryHistoryModel — every previous version of this entry.", editor_ehistory_sections, 1},
            };

            const Row help_guides_s0[] = {
                {"rocket_launch", "Getting Started guide", "", PillKind::Action, "Open"},
                {"menu_book", "User Guide", "", PillKind::Action, "Open"},
                {"keyboard_command_key", "Keyboard shortcuts", "KeyboardShortcuts.adoc", PillKind::Action, "Open"},
                {"public", "Online help", "keepassxc.org/docs", PillKind::Action, "Open"},
            };
            const Row help_guides_s1[] = {
                {"update", "Check for updates", "Weekly check, betas optional", PillKind::Action, "Check now"},
                {"volunteer_activism", "Donate", "", PillKind::Action, "Open"},
                {"bug_report", "Report a bug", "Pre-fills version and platform", PillKind::Action, "Open"},
                {"info", "About KeePassXC", "Version, Qt build, enabled extensions, contributors", PillKind::Action, "Open"},
            };
            const Section help_guides_sections[] = {
                {"Documentation", "", help_guides_s0, 4},
                {"Project", "", help_guides_s1, 4},
            };

            const Row help_cli_s0[] = {
                {"add", "db-create", "Create a database with password/key-file/YubiKey", PillKind::Mono, "db-create"},
                {"edit", "db-edit", "Change credentials or KDF settings", PillKind::Mono, "db-edit"},
                {"info", "db-info", "Cipher, KDF, transform rounds, entry count", PillKind::Mono, "db-info"},
                {"folder_open", "open / close / exit", "Interactive shell session", PillKind::Mono, "open"},
                {"merge", "merge", "Merge another database in", PillKind::Mono, "merge"},
                {"file_upload", "export", "XML or CSV to stdout", PillKind::Mono, "export"},
                {"file_download", "import", "Build a database from XML", PillKind::Mono, "import"},
                {"analytics", "analyze", "Check against an offline HIBP list", PillKind::Mono, "analyze"},
            };
            const Row help_cli_s1[] = {
                {"add", "add / edit / rm", "Create, change, remove an entry", PillKind::Mono, "add"},
                {"folder", "mkdir / rmdir", "Create or remove a group", PillKind::Mono, "mkdir"},
                {"drive_file_move", "mv", "Move an entry", PillKind::Mono, "mv"},
                {"list", "ls", "List a group recursively", PillKind::Mono, "ls"},
                {"visibility", "show", "Print attributes, TOTP and protected values", PillKind::Mono, "show"},
                {"content_paste", "clip", "Copy a field to the clipboard for N seconds", PillKind::Mono, "clip"},
                {"search", "search", "Same search syntax as the GUI", PillKind::Mono, "search"},
                {"attach_file", "attachment-import / -export / -rm", "", PillKind::Mono, "attachment-*"},
            };
            const Row help_cli_s2[] = {
                {"casino", "generate", "Character-class password generator", PillKind::Mono, "generate"},
                {"casino", "diceware", "Passphrase from a wordlist", PillKind::Mono, "diceware"},
                {"rule", "estimate", "zxcvbn-style entropy estimate", PillKind::Mono, "estimate"},
                {"help", "help", "Per-command usage", PillKind::Mono, "help"},
            };
            const Section help_cli_sections[] = {
                {"Database", "", help_cli_s0, 8},
                {"Entries and groups", "", help_cli_s1, 8},
                {"Generators and helpers", "", help_cli_s2, 4},
            };

            const Row help_search_syntax_s0[] = {
                {"title", "title:", "", PillKind::Mono, "title:github"},
                {"person", "user: / u:", "", PillKind::Mono, "user:ops@"},
                {"link", "url:", "", PillKind::Mono, "url:*.hk"},
                {"sticky_note_2", "notes:", "", PillKind::Mono, "notes:recovery"},
                {"label", "tag:", "", PillKind::Mono, "tag:2fa"},
                {"data_object", "attr: / attachment:", "", PillKind::Mono, "attr:api-token"},
                {"folder", "group: / g:", "", PillKind::Mono, "group:Servers"},
            };
            const Row help_search_syntax_s1[] = {
                {"rule", "Exact and negated terms", "Quote to require, minus to exclude", PillKind::Mono, "\"root@\" -archive"},
                {"regular_expression", "Regular expressions", "Wrapped in slashes, or built in the builder", PillKind::Mono, "/^(admin|root)@/"},
                {"text_fields", "Case sensitivity", "Off unless the term contains capitals", PillKind::Value, "Smart case"},
                {"event_busy", "Special filters", "is:expired, is:weak, is:reused", PillKind::Mono, "is:expired"},
            };
            const Section help_search_syntax_sections[] = {
                {"Field modifiers", "", help_search_syntax_s0, 7},
                {"Operators", "", help_search_syntax_s1, 4},
            };

            const Page help_pages[] = {
                {"guides", "menu_book", "Guides and support", "Guides and support", "Every item on the Help menu.", help_guides_sections, 2},
                {"cli", "terminal", "Command line (keepassxc-cli)", "keepassxc-cli", "Every command shipped by the CLI, from src/cli.", help_cli_sections, 3},
                {"search-syntax", "manage_search", "Search syntax", "Search syntax", "SearchHelpWidget — the modifiers the search bar accepts, all also usable through the regex builder.", help_search_syntax_sections, 2},
            };

            const Row tools_dbflows_s0[] = {
                {"badge", "Name and description", "Page 1: metadata", PillKind::Action, "Open"},
                {"enhanced_encryption", "Encryption settings", "Page 2: KDF, cipher, benchmark", PillKind::Action, "Open"},
                {"vpn_key", "Database credentials", "Page 3: password, key file, hardware key", PillKind::Action, "Open"},
                {"save", "Choose a file location", "Page 4: where the .kdbx lands", PillKind::Action, "Open"},
            };
            const Row tools_dbflows_s1[] = {
                {"password", "Password prompt", "With caps-lock warning and quality-free entry", PillKind::Action, "Open"},
                {"description", "Key file selector", "Remembers the last key file per database", PillKind::Value, "Remembered"},
                {"usb", "Hardware key", "Refresh, slot pick, touch prompt", PillKind::Value, "Slot 2"},
                {"fingerprint", "Quick unlock", "Windows Hello, Touch ID, polkit", PillKind::Good, "Available"},
                {"folder_open", "Welcome screen", "Recent databases, Open, New, Import", PillKind::Action, "Open"},
            };
            const Row tools_dbflows_s2[] = {
                {"merge", "Merge from database…", "Reports what changed per entry", PillKind::Action, "Merge"},
                {"cloud_sync", "Remote sync…", "Runs the configured download/upload commands", PillKind::Action, "Sync"},
                {"backup", "Save database backup…", "", PillKind::Action, "Save"},
                {"save_as", "Save database as…", "", PillKind::Action, "Save as"},
            };
            const Section tools_dbflows_sections[] = {
                {"Create a database", "NewDatabaseWizard — four pages.", tools_dbflows_s0, 4},
                {"Open and unlock", "DatabaseOpenWidget + QuickUnlock.", tools_dbflows_s1, 5},
                {"Merging and syncing", "Merger, MergeDialog, RemoteHandler.", tools_dbflows_s2, 4},
            };

            const Row tools_generator_s0[] = {
                {"straighten", "Length slider", "1–128 on the slider, up to 999 typed", PillKind::Value, "20"},
                {"text_fields", "A-Z · a-z · 0-9", "", PillKind::On, "On"},
                {"emoji_symbols", "/ * + & · . , : ; · \" ' · \\ / | _ - · < > * + ! ? = · { [ ( ) ] }", "Special, punctuation, quotes, dashes, math, braces", PillKind::Off, "Off"},
                {"translate", "Extended ASCII", "", PillKind::Off, "Off"},
                {"tune", "Advanced", "Additional and excluded characters, Hex button", PillKind::Off, "Off"},
                {"blur_on", "Exclude look-alike characters", "", PillKind::On, "On"},
                {"checklist", "Pick characters from every group", "", PillKind::On, "On"},
                {"bolt", "Entropy readout", "Live bits and strength label", PillKind::Good, "124 bits"},
            };
            const Row tools_generator_s1[] = {
                {"numbers", "Word count", "1–40", PillKind::Value, "6"},
                {"space_bar", "Word separator", "", PillKind::Mono, "-"},
                {"menu_book", "Wordlist", "Add or delete custom wordlists", PillKind::Value, "eff_large.wordlist"},
                {"text_format", "Word case", "", PillKind::Value, "Title Case"},
            };
            const Row tools_generator_s2[] = {
                {"keyboard", "Pick characters dialog", "{PICKCHARS} during Auto-Type", PillKind::Action, "Open"},
                {"rule", "Estimate an existing password", "keepassxc-cli estimate", PillKind::Action, "Open"},
            };
            const Section tools_generator_sections[] = {
                {"Password mode", "Character-type buttons toggle whole classes.", tools_generator_s0, 8},
                {"Passphrase mode", "PassphraseGenerator + bundled wordlists.", tools_generator_s1, 4},
                {"Related helpers", "", tools_generator_s2, 2},
            };

            const Row tools_autotypeflow_s0[] = {
                {"keyboard_alt", "Global shortcut match list", "Matches by window title and URL", PillKind::Action, "Open"},
                {"search", "Filter matches", "Search field inside the dialog", PillKind::Value, "Wired to regex builder"},
                {"sort", "Remembered sort column and order", "AutoTypeDialogSortColumn/Order", PillKind::Value, "Title, ascending"},
                {"window", "Window title of the target", "Shown so you can confirm before typing", PillKind::Value, "Live"},
            };
            const Row tools_autotypeflow_s1[] = {
                {"code", "Entry default sequence", "", PillKind::Mono, "{USERNAME}{TAB}{PASSWORD}{ENTER}"},
                {"person", "{USERNAME} / {USERNAME}{ENTER}", "", PillKind::Mono, "Ctrl+Shift+V"},
                {"password", "{PASSWORD} / {PASSWORD}{ENTER}", "", PillKind::Mono, "—"},
                {"timer", "{TOTP}", "", PillKind::Mono, "—"},
                {"link", "{URL} / {URL}{ENTER}", "", PillKind::Mono, "—"},
                {"touch_app", "{PICKCHARS}", "Prompts for individual characters", PillKind::Action, "Demo"},
            };
            const Section tools_autotypeflow_sections[] = {
                {"Global Auto-Type", "", tools_autotypeflow_s0, 4},
                {"Sequences", "", tools_autotypeflow_s1, 6},
            };

            const Row tools_importexport_s0[] = {
                {"table_view", "CSV file", "Column mapping preview, CsvImportWidget", PillKind::Action, "Import"},
                {"lock", "KeePass 1 database", "", PillKind::Action, "Import"},
                {"inventory_2", "1Password 1PUX", "", PillKind::Action, "Import"},
                {"inventory", "1Password OpVault", "", PillKind::Action, "Import"},
                {"shield", "Bitwarden JSON", "Encrypted or plain export", PillKind::Action, "Import"},
                {"mail_lock", "Proton Pass", "", PillKind::Action, "Import"},
                {"passkey", "Import passkey", "Standalone passkey file", PillKind::Action, "Import"},
            };
            const Row tools_importexport_s1[] = {
                {"table_view", "CSV file", "", PillKind::Action, "Export"},
                {"html", "HTML file", "HtmlGuiExporter — printable", PillKind::Action, "Export"},
                {"code", "XML file", "Unencrypted KDBX XML", PillKind::Action, "Export"},
                {"warning", "Exports are plaintext", "Every export leaves encryption behind", PillKind::Bad, "Confirm required"},
            };
            const Section tools_importexport_sections[] = {
                {"Import", "", tools_importexport_s0, 7},
                {"Export", "", tools_importexport_s1, 4},
            };

            const Row tools_reportstools_s0[] = {
                {"query_stats", "Statistics", "Size, unique passwords, average entropy, KDF", PillKind::Action, "Open"},
                {"health_and_safety", "Health check", "Weak, short, reused, expired, aged", PillKind::Action, "Open"},
                {"passkey", "Passkeys", "Relying parties and user handles", PillKind::Action, "Open"},
                {"extension", "Browser statistics", "Entries with browser rules and permissions", PillKind::Action, "Open"},
                {"gpp_bad", "HIBP", "Offline breach list check, never network", PillKind::Action, "Open"},
            };
            const Section tools_reportstools_sections[] = {
                {"Pages", "", tools_reportstools_s0, 5},
            };

            const Row tools_viewtools_s0[] = {
                {"view_agenda", "Show preview panel", "", PillKind::On, "On"},
                {"account_tree", "Show group panel", "", PillKind::On, "On"},
                {"menu", "Show menubar", "", PillKind::On, "On"},
                {"smart_button", "Show toolbar", "", PillKind::On, "On"},
                {"compress", "Compact mode", "", PillKind::Off, "Off"},
                {"vertical_align_top", "Always on top", "", PillKind::Off, "Off"},
            };
            const Row tools_viewtools_s1[] = {
                {"person_off", "Hide usernames", "", PillKind::Off, "Off"},
                {"visibility_off", "Hide passwords", "", PillKind::On, "On"},
                {"screenshot_monitor", "Allow screen capture", "", PillKind::Off, "Off"},
                {"palette", "Theme", "Automatic · Light · Dark · Classic (platform-native)", PillKind::Value, "Automatic"},
            };
            const Section tools_viewtools_sections[] = {
                {"Layout", "", tools_viewtools_s0, 6},
                {"Privacy", "", tools_viewtools_s1, 4},
            };

            const Row tools_grouptools_s0[] = {
                {"create_new_folder", "New group", "", PillKind::Action, "Create"},
                {"edit", "Edit group", "Name, icon, notes, Auto-Type sequence, search flag", PillKind::Action, "Edit"},
                {"content_copy", "Clone group", "", PillKind::Action, "Clone"},
                {"delete", "Delete group", "", PillKind::Action, "Delete"},
                {"sort_by_alpha", "Sort A-Z / Z-A", "", PillKind::Action, "Sort"},
                {"cloud_download", "Download all favicons", "", PillKind::Action, "Download"},
                {"delete_sweep", "Empty recycle bin", "", PillKind::Action, "Empty"},
            };
            const Row tools_grouptools_s1[] = {
                {"add", "New · Edit · Clone · Delete · Restore", "CloneDialog offers title suffix, history and reference options", PillKind::Action, "Run"},
                {"event_busy", "Expire entry", "Sets the expiry to now", PillKind::Action, "Expire"},
                {"swap_vert", "Move up / move down", "", PillKind::Action, "Move"},
                {"content_copy", "Copy username, password, URL, title, notes, TOTP", "", PillKind::Action, "Copy"},
                {"label", "Tags submenu", "Assign or remove tags in bulk", PillKind::Action, "Tag"},
                {"open_in_new", "Open URL", "", PillKind::Action, "Open"},
                {"image", "Download favicon", "IconDownloaderDialog for a whole selection", PillKind::Action, "Download"},
                {"terminal", "Add to / remove from SSH agent", "", PillKind::Action, "Run"},
                {"passkey", "Import or remove passkey", "", PillKind::Action, "Run"},
            };
            const Section tools_grouptools_sections[] = {
                {"Groups", "", tools_grouptools_s0, 7},
                {"Entries", "", tools_grouptools_s1, 9},
            };

            const Page tools_pages[] = {
                {"dbflows", "database", "Database flows", "Database flows", "NewDatabaseWizard, DatabaseOpenWidget, WelcomeWidget, DatabaseTabWidget.", tools_dbflows_sections, 3},
                {"generator", "casino", "Password generator", "Password generator", "PasswordGeneratorWidget — both modes and the advanced panel. The interactive dialog is on the toolbar.", tools_generator_sections, 3},
                {"autotypeflow", "keyboard", "Auto-Type flows", "Auto-Type flows", "AutoTypeSelectDialog, AutoTypeMatchView, PickcharsDialog.", tools_autotypeflow_sections, 2},
                {"importexport", "import_export", "Import and export", "Import and export", "ImportWizard (Select → Review) and the Export menu.", tools_importexport_sections, 2},
                {"reportstools", "health_metrics", "Reports", "Reports", "ReportsDialog pages — the interactive versions live on the Reports destination.", tools_reportstools_sections, 1},
                {"viewtools", "visibility", "View toggles", "View toggles", "Everything on the View menu, applied live.", tools_viewtools_sections, 2},
                {"grouptools", "account_tree", "Group and entry actions", "Group and entry actions", "The Groups and Entries menus in full.", tools_grouptools_sections, 2},
            };

            const Sheet AllSheets[] = {
                {"settings", "Application settings", settings_pages, 8},
                {"database", "Database settings", database_pages, 7},
                {"editor", "Entry editor", editor_pages, 8},
                {"help", "Help", help_pages, 3},
                {"tools", "Tools and flows", tools_pages, 7},
            };
        } // namespace

        /** Translate a design string in the catalogue's own context. */
        static QString text(const char* raw)
        {
            return (raw && *raw) ? QCoreApplication::translate("Material::SheetCatalogue", raw) : QString();
        }

        QStringList sheetIds()
        {
            QStringList ids;
            ids.reserve(static_cast<int>(std::size(AllSheets)));
            for (const auto& sheet : AllSheets) {
                ids << QString::fromLatin1(sheet.id);
            }
            return ids;
        }

        const Sheet* sheet(const QString& id)
        {
            for (const auto& sheet : AllSheets) {
                if (id == QLatin1String(sheet.id)) {
                    return &sheet;
                }
            }
            return nullptr;
        }

        QString label(const QString& id)
        {
            const Sheet* found = sheet(id);
            return found ? text(found->label) : QString();
        }

        int pageCount(const QString& id)
        {
            const Sheet* found = sheet(id);
            return found ? found->pageCount : 0;
        }

        bool addPage(SpecSheet* target, const QString& sheetId, const QString& pageId)
        {
            const Sheet* found = sheet(sheetId);
            if (!target || !found) {
                return false;
            }
            for (int i = 0; i < found->pageCount; ++i) {
                const Page& page = found->pages[i];
                if (pageId != QLatin1String(page.id)) {
                    continue;
                }
                auto* built = target->addPage(
                    QString::fromLatin1(page.id), QString::fromLatin1(page.symbol), text(page.label));
                if (!built) {
                    return false;
                }
                built->setNote(text(page.note));
                for (int s = 0; s < page.sectionCount; ++s) {
                    const Section& section = page.sections[s];
                    const QString title = text(section.title);
                    built->setSectionNote(title, text(section.note));
                    for (int r = 0; r < section.rowCount; ++r) {
                        const Row& row = section.rows[r];
                        built->addRow(title,
                                      QString::fromLatin1(row.symbol),
                                      text(row.label),
                                      text(row.sub),
                                      row.kind,
                                      text(row.control));
                    }
                }
                return true;
            }
            return false;
        }

        SpecSheet* create(const QString& id, QWidget* parent)
        {
            const Sheet* found = sheet(id);
            if (!found) {
                return nullptr;
            }
            auto* target = new SpecSheet(parent);
            for (int i = 0; i < found->pageCount; ++i) {
                addPage(target, id, QString::fromLatin1(found->pages[i].id));
            }
            return target;
        }

    } // namespace SheetCatalogue
} // namespace Material
