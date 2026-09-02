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
            // Typography, to the word-processor depth the editor offers.
            std::optional<QString> fontFamily;
            std::optional<int> fontWeight; // 100..900
            std::optional<bool> italic;
            std::optional<bool> underline;
            std::optional<bool> strikeout;
            std::optional<bool> overline;
            std::optional<double> letterSpacing; // px
            std::optional<double> lineHeight; // multiplier, 0.8..3
            std::optional<int> capitalization; // QFont::Capitalization
            // Shape, layout and elevation.
            std::optional<int> elevation; // 0..5
            std::optional<int> borderWidth; // px
            std::optional<QColor> borderColor;
            std::optional<double> opacity; // 0..1
            // The animated rainbow replaces the background when set; the level
            // is the speed 1..5. It is a flag, never a colour string.
            std::optional<bool> rainbow;
            std::optional<int> rainbowLevel;
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
