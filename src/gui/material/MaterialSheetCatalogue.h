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
