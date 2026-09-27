import assert from 'node:assert/strict';
import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const read = (path) => readFileSync(join(root, path), 'utf8');
const currentApplication = read('src/gui/Application.cpp');
const currentWindow = read('src/gui/MainWindow.cpp');
const currentMain = read('src/main.cpp');
const currentBrowserSettings = read('src/browser/BrowserSettingsWidget.ui');
const currentGeneralUi = read('src/gui/ApplicationSettingsWidgetGeneral.ui');
const currentHub = read('src/gui/material/MaterialSettingsHub.cpp');
const currentCatalogue = read('src/gui/material/MaterialSheetCatalogue.cpp');
const currentConfig = read('src/core/Config.cpp');

// Negative regression: the probes must recognize representative forms of each removed route.
const oldStyleEscape = 'if (!env.contains(QStringLiteral("KPXC_NO_MATERIAL_STYLE"))) { setStyle(new Material::Style); }';
const oldSheetEscape = 'if (!env.contains(QStringLiteral("KPXC_NO_MATERIAL_SHEET"))) { setStyleSheet(theme()->styleSheet()); }';
const oldCaptionEscape = 'if (!QCoreApplication::arguments().contains(QStringLiteral("--native-caption"))) { installNativeCaption(); }';
const oldToolbarControl = 'addChoice(interfacePage, window, Config::GUI_ToolButtonStyle);';
assert.match(oldStyleEscape, /KPXC_NO_MATERIAL_STYLE/);
assert.match(oldSheetEscape, /KPXC_NO_MATERIAL_SHEET/);
assert.match(oldCaptionEscape, /--native-caption/);
assert.match(oldToolbarControl, /GUI_ToolButtonStyle/);

assert.doesNotMatch(currentApplication, /KPXC_NO_MATERIAL_(?:STYLE|SHEET)/);
assert.doesNotMatch(currentWindow, /--native-caption/);
assert.doesNotMatch(currentMain, /--native-caption/);
assert.doesNotMatch(currentGeneralUi, /toolButtonStyleComboBox|toolButtonStyleLabel/);
assert.doesNotMatch(currentHub, /GUI_ToolButtonStyle|Tool button style/);
assert.doesNotMatch(currentCatalogue, /Toolbar button style/);
assert.doesNotMatch(currentBrowserSettings, /Toolbar button style/);
assert.match(currentConfig, /\{QS\("GUI\/ToolButtonStyle"\), Config::Deleted\}/);

const settingsCatalogs = readdirSync(join(root, 'share/translations')).filter(name => /^keepassxc_.*\.ts$/.test(name));
for (const name of settingsCatalogs) {
    const catalog = read(`share/translations/${name}`);
    const settingsContext = catalog.match(/<context>\r?\n\s*<name>ApplicationSettingsWidget<\/name>[\s\S]*?<\/context>/);
    if (settingsContext) {
        assert.doesNotMatch(settingsContext[0], /<source>(?:Icon only|Text only|Text beside icon|Text under icon|Follow style)<\/source>/, `${name} retains obsolete toolbar labels`);
    }
}

process.stdout.write(`PASS: obsolete source routes are absent; ${settingsCatalogs.length} translation catalogs contain no stale toolbar-style labels in ApplicationSettingsWidget.\n`);
