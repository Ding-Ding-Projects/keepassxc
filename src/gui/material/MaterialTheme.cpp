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

#include "MaterialTheme.h"
#include "MaterialStyleSheet.h"

#include "core/Config.h"
#include "gui/osutils/OSUtils.h"

#include <QApplication>
#include <QFontDatabase>
#include <QHash>

namespace Material
{
    namespace
    {
        /**
         * Surface, outline and neutral roles. These are shared by every seed;
         * only the accent roles below change when the seed changes.
         */
        struct NeutralTokens
        {
            const char* surface;
            const char* surfaceContainerLowest;
            const char* surfaceContainerLow;
            const char* surfaceContainer;
            const char* surfaceContainerHigh;
            const char* surfaceContainerHighest;
            const char* onSurface;
            const char* onSurfaceVariant;
            const char* outline;
            const char* outlineVariant;
            const char* inverseSurface;
            const char* inverseOnSurface;
        };

        constexpr NeutralTokens LightNeutrals = {"#f8f9ff",
                                                 "#ffffff",
                                                 "#f2f3f9",
                                                 "#eceef4",
                                                 "#e6e8ee",
                                                 "#e0e2e8",
                                                 "#191c20",
                                                 "#41474d",
                                                 "#71787e",
                                                 "#c1c7ce",
                                                 "#2e3135",
                                                 "#eff1f7"};

        constexpr NeutralTokens DarkNeutrals = {"#111417",
                                                "#0c0f12",
                                                "#191c20",
                                                "#1d2024",
                                                "#282a2e",
                                                "#333539",
                                                "#e1e2e7",
                                                "#c1c7ce",
                                                "#8b9198",
                                                "#41474d",
                                                "#e1e2e7",
                                                "#2e3135"};

        /** The accent roles a seed contributes, for one surface family. */
        struct SeedTokens
        {
            const char* primary;
            const char* onPrimary;
            const char* primaryContainer;
            const char* onPrimaryContainer;
            const char* secondaryContainer;
            const char* onSecondaryContainer;
        };

        struct SeedPalette
        {
            const char* id;
            const char* displayName;
            const char* swatch;
            SeedTokens light;
            SeedTokens dark;
        };

        constexpr SeedPalette SeedPalettes[] = {
            {"keepass",
             "KeePassXC blue",
             "#006493",
             {"#006493", "#ffffff", "#c9e6ff", "#001e30", "#d3e5f5", "#0c1d29"},
             {"#8dcdff", "#00344f", "#004b70", "#c9e6ff", "#394956", "#d3e5f5"}},
            {"purple",
             "Baseline purple",
             "#6750a4",
             {"#6750a4", "#ffffff", "#e9ddff", "#22005d", "#e8def8", "#1e192b"},
             {"#cfbcff", "#381e72", "#4f378a", "#e9ddff", "#4a4458", "#e8def8"}},
            {"green",
             "Vault green",
             "#1b7f37",
             {"#146c2e", "#ffffff", "#a6f5b0", "#002108", "#d5e8d5", "#101f12"},
             {"#8bd996", "#003914", "#00531f", "#a6f5b0", "#3a4b3c", "#d5e8d5"}},
            {"amber",
             "Signal amber",
             "#9a6700",
             {"#8a5200", "#ffffff", "#ffdda9", "#2c1600", "#f0e0cc", "#251a09"},
             {"#ffb95c", "#492900", "#693c00", "#ffdda9", "#4f4536", "#f0e0cc"}},
        };

        const SeedPalette& paletteFor(Seed seed)
        {
            return SeedPalettes[static_cast<int>(seed)];
        }

        /**
         * Error and password-health colours. Like the neutrals, these flip
         * between the surface families so a "weak" chip stays legible on a dark
         * background instead of glowing.
         */
        struct StatusTokens
        {
            const char* error;
            const char* onError;
            const char* errorContainer;
            const char* onErrorContainer;
            const char* green;
            const char* greenContainer;
            const char* onGreenContainer;
            const char* amber;
            const char* amberContainer;
            const char* onAmberContainer;
        };

        constexpr StatusTokens LightStatus = {"#ba1a1a",
                                              "#ffffff",
                                              "#ffdad6",
                                              "#410002",
                                              "#1b7f37",
                                              "#d2f2d8",
                                              "#0b3d1c",
                                              "#9a6700",
                                              "#ffe9b8",
                                              "#5c4400"};

        constexpr StatusTokens DarkStatus = {"#ffb4ab",
                                             "#690005",
                                             "#93000a",
                                             "#ffdad6",
                                             "#57ab5a",
                                             "#113a1b",
                                             "#b9f0c0",
                                             "#d8a739",
                                             "#3d2e00",
                                             "#ffe08a"};

        /** Blend @p fg over @p bg at @p alpha. */
        QColor blend(const QColor& fg, const QColor& bg, qreal alpha)
        {
            alpha = qBound(0.0, alpha, 1.0);
            return QColor::fromRgbF(fg.redF() * alpha + bg.redF() * (1.0 - alpha),
                                    fg.greenF() * alpha + bg.greenF() * (1.0 - alpha),
                                    fg.blueF() * alpha + bg.blueF() * (1.0 - alpha));
        }
    } // namespace

    // ---------------------------------------------------------------- ColorScheme

    ColorScheme::ColorScheme(Seed seed, Mode mode)
        : m_seed(seed)
        , m_mode(mode)
    {
        resolve();
    }

    void ColorScheme::resolve()
    {
        const auto& palette = paletteFor(m_seed);
        const bool dark = m_mode == Mode::Dark;
        const SeedTokens& accents = dark ? palette.dark : palette.light;
        const NeutralTokens& neutrals = dark ? DarkNeutrals : LightNeutrals;
        const StatusTokens& status = dark ? DarkStatus : LightStatus;

        auto set = [this](Role role, const char* hex) { m_colors.insert(static_cast<int>(role), QColor(hex)); };

        set(Role::Primary, accents.primary);
        set(Role::OnPrimary, accents.onPrimary);
        set(Role::PrimaryContainer, accents.primaryContainer);
        set(Role::OnPrimaryContainer, accents.onPrimaryContainer);
        set(Role::SecondaryContainer, accents.secondaryContainer);
        set(Role::OnSecondaryContainer, accents.onSecondaryContainer);

        set(Role::Surface, neutrals.surface);
        set(Role::SurfaceContainerLowest, neutrals.surfaceContainerLowest);
        set(Role::SurfaceContainerLow, neutrals.surfaceContainerLow);
        set(Role::SurfaceContainer, neutrals.surfaceContainer);
        set(Role::SurfaceContainerHigh, neutrals.surfaceContainerHigh);
        set(Role::SurfaceContainerHighest, neutrals.surfaceContainerHighest);
        set(Role::OnSurface, neutrals.onSurface);
        set(Role::OnSurfaceVariant, neutrals.onSurfaceVariant);
        set(Role::Outline, neutrals.outline);
        set(Role::OutlineVariant, neutrals.outlineVariant);
        set(Role::InverseSurface, neutrals.inverseSurface);
        set(Role::InverseOnSurface, neutrals.inverseOnSurface);

        set(Role::Error, status.error);
        set(Role::OnError, status.onError);
        set(Role::ErrorContainer, status.errorContainer);
        set(Role::OnErrorContainer, status.onErrorContainer);

        set(Role::Green, status.green);
        set(Role::GreenContainer, status.greenContainer);
        set(Role::OnGreenContainer, status.onGreenContainer);
        set(Role::Amber, status.amber);
        set(Role::AmberContainer, status.amberContainer);
        set(Role::OnAmberContainer, status.onAmberContainer);
    }

    QColor ColorScheme::color(Role role) const
    {
        return m_colors.value(static_cast<int>(role));
    }

    QColor ColorScheme::overlay(Role role, qreal alpha) const
    {
        return blend(color(role), color(Role::Surface), alpha);
    }

    QColor ColorScheme::healthColor(Health health) const
    {
        switch (health) {
        case Health::Ok:
            return color(Role::Green);
        case Health::Weak:
        case Health::Reused:
            return color(Role::Amber);
        case Health::Breached:
            return color(Role::Error);
        case Health::Unknown:
            break;
        }
        return color(Role::OnSurfaceVariant);
    }

    QColor ColorScheme::healthContainer(Health health) const
    {
        switch (health) {
        case Health::Ok:
            return color(Role::GreenContainer);
        case Health::Weak:
        case Health::Reused:
            return color(Role::AmberContainer);
        case Health::Breached:
            return color(Role::ErrorContainer);
        case Health::Unknown:
            break;
        }
        return color(Role::SurfaceContainerHigh);
    }

    QColor ColorScheme::onHealthContainer(Health health) const
    {
        switch (health) {
        case Health::Ok:
            return color(Role::OnGreenContainer);
        case Health::Weak:
        case Health::Reused:
            return color(Role::OnAmberContainer);
        case Health::Breached:
            return color(Role::OnErrorContainer);
        case Health::Unknown:
            break;
        }
        return color(Role::OnSurface);
    }

    // ---------------------------------------------------------------------- Theme

    Theme* Theme::instance()
    {
        static Theme* s_instance = new Theme();
        return s_instance;
    }

    Theme::Theme()
    {
        reload();
    }

    void Theme::reload()
    {
        m_seed = seedFromString(config()->get(Config::GUI_MaterialSeed).toString());
        m_density = densityFromString(config()->get(Config::GUI_MaterialDensity).toString());
        m_configuredMode = config()->get(Config::GUI_ApplicationTheme).toString();
        resolveMode();
        emit changed();
    }

    void Theme::resolveMode()
    {
        Mode mode;
        if (m_configuredMode == QLatin1String("dark")) {
            mode = Mode::Dark;
        } else if (m_configuredMode == QLatin1String("light")) {
            mode = Mode::Light;
        } else {
            mode = osUtils->isDarkMode() ? Mode::Dark : Mode::Light;
        }
        m_colors = ColorScheme(m_seed, mode);
    }

    void Theme::setSeed(Seed seed)
    {
        if (seed == m_seed) {
            return;
        }
        m_seed = seed;
        config()->set(Config::GUI_MaterialSeed, seedToString(seed));
        resolveMode();
        emit changed();
    }

    void Theme::setMode(Mode mode)
    {
        const QString value = mode == Mode::Dark ? QStringLiteral("dark") : QStringLiteral("light");
        if (value == m_configuredMode) {
            return;
        }
        m_configuredMode = value;
        config()->set(Config::GUI_ApplicationTheme, value);
        resolveMode();
        emit changed();
    }

    void Theme::setDensity(Density density)
    {
        if (density == m_density) {
            return;
        }
        m_density = density;
        config()->set(Config::GUI_MaterialDensity, densityToString(density));
        emit changed();
    }

    int Theme::rowHeight() const
    {
        switch (m_density) {
        case Density::Compact:
            return 40;
        case Density::Spacious:
            return 64;
        case Density::Comfortable:
            break;
        }
        return 52;
    }

    int Theme::pagePadding() const
    {
        switch (m_density) {
        case Density::Compact:
            return 14;
        case Density::Spacious:
            return 30;
        case Density::Comfortable:
            break;
        }
        return 22;
    }

    QString Theme::uiFamily()
    {
        static const QString family = [] {
            const auto families = QFontDatabase::families();
            for (const auto& candidate : {QStringLiteral("Roboto"), QStringLiteral("Roboto Flex")}) {
                if (families.contains(candidate)) {
                    return candidate;
                }
            }
            return QApplication::font().family();
        }();
        return family;
    }

    QString Theme::monoFamily()
    {
        static const QString family = [] {
            const auto families = QFontDatabase::families();
            for (const auto& candidate : {QStringLiteral("Roboto Mono"),
                                          QStringLiteral("JetBrains Mono"),
                                          QStringLiteral("Cascadia Mono"),
                                          QStringLiteral("Consolas"),
                                          QStringLiteral("DejaVu Sans Mono"),
                                          QStringLiteral("Menlo")}) {
                if (families.contains(candidate)) {
                    return candidate;
                }
            }
            return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
        }();
        return family;
    }

    QFont Theme::font(TypeRole role) const
    {
        // The base size follows the application font so the accessibility font
        // size setting keeps working; the scale is expressed as a delta from 14.
        const int base = QApplication::font().pointSize() > 0 ? QApplication::font().pointSize() : 10;
        auto scaled = [base](int designPx) {
            // The design is specified at 14px body text; keep the ratio.
            return qMax(1, qRound(base * designPx / 14.0));
        };

        QFont f(uiFamily());
        switch (role) {
        case TypeRole::DisplaySmall:
            f.setPointSize(scaled(44));
            f.setWeight(QFont::Light);
            break;
        case TypeRole::HeadlineSmall:
            f.setPointSize(scaled(28));
            f.setWeight(QFont::Normal);
            break;
        case TypeRole::TitleLarge:
            f.setPointSize(scaled(22));
            f.setWeight(QFont::Normal);
            break;
        case TypeRole::TitleMedium:
            f.setPointSize(scaled(18));
            f.setWeight(QFont::Normal);
            break;
        case TypeRole::TitleSmall:
            f.setPointSize(scaled(17));
            f.setWeight(QFont::Normal);
            break;
        case TypeRole::BodyLarge:
            f.setPointSize(scaled(15));
            break;
        case TypeRole::BodyMedium:
            f.setPointSize(scaled(14));
            break;
        case TypeRole::LabelLarge:
            f.setPointSize(scaled(14));
            f.setWeight(QFont::Medium);
            break;
        case TypeRole::BodySmall:
            f.setPointSize(scaled(13));
            break;
        case TypeRole::LabelMedium:
            f.setPointSize(scaled(12));
            f.setWeight(QFont::Medium);
            break;
        case TypeRole::LabelSmall:
            f.setPointSize(scaled(11));
            f.setWeight(QFont::Medium);
            break;
        case TypeRole::Mono:
            f.setFamily(monoFamily());
            f.setPointSize(scaled(14));
            break;
        }
        return f;
    }

    QPalette Theme::palette() const
    {
        QPalette p;
        const auto surface = color(Role::Surface);
        const auto onSurface = color(Role::OnSurface);
        const auto container = color(Role::SurfaceContainer);
        const auto containerLowest = color(Role::SurfaceContainerLowest);
        const auto primary = color(Role::Primary);
        const auto onPrimary = color(Role::OnPrimary);
        const auto disabled = blend(onSurface, surface, 0.38);

        p.setColor(QPalette::Window, surface);
        p.setColor(QPalette::WindowText, onSurface);
        p.setColor(QPalette::Base, containerLowest);
        p.setColor(QPalette::AlternateBase, color(Role::SurfaceContainerLow));
        p.setColor(QPalette::Text, onSurface);
        p.setColor(QPalette::PlaceholderText, color(Role::OnSurfaceVariant));
        p.setColor(QPalette::Button, container);
        p.setColor(QPalette::ButtonText, onSurface);
        p.setColor(QPalette::BrightText, color(Role::Error));
        p.setColor(QPalette::Highlight, primary);
        p.setColor(QPalette::HighlightedText, onPrimary);
        p.setColor(QPalette::ToolTipBase, color(Role::InverseSurface));
        p.setColor(QPalette::ToolTipText, color(Role::InverseOnSurface));
        p.setColor(QPalette::Link, primary);
        p.setColor(QPalette::LinkVisited, color(Role::OnPrimaryContainer));
        p.setColor(QPalette::Light, color(Role::SurfaceContainerLowest));
        p.setColor(QPalette::Midlight, color(Role::SurfaceContainerLow));
        p.setColor(QPalette::Mid, color(Role::OutlineVariant));
        p.setColor(QPalette::Dark, color(Role::Outline));
        p.setColor(QPalette::Shadow, QColor(0, 0, 0, 90));

        p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
        p.setColor(QPalette::Disabled, QPalette::Text, disabled);
        p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
        p.setColor(QPalette::Disabled, QPalette::Highlight, blend(primary, surface, 0.30));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, disabled);

        // Inactive selection keeps the container tint rather than turning grey.
        p.setColor(QPalette::Inactive, QPalette::Highlight, color(Role::SecondaryContainer));
        p.setColor(QPalette::Inactive, QPalette::HighlightedText, color(Role::OnSecondaryContainer));
        return p;
    }

    QString Theme::styleSheet() const
    {
        return buildStyleSheet(*this);
    }

    // ------------------------------------------------------------ configuration

    Seed Theme::seedFromString(const QString& value)
    {
        for (int i = 0; i < static_cast<int>(std::size(SeedPalettes)); ++i) {
            if (value == QLatin1String(SeedPalettes[i].id)) {
                return static_cast<Seed>(i);
            }
        }
        return Seed::KeePass;
    }

    QString Theme::seedToString(Seed seed)
    {
        return QString::fromLatin1(paletteFor(seed).id);
    }

    QString Theme::seedDisplayName(Seed seed)
    {
        return QString::fromLatin1(paletteFor(seed).displayName);
    }

    QColor Theme::seedSwatch(Seed seed)
    {
        return QColor(paletteFor(seed).swatch);
    }

    Density Theme::densityFromString(const QString& value)
    {
        if (value == QLatin1String("compact")) {
            return Density::Compact;
        }
        if (value == QLatin1String("spacious")) {
            return Density::Spacious;
        }
        return Density::Comfortable;
    }

    QString Theme::densityToString(Density density)
    {
        switch (density) {
        case Density::Compact:
            return QStringLiteral("compact");
        case Density::Spacious:
            return QStringLiteral("spacious");
        case Density::Comfortable:
            break;
        }
        return QStringLiteral("comfortable");
    }

    Health Theme::healthFromString(const QString& value)
    {
        if (value == QLatin1String("ok")) {
            return Health::Ok;
        }
        if (value == QLatin1String("weak")) {
            return Health::Weak;
        }
        if (value == QLatin1String("reuse") || value == QLatin1String("reused")) {
            return Health::Reused;
        }
        if (value == QLatin1String("breach") || value == QLatin1String("breached")) {
            return Health::Breached;
        }
        return Health::Unknown;
    }

} // namespace Material
