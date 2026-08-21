#ifndef KEEPASSXC_MATERIALELEMENTOVERRIDES_H
#define KEEPASSXC_MATERIALELEMENTOVERRIDES_H

#include <QColor>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <optional>

namespace Material
{
    class ElementOverrides : public QObject
    {
        Q_OBJECT
    public:
        struct Override
        {
            std::optional<int> height;
            std::optional<int> radius;
            std::optional<int> fontSize;
            std::optional<int> spacing;
            std::optional<QColor> background;
            std::optional<QColor> foreground;
            QJsonObject toJson() const;
            static Override fromJson(const QJsonObject& object);
            bool isEmpty() const;
        };

        static ElementOverrides* instance();
        Override get(const QString& key) const;
        void set(const QString& key, const Override& value);
        void reset(const QString& key);
        void resetAll();
        QStringList customisedKeys() const;
        void load();
        void save() const;

    signals:
        void overrideChanged(const QString& key);

    private:
        ElementOverrides();
        QHash<QString, Override> m_overrides;
    };
}

#endif
