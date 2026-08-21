#include "MaterialElementOverrides.h"

#include "core/Config.h"
#include <QJsonDocument>

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
        return value;
    }

    bool ElementOverrides::Override::isEmpty() const
    {
        return !height && !radius && !fontSize && !spacing && !background && !foreground;
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

    void ElementOverrides::load()
    {
        m_overrides.clear();
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
        QJsonObject root;
        for (auto it = m_overrides.cbegin(); it != m_overrides.cend(); ++it) root[it.key()] = it.value().toJson();
        config()->set(Config::GUI_ElementOverrides, QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
    }
}
