import fs from 'fs';

const SHEETS = JSON.parse(fs.readFileSync(new URL('./design/sheets.json', import.meta.url), 'utf8'));

// Secret Service Integration is a freedesktop.org feature. This fork is Windows
// only and src/fdosecrets/ is gone, so the design's `fdo` page has nothing to
// describe and is deliberately not generated.
const SKIP_PAGES = new Set(['fdo', 'dbfdo']);

const KIND = { on: 'On', off: 'Off', val: 'Value', act: 'Action', mono: 'Mono', good: 'Good', warn: 'Warn', bad: 'Bad' };

const esc = (s) =>
  String(s == null ? '' : s)
    .replace(/\\/g, '\\\\')
    .replace(/"/g, '\\"')
    .replace(/\n/g, '\\n');

const LICENSE = `/*
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
`;

const GENERATED = `
// This file is generated from the design mockup's SHEETS table
// ("KeePassXC Material.dc.html"). It is the design's own wording, section
// order and control values, transcribed rather than paraphrased, so the
// reference sheets can be diffed against the mockup.
//
// Regenerate with utils/generate_sheet_catalogue.mjs; do not hand-edit.
`;

let cpp = LICENSE + GENERATED + `
#include "MaterialSheetCatalogue.h"

#include "MaterialSpecSheet.h"

#include <QCoreApplication>

namespace Material
{
    namespace SheetCatalogue
    {
        namespace
        {
`;

const order = ['settings', 'database', 'editor', 'help', 'tools'];
const sheetVars = [];
let counts = { pages: 0, sections: 0, rows: 0 };

for (const sheetId of order) {
  const sheet = SHEETS[sheetId];
  const pageVars = [];
  for (const page of sheet.pages) {
    if (SKIP_PAGES.has(page.id)) continue;
    const sectionVars = [];
    page.sections.forEach((sec, si) => {
      const v = `${sheetId}_${page.id.replace(/-/g, '_')}_s${si}`;
      cpp += `            const Row ${v}[] = {\n`;
      for (const r of sec.rows) {
        cpp += `                {"${esc(r.icon)}", "${esc(r.label)}", "${esc(r.sub)}", PillKind::${KIND[r.kind] || 'Value'}, "${esc(r.control)}"},\n`;
        counts.rows++;
      }
      cpp += `            };\n`;
      sectionVars.push({ v, title: sec.title, note: sec.note, n: sec.rows.length });
      counts.sections++;
    });

    const pv = `${sheetId}_${page.id.replace(/-/g, '_')}_sections`;
    cpp += `            const Section ${pv}[] = {\n`;
    for (const s of sectionVars) {
      cpp += `                {"${esc(s.title)}", "${esc(s.note)}", ${s.v}, ${s.n}},\n`;
    }
    cpp += `            };\n\n`;
    pageVars.push({ pv, page, n: sectionVars.length });
    counts.pages++;
  }

  const sv = `${sheetId}_pages`;
  cpp += `            const Page ${sv}[] = {\n`;
  for (const p of pageVars) {
    cpp += `                {"${esc(p.page.id)}", "${esc(p.page.icon)}", "${esc(p.page.label)}", "${esc(p.page.title)}", "${esc(p.page.note)}", ${p.pv}, ${p.n}},\n`;
  }
  cpp += `            };\n\n`;
  sheetVars.push({ sheetId, sv, label: sheet.label, n: pageVars.length });
}

cpp += `            const Sheet AllSheets[] = {\n`;
for (const s of sheetVars) {
  cpp += `                {"${esc(s.sheetId)}", "${esc(s.label)}", ${s.sv}, ${s.n}},\n`;
}
cpp += `            };
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
`;

const h = LICENSE + GENERATED + `
#ifndef KEEPASSXC_MATERIALSHEETCATALOGUE_H
#define KEEPASSXC_MATERIALSHEETCATALOGUE_H

#include "MaterialChip.h"

#include <QString>
#include <QStringList>

class QWidget;

namespace Material
{
    class SpecSheet;

    /**
     * The design's reference spec sheets, transcribed from the mockup.
     *
     * The mockup describes five sheets - Application settings, Database
     * settings, Entry editor, Help and Tools and flows - as pages of sections
     * of rows, each row a glyph, a label, a sub line and a control pill. The
     * settings sheet is served live by SettingsHub, which binds its rows to
     * real Config keys; the other four describe surfaces that live elsewhere
     * in the application, so they are presented as reference sheets.
     */
    namespace SheetCatalogue
    {
        struct Row
        {
            const char* symbol;
            const char* label;
            const char* sub;
            PillKind kind;
            const char* control;
        };

        struct Section
        {
            const char* title;
            const char* note;
            const Row* rows;
            int rowCount;
        };

        struct Page
        {
            const char* id;
            const char* symbol;
            /** Short name, used in the sidebar. */
            const char* label;
            /** Full heading, used above the sections. */
            const char* title;
            const char* note;
            const Section* sections;
            int sectionCount;
        };

        struct Sheet
        {
            const char* id;
            const char* label;
            const Page* pages;
            int pageCount;
        };

        /** Every sheet id, in design order. */
        QStringList sheetIds();

        const Sheet* sheet(const QString& id);

        /** The sheet's own name, translated, or an empty string. */
        QString label(const QString& id);

        /** How many pages @p id carries, which the rail reports as its sublabel. */
        int pageCount(const QString& id);

        /** Append one design page onto @p target. Returns false if unknown. */
        bool addPage(SpecSheet* target, const QString& sheetId, const QString& pageId);

        /** A new SpecSheet carrying every page of @p id, in design order. */
        SpecSheet* create(const QString& id, QWidget* parent = nullptr);

    } // namespace SheetCatalogue
} // namespace Material

#endif // KEEPASSXC_MATERIALSHEETCATALOGUE_H
`;

fs.writeFileSync(new URL('../src/gui/material/MaterialSheetCatalogue.cpp', import.meta.url), cpp);
fs.writeFileSync(new URL('../src/gui/material/MaterialSheetCatalogue.h', import.meta.url), h);
console.log('generated', JSON.stringify(counts));
