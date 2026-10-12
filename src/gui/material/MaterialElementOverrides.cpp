#include "MaterialElementOverrides.h"

#include "core/Config.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QSignalBlocker>

namespace Material
{
    QJsonObject ElementOverrides::Override::toJson() const
    {
        QJsonObject object;
        if (height) object[QStringLiteral("height")] = *height;
        if (radius) object[QStringLiteral("radius")] = *radius;
        if (fontSize) object[QStringLiteral("fontSize")] = *fontSize;
        if (spacing) object[QStringLiteral("spacing")] = *spacing;
        if (background) object[QStringLiteral("background")] = background->name(QColor::HexArgb);
        if (foreground) object[QStringLiteral("foreground")] = foreground->name(QColor::HexArgb);
        if (fontFamily) object[QStringLiteral("fontFamily")] = *fontFamily;
        if (fontWeight) object[QStringLiteral("fontWeight")] = *fontWeight;
        if (italic) object[QStringLiteral("italic")] = *italic;
        if (underline) object[QStringLiteral("underline")] = *underline;
        if (strikeout) object[QStringLiteral("strikeout")] = *strikeout;
        if (overline) object[QStringLiteral("overline")] = *overline;
        if (letterSpacing) object[QStringLiteral("letterSpacing")] = *letterSpacing;
        if (lineHeight) object[QStringLiteral("lineHeight")] = *lineHeight;
        if (capitalization) object[QStringLiteral("capitalization")] = *capitalization;
        if (elevation) object[QStringLiteral("elevation")] = *elevation;
        if (borderWidth) object[QStringLiteral("borderWidth")] = *borderWidth;
        if (borderColor) object[QStringLiteral("borderColor")] = borderColor->name(QColor::HexArgb);
        if (opacity) object[QStringLiteral("opacity")] = *opacity;
        if (rainbow) object[QStringLiteral("rainbow")] = *rainbow;
        if (rainbowLevel) object[QStringLiteral("rainbowLevel")] = *rainbowLevel;
        return object;
    }

    ElementOverrides::Override ElementOverrides::Override::fromJson(const QJsonObject& object)
    {
        Override value;
        if (object.value(QStringLiteral("height")).isDouble()) value.height = qBound(24, object.value(QStringLiteral("height")).toInt(), 160);
        if (object.value(QStringLiteral("radius")).isDouble()) value.radius = qBound(0, object.value(QStringLiteral("radius")).toInt(), 80);
        if (object.value(QStringLiteral("fontSize")).isDouble()) value.fontSize = qBound(8, object.value(QStringLiteral("fontSize")).toInt(), 48);
        if (object.value(QStringLiteral("spacing")).isDouble()) value.spacing = qBound(0, object.value(QStringLiteral("spacing")).toInt(), 48);
        const QColor background(object.value(QStringLiteral("background")).toString());
        if (background.isValid()) value.background = background;
        const QColor foreground(object.value(QStringLiteral("foreground")).toString());
        if (foreground.isValid()) value.foreground = foreground;
        if (object.value(QStringLiteral("fontFamily")).isString()) {
            const QString family = object.value(QStringLiteral("fontFamily")).toString().left(120);
            if (!family.isEmpty()) value.fontFamily = family;
        }
        if (object.value(QStringLiteral("fontWeight")).isDouble()) value.fontWeight = qBound(100, object.value(QStringLiteral("fontWeight")).toInt(), 900);
        auto flag = [&object](const char* key, std::optional<bool>& target) {
            const QJsonValue v = object.value(QLatin1String(key));
            if (v.isBool()) target = v.toBool();
        };
        flag("italic", value.italic);
        flag("underline", value.underline);
        flag("strikeout", value.strikeout);
        flag("overline", value.overline);
        flag("rainbow", value.rainbow);
        if (object.value(QStringLiteral("letterSpacing")).isDouble()) value.letterSpacing = qBound(-2.0, object.value(QStringLiteral("letterSpacing")).toDouble(), 12.0);
        if (object.value(QStringLiteral("lineHeight")).isDouble()) value.lineHeight = qBound(0.8, object.value(QStringLiteral("lineHeight")).toDouble(), 3.0);
        if (object.value(QStringLiteral("capitalization")).isDouble()) value.capitalization = qBound(0, object.value(QStringLiteral("capitalization")).toInt(), 4);
        if (object.value(QStringLiteral("elevation")).isDouble()) value.elevation = qBound(0, object.value(QStringLiteral("elevation")).toInt(), 5);
        if (object.value(QStringLiteral("borderWidth")).isDouble()) value.borderWidth = qBound(0, object.value(QStringLiteral("borderWidth")).toInt(), 8);
        const QColor border(object.value(QStringLiteral("borderColor")).toString());
        if (border.isValid()) value.borderColor = border;
        if (object.value(QStringLiteral("opacity")).isDouble()) value.opacity = qBound(0.0, object.value(QStringLiteral("opacity")).toDouble(), 1.0);
        if (object.value(QStringLiteral("rainbowLevel")).isDouble()) value.rainbowLevel = qBound(1, object.value(QStringLiteral("rainbowLevel")).toInt(), 5);
        return value;
    }

    bool ElementOverrides::Override::isEmpty() const
    {
        return !height && !radius && !fontSize && !spacing && !background && !foreground && !fontFamily && !fontWeight
               && !italic && !underline && !strikeout && !overline && !letterSpacing && !lineHeight && !capitalization
               && !elevation && !borderWidth && !borderColor && !opacity && !rainbow && !rainbowLevel;
    }

    ElementOverrides::ElementOverrides()
    {
        load();
    }

    ElementOverrides* ElementOverrides::instance()
    {
        static ElementOverrides instance;
        return &instance;
    }

    ElementOverrides::Override ElementOverrides::get(const QString& key) const { return m_overrides.value(key); }

    void ElementOverrides::set(const QString& key, const Override& value)
    {
        if (key.isEmpty()) return;
        if (value.isEmpty()) {
            reset(key);
            return;
        }
        m_overrides.insert(key, value);
        save();
        emit overrideChanged(key);
    }

    void ElementOverrides::reset(const QString& key)
    {
        if (m_overrides.remove(key)) {
            save();
            emit overrideChanged(key);
        }
    }

    void ElementOverrides::resetAll()
    {
        const auto keys = m_overrides.keys();
        m_overrides.clear();
        save();
        for (const auto& key : keys) emit overrideChanged(key);
    }

    QStringList ElementOverrides::customisedKeys() const { return m_overrides.keys(); }

    void ElementOverrides::migrateLegacy(const QString& source, const QString& target)
    {
        if (!m_migrationStateValid || source.isEmpty() || target.isEmpty()
            || m_consumedMigrations.contains(target)) return;
        const auto value = get(source);
        if (value.isEmpty()) return;
        // Consume independently of the current style. Resetting a migrated or
        // already-customized target must not resurrect its retained legacy alias.
        m_consumedMigrations.insert(target);
        const bool copied = get(target).isEmpty();
        if (copied) m_overrides.insert(target, value);
        save();
        config()->sync();
        if (copied) emit overrideChanged(target);
    }

    void ElementOverrides::load()
    {
        m_overrides.clear();
        m_consumedMigrations.clear();
        QJsonParseError migrationError;
        const auto migrations = QJsonDocument::fromJson(config()->get(Config::GUI_ElementOverrideMigrations).toString().toUtf8(), &migrationError);
        const auto state = migrations.object();
        m_migrationStateValid = migrationError.error == QJsonParseError::NoError && migrations.isObject()
                                && state.value(QStringLiteral("version")).toInt() == 1
                                && state.value(QStringLiteral("targets")).isArray();
        if (m_migrationStateValid) {
            for (const auto& target : state.value(QStringLiteral("targets")).toArray()) {
                if (!target.isString() || target.toString().isEmpty()) {
                    m_migrationStateValid = false;
                    break;
                }
                m_consumedMigrations.insert(target.toString());
            }
        }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(config()->get(Config::GUI_ElementOverrides).toString().toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) return;
        const auto root = document.object();
        for (auto it = root.begin(); it != root.end(); ++it) {
            if (!it.value().isObject()) continue;
            const auto value = Override::fromJson(it.value().toObject());
            if (!value.isEmpty()) m_overrides.insert(it.key(), value);
        }
    }

    void ElementOverrides::save() const
    {
        if (m_migrationStateValid) {
            auto targets = m_consumedMigrations.values();
            targets.sort();
            QJsonArray consumed;
            for (const auto& target : targets) consumed.append(target);
            const QJsonObject migrations{{QStringLiteral("version"), 1}, {QStringLiteral("targets"), consumed}};
            // Both fields share the local INI. Do not notify observers between
            // recording consumption and publishing the corresponding style.
            const QSignalBlocker blocker(config());
            config()->set(Config::GUI_ElementOverrideMigrations, QString::fromUtf8(QJsonDocument(migrations).toJson(QJsonDocument::Compact)));
        }
        QJsonObject root;
        for (auto it = m_overrides.cbegin(); it != m_overrides.cend(); ++it) root[it.key()] = it.value().toJson();
        config()->set(Config::GUI_ElementOverrides, QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
    }
}
