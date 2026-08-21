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

#ifndef KEEPASSXC_MATERIALTHEME_H
#define KEEPASSXC_MATERIALTHEME_H

#include <QColor>
#include <QFont>
#include <QHash>
#include <QObject>
#include <QPalette>
#include <QString>

/**
 * The Material Design 3 design system for KeePassXC.
 *
 * Everything the interface draws resolves through this file: colour roles,
 * shape, elevation, motion, type scale and density. Widgets never hard-code a
 * colour; they ask the theme for a role. Changing the seed, the light/dark mode
 * or the density therefore restyles the whole application at once.
 */
namespace Material
{
    /** Light or dark surface family. */
    enum class Mode
    {
        Light,
        Dark
    };

    /** The four seed palettes offered in the appearance settings. */
    enum class Seed
    {
        KeePass,
        Purple,
        Green,
        Amber
    };

    /** Row height family. Drives every list, tree and table in the app. */
    enum class Density
    {
        Compact,
        Comfortable,
        Spacious
    };

    /**
     * Semantic colour roles. Names follow the Material Design 3 `md.sys.color`
     * tokens, plus the three KeePassXC status families (green / amber / error)
     * used for password health.
     */
    enum class Role
    {
        Primary,
        OnPrimary,
        PrimaryContainer,
        OnPrimaryContainer,
        SecondaryContainer,
        OnSecondaryContainer,
        Error,
        OnError,
        ErrorContainer,
        OnErrorContainer,
        Surface,
        SurfaceContainerLowest,
        SurfaceContainerLow,
        SurfaceContainer,
        SurfaceContainerHigh,
        SurfaceContainerHighest,
        OnSurface,
        OnSurfaceVariant,
        Outline,
        OutlineVariant,
        InverseSurface,
        InverseOnSurface,
        Green,
        GreenContainer,
        OnGreenContainer,
        Amber,
        AmberContainer,
        OnAmberContainer
    };

    /** Password / entry health state, mapped onto the status colour families. */
    enum class Health
    {
        Ok,
        Weak,
        Reused,
        Breached,
        Unknown
    };

    /**
     * Corner radii. Material 3 shape scale, extended with the two bespoke radii
     * the KeePassXC design uses for list rows and the navigation rail.
     */
    namespace Shape
    {
        constexpr int None = 0;
        constexpr int ExtraSmall = 6;
        constexpr int Small = 8;
        constexpr int Medium = 12;
        constexpr int Large = 14;
        constexpr int Row = 16;
        constexpr int Rail = 18;
        constexpr int ExtraLarge = 28;
        constexpr int Full = 999;
    } // namespace Shape

    /** Motion durations in milliseconds, from the mockup's transitions. */
    namespace Duration
    {
        constexpr int Short = 140;
        constexpr int Toggle = 160; // switch track and knob; the only 160ms transition
        constexpr int Medium = 180;
        constexpr int Long = 240;
        constexpr int Toast = 4200;
    } // namespace Duration

    /** Fixed layout metrics that do not vary with density. */
    namespace Layout
    {
        constexpr int RailWidth = 88;
        constexpr int RailItemWidth = 66;
        constexpr int AppBarHeight = 64;
        constexpr int TabStripHeight = 48;
        constexpr int TabHeight = 38;
        constexpr int GroupPaneWidth = 250;
        constexpr int DetailPaneWidth = 392;
        constexpr int SheetNavWidth = 266;
        constexpr int SearchBarHeight = 52;
        constexpr int SurfaceSearchHeight = 44;
        constexpr int FabHeight = 56;
        constexpr int ButtonHeight = 40;
        constexpr int ChipHeight = 32;
        constexpr int IconButtonSize = 40;
    } // namespace Layout

    /**
     * The Material type scale as used by the design. Each role resolves to a
     * concrete QFont built from the interface font family and the user's font
     * size adjustment.
     */
    enum class TypeRole
    {
        DisplaySmall, // 44 light - report stat values
        HeadlineSmall, // 28 regular - screen titles
        TitleLarge, // 22 regular - app bar title, detail title
        TitleMedium, // 18 regular - card titles
        TitleSmall, // 17 regular - section titles
        BodyLarge, // 15 regular - search input
        BodyMedium, // 14 regular - default body
        LabelLarge, // 14 medium - buttons
        BodySmall, // 13 regular - secondary text
        LabelMedium, // 12 medium - rail labels, meta
        LabelSmall, // 11 medium - field captions, overlines
        Mono // 14 Roboto Mono - credentials
    };

    /**
     * A resolved set of colour roles. Built by combining a seed palette with a
     * light or dark surface family.
     */
    class ColorScheme
    {
    public:
        ColorScheme() = default;
        ColorScheme(Seed seed, Mode mode);

        QColor color(Role role) const;
        /** Role colour blended over its surface at @p alpha (0..1). */
        QColor overlay(Role role, qreal alpha) const;

        Seed seed() const
        {
            return m_seed;
        }
        Mode mode() const
        {
            return m_mode;
        }
        bool isDark() const
        {
            return m_mode == Mode::Dark;
        }

        /** The status colour family for a health state. */
        QColor healthColor(Health health) const;
        QColor healthContainer(Health health) const;
        QColor onHealthContainer(Health health) const;

    private:
        void resolve();

        Seed m_seed = Seed::KeePass;
        Mode m_mode = Mode::Light;
        QHash<int, QColor> m_colors;
    };

    /**
     * Application-wide design system state.
     *
     * Owns the active colour scheme, density and type scale, applies them to
     * the QApplication palette and stylesheet, and notifies the interface when
     * any of it changes.
     */
    class Theme : public QObject
    {
        Q_OBJECT

    public:
        static Theme* instance();

        /** Re-read the configuration and restyle the application. */
        void reload();

        const ColorScheme& colors() const
        {
            return m_colors;
        }
        QColor color(Role role) const
        {
            return m_colors.color(role);
        }
        bool isDark() const
        {
            return m_colors.isDark();
        }

        Seed seed() const
        {
            return m_seed;
        }
        Mode mode() const
        {
            return m_colors.mode();
        }
        Density density() const
        {
            return m_density;
        }

        void setSeed(Seed seed);
        void setMode(Mode mode);
        void setDensity(Density density);
        QString configuredMode() const;
        void setConfiguredMode(const QString& mode);

        /** List row height for the active density: 40, 52 or 64 logical px. */
        int rowHeight() const;
        /** Vertical padding inside cards and panes for the active density. */
        int pagePadding() const;

        QFont font(TypeRole role) const;
        /** The monospace family used for credentials and metadata. */
        static QString monoFamily();
        /** The interface family: Roboto when present, else the platform UI font. */
        static QString uiFamily();

        /** A QPalette derived from the active scheme, for widgets Qt draws itself. */
        QPalette palette() const;

        /** The complete application stylesheet for the active scheme. */
        QString styleSheet() const;

        /** CSS-ready `#rrggbb` for a role, for use in local stylesheet fragments. */
        QString hex(Role role) const
        {
            return m_colors.color(role).name();
        }

        /** Parse / serialise for configuration storage. */
        static Seed seedFromString(const QString& value);
        static QString seedToString(Seed seed);
        static QString seedDisplayName(Seed seed);
        static QColor seedSwatch(Seed seed);
        static Density densityFromString(const QString& value);
        static QString densityToString(Density density);

        static Health healthFromString(const QString& value);
        /**
         * The design's name for a health state: Healthy / Weak / Reused /
         * Breached. Shares one vocabulary with healthColor() so a readout and
         * its tint can never contradict each other.
         */
        static QString healthLabel(Health health);

    signals:
        /** Emitted after the scheme, density or type scale changed. */
        void changed();

    private:
        Theme();

        void resolveMode();

        Seed m_seed = Seed::KeePass;
        Density m_density = Density::Comfortable;
        QString m_configuredMode = QStringLiteral("auto");
        ColorScheme m_colors;
    };

} // namespace Material

/** Convenience accessor, mirroring config() / icons() elsewhere in the codebase. */
inline Material::Theme* theme()
{
    return Material::Theme::instance();
}

#endif // KEEPASSXC_MATERIALTHEME_H
