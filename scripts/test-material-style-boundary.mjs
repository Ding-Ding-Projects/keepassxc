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
const currentConfigTest = read('tests/TestConfig.cpp');

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
assert.match(currentConfig, /#define CONFIG_VERSION 3/);
assert.match(currentConfigTest, /oldConfig\.setValue\("ConfigVersion", 2\)/);
assert.match(currentConfigTest, /GUI_Language\).*QStringLiteral\("fr"\)/);
assert.match(currentConfigTest, /GUI_ShowTrayIcon\).*toBool\(\)/);

const settingsCatalogs = readdirSync(join(root, 'share/translations')).filter(name => /^keepassxc_.*\.ts$/.test(name));
let translatedClassicMessages = 0;
for (const name of settingsCatalogs) {
    const catalog = read(`share/translations/${name}`);
    const settingsContext = catalog.match(/<context>\r?\n\s*<name>ApplicationSettingsWidget<\/name>[\s\S]*?<\/context>/);
    assert.ok(settingsContext, `${name} lost the ApplicationSettingsWidget context`);
    assert.doesNotMatch(catalog, /<source>Toolbar button style:?<\/source>/, `${name} retains an obsolete toolbar label`);
    assert.doesNotMatch(settingsContext[0], /<source>(?:Icon only|Text only|Text beside icon|Text under icon|Follow style)<\/source>/, `${name} retains obsolete toolbar labels`);
    for (const source of [
        'Application Settings',
        'General',
        'Security',
        'This setting cannot be enabled when minimize on unlock is enabled.',
        'Access error for config file %1',
    ]) {
        const escapedSource = source.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
        const message = settingsContext[0].match(new RegExp(`<message>\\s*<source>${escapedSource}<\\/source>\\s*<translation(?:\\s+[^>]*)?(?:\\/>(?:\\s*<\\/message>)|>([\\s\\S]*?)<\\/translation>\\s*<\\/message>)`));
        assert.ok(message, `${name} lost classic settings copy: ${source}`);
        const isExplicitlyUnfinished = /<translation\s+type="unfinished"/.test(message[0]);
        assert.ok(isExplicitlyUnfinished || message[1]?.trim(), `${name} lost translated classic settings copy: ${source}`);
        if (!isExplicitlyUnfinished) {
            translatedClassicMessages++;
        }
    }
}

process.stdout.write(`PASS: obsolete source routes are absent; ${translatedClassicMessages}/${settingsCatalogs.length * 5} classic settings messages remain translated across ${settingsCatalogs.length} catalogs (unfinished entries remain explicitly marked), with no stale toolbar-style labels.\n`);
