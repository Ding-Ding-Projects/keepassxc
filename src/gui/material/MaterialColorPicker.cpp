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

#include "MaterialColorPicker.h"

#include "MaterialElevation.h"
#include "MaterialSlider.h"
#include "MaterialSwitch.h"
#include "MaterialTheme.h"

#include <QAbstractButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QtMath>

#include <functional>

namespace Material
{
    // ------------------------------------------------------------ ColorText

    namespace
    {
        constexpr int FieldHeight = 160;
        constexpr int BarHeight = 20;
        constexpr int BarRadius = 10;
        constexpr int SwatchSize = 44;
        constexpr int RecentSize = 26;
        constexpr int MaxRecent = 8;
        constexpr int ThumbRadius = 8;

        double srgbToLinear(double channel)
        {
            return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        }

        double linearToSrgb(double channel)
        {
            return channel <= 0.0031308 ? channel * 12.92 : 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
        }

        QString number(double value, int decimals = 1)
        {
            QString text = QString::number(value, 'f', decimals);
            if (text.contains(QLatin1Char('.'))) {
                while (text.endsWith(QLatin1Char('0'))) {
                    text.chop(1);
                }
                if (text.endsWith(QLatin1Char('.'))) {
                    text.chop(1);
                }
            }
            if (text == QLatin1String("-0")) {
                text = QStringLiteral("0");
            }
            return text;
        }

        QString percent(double fraction)
        {
            return number(fraction * 100.0, 1) + QLatin1Char('%');
        }

        struct Lab
        {
            double l;
            double a;
            double b;
        };

        // CIE XYZ (D65) from linear sRGB.
        void toXyz(const QColor& color, double& x, double& y, double& z)
        {
            const double r = srgbToLinear(color.redF());
            const double g = srgbToLinear(color.greenF());
            const double b = srgbToLinear(color.blueF());
            x = 0.4124564 * r + 0.3575761 * g + 0.1804375 * b;
            y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
            z = 0.0193339 * r + 0.1191920 * g + 0.9503041 * b;
        }

        Lab cieLab(const QColor& color)
        {
            double x, y, z;
            toXyz(color, x, y, z);
            const double xn = 0.95047, yn = 1.0, zn = 1.08883;
            auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : (7.787 * t) + 16.0 / 116.0; };
            const double fx = f(x / xn), fy = f(y / yn), fz = f(z / zn);
            return {116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
        }

        Lab okLab(const QColor& color)
        {
            const double r = srgbToLinear(color.redF());
            const double g = srgbToLinear(color.greenF());
            const double b = srgbToLinear(color.blueF());
            const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
            const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
            const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
            return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
                    1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
                    0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
        }

        QColor fromOkLab(double L, double a, double b, double alpha)
        {
            const double l_ = L + 0.3963377774 * a + 0.2158037573 * b;
            const double m_ = L - 0.1055613458 * a - 0.0638541728 * b;
            const double s_ = L - 0.0894841775 * a - 1.2914855480 * b;
            const double l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
            const double r = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
            const double g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
            const double bb = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;
            QColor color;
            color.setRgbF(qBound(0.0, linearToSrgb(r), 1.0), qBound(0.0, linearToSrgb(g), 1.0), qBound(0.0, linearToSrgb(bb), 1.0), alpha);
            return color;
        }

        QColor fromCieLab(double L, double a, double b, double alpha)
        {
            const double fy = (L + 16.0) / 116.0, fx = fy + a / 500.0, fz = fy - b / 200.0;
            auto finv = [](double t) { const double t3 = t * t * t; return t3 > 0.008856 ? t3 : (t - 16.0 / 116.0) / 7.787; };
            const double x = 0.95047 * finv(fx), y = 1.0 * finv(fy), z = 1.08883 * finv(fz);
            const double r = 3.2404542 * x - 1.5371385 * y - 0.4985314 * z;
            const double g = -0.9692660 * x + 1.8760108 * y + 0.0415560 * z;
            const double bb = 0.0556434 * x - 0.2040259 * y + 1.0572252 * z;
            QColor color;
            color.setRgbF(qBound(0.0, linearToSrgb(r), 1.0), qBound(0.0, linearToSrgb(g), 1.0), qBound(0.0, linearToSrgb(bb), 1.0), alpha);
            return color;
        }

        double hueDegrees(double a, double b)
        {
            double hue = qRadiansToDegrees(std::atan2(b, a));
            if (hue < 0) hue += 360.0;
            return hue;
        }

        QString alphaSuffix(const QColor& color)
        {
            return color.alpha() == 255 ? QString() : QStringLiteral(" / %1").arg(number(color.alphaF(), 2));
        }

        QStringList numbersIn(const QString& text)
        {
            static const QRegularExpression pattern(QStringLiteral("-?\\d+(?:\\.\\d+)?%?"));
            QStringList out;
            auto it = pattern.globalMatch(text);
            while (it.hasNext()) {
                out << it.next().captured(0);
            }
            return out;
        }

        double numberValue(const QString& token, double percentScale)
        {
            if (token.endsWith(QLatin1Char('%'))) {
                return token.chopped(1).toDouble() / 100.0 * percentScale;
            }
            return token.toDouble();
        }
    } // namespace

    namespace ColorText
    {
        QString hex(const QColor& color)
        {
            const QString rgb = color.name(QColor::HexRgb).toUpper();
            if (color.alpha() == 255) {
                return rgb;
            }
            return rgb + QStringLiteral("%1").arg(color.alpha(), 2, 16, QLatin1Char('0')).toUpper();
        }

        QString rgb(const QColor& color)
        {
            return QStringLiteral("rgb(%1 %2 %3%4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alphaSuffix(color));
        }

        QString hsl(const QColor& color)
        {
            const double h = color.hslHueF() < 0 ? 0 : color.hslHueF() * 360.0;
            return QStringLiteral("hsl(%1 %2 %3%4)").arg(number(h)).arg(percent(color.hslSaturationF())).arg(percent(color.lightnessF())).arg(alphaSuffix(color));
        }

        QString hsv(const QColor& color)
        {
            const double h = color.hsvHueF() < 0 ? 0 : color.hsvHueF() * 360.0;
            return QStringLiteral("hsv(%1 %2 %3%4)").arg(number(h)).arg(percent(color.hsvSaturationF())).arg(percent(color.valueF())).arg(alphaSuffix(color));
        }

        QString hwb(const QColor& color)
        {
            const double h = color.hsvHueF() < 0 ? 0 : color.hsvHueF() * 360.0;
            const double w = qMin(color.redF(), qMin(color.greenF(), color.blueF()));
            const double b = 1.0 - qMax(color.redF(), qMax(color.greenF(), color.blueF()));
            return QStringLiteral("hwb(%1 %2 %3%4)").arg(number(h)).arg(percent(w)).arg(percent(b)).arg(alphaSuffix(color));
        }

        QString cmyk(const QColor& color)
        {
            const QColor c = color.toCmyk();
            return QStringLiteral("cmyk(%1 %2 %3 %4%5)").arg(percent(c.cyanF())).arg(percent(c.magentaF())).arg(percent(c.yellowF())).arg(percent(c.blackF())).arg(alphaSuffix(color));
        }

        QString lab(const QColor& color)
        {
            const Lab v = cieLab(color);
            return QStringLiteral("lab(%1 %2 %3%4)").arg(number(v.l)).arg(number(v.a)).arg(number(v.b)).arg(alphaSuffix(color));
        }

        QString lch(const QColor& color)
        {
            const Lab v = cieLab(color);
            const double chroma = std::hypot(v.a, v.b);
            return QStringLiteral("lch(%1 %2 %3%4)").arg(number(v.l)).arg(number(chroma)).arg(number(chroma < 0.05 ? 0 : hueDegrees(v.a, v.b))).arg(alphaSuffix(color));
        }

        QString oklab(const QColor& color)
        {
            const Lab v = okLab(color);
            return QStringLiteral("oklab(%1 %2 %3%4)").arg(number(v.l, 3)).arg(number(v.a, 3)).arg(number(v.b, 3)).arg(alphaSuffix(color));
        }

        QString oklch(const QColor& color)
        {
            const Lab v = okLab(color);
            const double chroma = std::hypot(v.a, v.b);
            return QStringLiteral("oklch(%1 %2 %3%4)").arg(number(v.l, 3)).arg(number(chroma, 3)).arg(number(chroma < 0.0005 ? 0 : hueDegrees(v.a, v.b))).arg(alphaSuffix(color));
        }

        QString name(const QColor& color)
        {
            if (color.alpha() != 255) {
                return QString();
            }
            const QStringList names = QColor::colorNames();
            for (const QString& candidate : names) {
                if (QColor(candidate) == color) {
                    return candidate;
                }
            }
            return QString();
        }

        QList<QPair<QString, QString>> all(const QColor& color)
        {
            return {{QStringLiteral("HEX"), hex(color)},
                    {QStringLiteral("RGB"), rgb(color)},
                    {QStringLiteral("HSL"), hsl(color)},
                    {QStringLiteral("HSV"), hsv(color)},
                    {QStringLiteral("HWB"), hwb(color)},
                    {QStringLiteral("CMYK"), cmyk(color)},
                    {QStringLiteral("LAB"), lab(color)},
                    {QStringLiteral("LCH"), lch(color)},
                    {QStringLiteral("OKLAB"), oklab(color)},
                    {QStringLiteral("OKLCH"), oklch(color)}};
        }

        QColor parse(const QString& raw)
        {
            const QString text = raw.trimmed().toLower();
            if (text.isEmpty()) {
                return QColor();
            }
            if (text.startsWith(QLatin1Char('#'))) {
                const QString digits = text.mid(1);
                if (digits.size() == 8) {
                    // #RRGGBBAA, as the picker writes it.
                    const QColor base(QStringLiteral("#") + digits.left(6));
                    if (!base.isValid()) return QColor();
                    bool ok = false;
                    const int alpha = digits.mid(6, 2).toInt(&ok, 16);
                    QColor color = base;
                    color.setAlpha(ok ? alpha : 255);
                    return color;
                }
                return QColor(text);
            }
            const int open = text.indexOf(QLatin1Char('('));
            if (open < 0) {
                return QColor(text); // a CSS name
            }
            const QString kind = text.left(open).trimmed();
            const QStringList values = numbersIn(text.mid(open));
            if (values.size() < 3) {
                return QColor();
            }
            double alpha = 1.0;
            if (values.size() >= 4 && kind != QLatin1String("cmyk")) {
                alpha = qBound(0.0, numberValue(values.at(3), 1.0), 1.0);
            } else if (values.size() >= 5 && kind == QLatin1String("cmyk")) {
                alpha = qBound(0.0, numberValue(values.at(4), 1.0), 1.0);
            }
            QColor color;
            if (kind == QLatin1String("rgb") || kind == QLatin1String("rgba")) {
                color.setRgbF(qBound(0.0, numberValue(values.at(0), 255.0) / 255.0, 1.0),
                              qBound(0.0, numberValue(values.at(1), 255.0) / 255.0, 1.0),
                              qBound(0.0, numberValue(values.at(2), 255.0) / 255.0, 1.0),
                              alpha);
            } else if (kind == QLatin1String("hsl") || kind == QLatin1String("hsla")) {
                color.setHslF(std::fmod(std::fmod(numberValue(values.at(0), 360.0), 360.0) + 360.0, 360.0) / 360.0,
                              qBound(0.0, numberValue(values.at(1), 1.0), 1.0),
                              qBound(0.0, numberValue(values.at(2), 1.0), 1.0),
                              alpha);
            } else if (kind == QLatin1String("hsv") || kind == QLatin1String("hsb")) {
                color.setHsvF(std::fmod(std::fmod(numberValue(values.at(0), 360.0), 360.0) + 360.0, 360.0) / 360.0,
                              qBound(0.0, numberValue(values.at(1), 1.0), 1.0),
                              qBound(0.0, numberValue(values.at(2), 1.0), 1.0),
                              alpha);
            } else if (kind == QLatin1String("hwb")) {
                double w = qBound(0.0, numberValue(values.at(1), 1.0), 1.0);
                double b = qBound(0.0, numberValue(values.at(2), 1.0), 1.0);
                if (w + b > 1.0) {
                    const double sum = w + b;
                    w /= sum;
                    b /= sum;
                }
                const double value = 1.0 - b;
                const double saturation = value <= 0 ? 0 : 1.0 - w / value;
                color.setHsvF(std::fmod(std::fmod(numberValue(values.at(0), 360.0), 360.0) + 360.0, 360.0) / 360.0, saturation, value, alpha);
            } else if (kind == QLatin1String("cmyk")) {
                if (values.size() < 4) return QColor();
                color.setCmykF(qBound(0.0, numberValue(values.at(0), 1.0), 1.0),
                               qBound(0.0, numberValue(values.at(1), 1.0), 1.0),
                               qBound(0.0, numberValue(values.at(2), 1.0), 1.0),
                               qBound(0.0, numberValue(values.at(3), 1.0), 1.0),
                               alpha);
                color = color.toRgb();
            } else if (kind == QLatin1String("lab")) {
                color = fromCieLab(numberValue(values.at(0), 100.0), numberValue(values.at(1), 125.0), numberValue(values.at(2), 125.0), alpha);
            } else if (kind == QLatin1String("lch")) {
                const double hue = qDegreesToRadians(numberValue(values.at(2), 360.0));
                const double chroma = numberValue(values.at(1), 150.0);
                color = fromCieLab(numberValue(values.at(0), 100.0), chroma * std::cos(hue), chroma * std::sin(hue), alpha);
            } else if (kind == QLatin1String("oklab")) {
                color = fromOkLab(numberValue(values.at(0), 1.0), numberValue(values.at(1), 0.4), numberValue(values.at(2), 0.4), alpha);
            } else if (kind == QLatin1String("oklch")) {
                const double hue = qDegreesToRadians(numberValue(values.at(2), 360.0));
                const double chroma = numberValue(values.at(1), 0.4);
                color = fromOkLab(numberValue(values.at(0), 1.0), chroma * std::cos(hue), chroma * std::sin(hue), alpha);
            } else {
                return QColor();
            }
            return color;
        }

        double contrastRatio(const QColor& foreground, const QColor& background)
        {
            auto luminance = [](const QColor& color) {
                return 0.2126 * srgbToLinear(color.redF()) + 0.7152 * srgbToLinear(color.greenF()) + 0.0722 * srgbToLinear(color.blueF());
            };
            const double a = luminance(foreground), b = luminance(background);
            const double light = qMax(a, b), dark = qMin(a, b);
            return (light + 0.05) / (dark + 0.05);
        }

        QString rainbowSentinel()
        {
            return QStringLiteral("rainbow");
        }

        bool isRainbow(const QString& stored)
        {
            return stored.compare(rainbowSentinel(), Qt::CaseInsensitive) == 0;
        }

        int rainbowCycleMs(int level)
        {
            // Level 1 is a slow drift, level 5 a brisk cycle; one table, read
            // by every surface that animates, so nothing drifts apart.
            static const int table[] = {24000, 16000, 10000, 6000, 3000};
            return table[qBound(1, level, 5) - 1];
        }
    } // namespace ColorText

    // ----------------------------------------------------------------- Field

    /** The two-dimensional saturation/value field for the current hue. */
    class ColorPicker::Field : public QWidget
    {
    public:
        explicit Field(ColorPicker* picker)
            : QWidget(picker)
            , m_picker(picker)
        {
            setMinimumHeight(FieldHeight);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            setCursor(Qt::CrossCursor);
            setFocusPolicy(Qt::StrongFocus);
            setAccessibleName(ColorPicker::tr("Saturation and brightness field"));
        }

        void setHue(double hue)
        {
            m_hue = hue;
            update();
        }

        void setPoint(double saturation, double value)
        {
            m_saturation = saturation;
            m_value = value;
            update();
        }

        std::function<void(double, double)> onChanged;

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            QPainterPath clip;
            clip.addRoundedRect(QRectF(rect()), 12, 12);
            painter.setClipPath(clip);
            QLinearGradient hue(0, 0, width(), 0);
            hue.setColorAt(0, Qt::white);
            hue.setColorAt(1, QColor::fromHsvF(m_hue, 1.0, 1.0));
            painter.fillRect(rect(), hue);
            QLinearGradient dark(0, 0, 0, height());
            dark.setColorAt(0, QColor(0, 0, 0, 0));
            dark.setColorAt(1, Qt::black);
            painter.fillRect(rect(), dark);
            painter.setClipping(false);
            const QPointF thumb(m_saturation * width(), (1.0 - m_value) * height());
            painter.setPen(QPen(Qt::white, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(thumb, ThumbRadius, ThumbRadius);
            painter.setPen(QPen(QColor(0, 0, 0, 120), 1));
            painter.drawEllipse(thumb, ThumbRadius + 1.5, ThumbRadius + 1.5);
            if (hasFocus()) {
                painter.setPen(QPen(theme()->color(Role::Primary), 2));
                painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 12, 12);
            }
        }

        void mousePressEvent(QMouseEvent* event) override { pick(event->position()); }
        void mouseMoveEvent(QMouseEvent* event) override
        {
            if (event->buttons() & Qt::LeftButton) pick(event->position());
        }

        void keyPressEvent(QKeyEvent* event) override
        {
            const double step = event->modifiers().testFlag(Qt::ShiftModifier) ? 0.1 : 0.01;
            switch (event->key()) {
            case Qt::Key_Left: emitPoint(m_saturation - step, m_value); return;
            case Qt::Key_Right: emitPoint(m_saturation + step, m_value); return;
            case Qt::Key_Up: emitPoint(m_saturation, m_value + step); return;
            case Qt::Key_Down: emitPoint(m_saturation, m_value - step); return;
            default: break;
            }
            QWidget::keyPressEvent(event);
        }

    private:
        void pick(const QPointF& position)
        {
            emitPoint(position.x() / qMax(1, width()), 1.0 - position.y() / qMax(1, height()));
        }

        void emitPoint(double saturation, double value)
        {
            m_saturation = qBound(0.0, saturation, 1.0);
            m_value = qBound(0.0, value, 1.0);
            update();
            if (onChanged) onChanged(m_saturation, m_value);
        }

        ColorPicker* m_picker;
        double m_hue = 0.5;
        double m_saturation = 1.0;
        double m_value = 0.5;
    };

    // ---------------------------------------------------------------- HueBar

    class ColorPicker::HueBar : public QWidget
    {
    public:
        explicit HueBar(QWidget* parent = nullptr)
            : QWidget(parent)
        {
            setFixedHeight(BarHeight + 8);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            setCursor(Qt::PointingHandCursor);
            setFocusPolicy(Qt::StrongFocus);
            setAccessibleName(ColorPicker::tr("Hue"));
        }

        void setHue(double hue)
        {
            m_hue = hue;
            update();
        }

        std::function<void(double)> onChanged;

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const QRectF bar(0, 4, width(), BarHeight);
            QLinearGradient gradient(0, 0, width(), 0);
            for (int step = 0; step <= 6; ++step) {
                gradient.setColorAt(step / 6.0, QColor::fromHsvF(std::fmod(step / 6.0, 1.0), 1.0, 1.0));
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(gradient);
            painter.drawRoundedRect(bar, BarRadius, BarRadius);
            const double x = m_hue * width();
            painter.setPen(QPen(Qt::white, 2));
            painter.setBrush(QColor::fromHsvF(m_hue, 1.0, 1.0));
            painter.drawEllipse(QPointF(qBound(ThumbRadius + 0.0, x, width() - ThumbRadius - 0.0), bar.center().y()), ThumbRadius, ThumbRadius);
            if (hasFocus()) {
                painter.setPen(QPen(theme()->color(Role::Primary), 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(bar.adjusted(1, 1, -1, -1), BarRadius, BarRadius);
            }
        }

        void mousePressEvent(QMouseEvent* event) override { pick(event->position().x()); }
        void mouseMoveEvent(QMouseEvent* event) override
        {
            if (event->buttons() & Qt::LeftButton) pick(event->position().x());
        }
        void keyPressEvent(QKeyEvent* event) override
        {
            const double step = event->modifiers().testFlag(Qt::ShiftModifier) ? 0.1 : 1.0 / 360.0;
            if (event->key() == Qt::Key_Left) { set(m_hue - step); return; }
            if (event->key() == Qt::Key_Right) { set(m_hue + step); return; }
            QWidget::keyPressEvent(event);
        }

    private:
        void pick(double x) { set(x / qMax(1, width())); }
        void set(double hue)
        {
            m_hue = std::fmod(std::fmod(hue, 1.0) + 1.0, 1.0);
            if (hue >= 1.0) m_hue = 0.9999;
            update();
            if (onChanged) onChanged(m_hue);
        }
        double m_hue = 0.5;
    };

    // -------------------------------------------------------------- AlphaBar

    class ColorPicker::AlphaBar : public QWidget
    {
    public:
        explicit AlphaBar(QWidget* parent = nullptr)
            : QWidget(parent)
        {
            setFixedHeight(BarHeight + 8);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            setCursor(Qt::PointingHandCursor);
            setFocusPolicy(Qt::StrongFocus);
            setAccessibleName(ColorPicker::tr("Opacity"));
        }

        void setColor(const QColor& color)
        {
            m_color = color;
            update();
        }

        std::function<void(double)> onChanged;

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const QRectF bar(0, 4, width(), BarHeight);
            QPainterPath clip;
            clip.addRoundedRect(bar, BarRadius, BarRadius);
            painter.setClipPath(clip);
            // Checkerboard so transparency reads as transparency.
            for (int x = 0; x < width(); x += 8) {
                for (int y = 4; y < 4 + BarHeight; y += 8) {
                    painter.fillRect(QRect(x, y, 8, 8), ((x / 8 + y / 8) % 2) ? QColor(200, 200, 200) : QColor(240, 240, 240));
                }
            }
            QLinearGradient gradient(0, 0, width(), 0);
            QColor start = m_color; start.setAlpha(0);
            QColor end = m_color; end.setAlpha(255);
            gradient.setColorAt(0, start);
            gradient.setColorAt(1, end);
            painter.fillRect(bar, gradient);
            painter.setClipping(false);
            const double x = m_color.alphaF() * width();
            painter.setPen(QPen(Qt::white, 2));
            painter.setBrush(m_color);
            painter.drawEllipse(QPointF(qBound(ThumbRadius + 0.0, x, width() - ThumbRadius - 0.0), bar.center().y()), ThumbRadius, ThumbRadius);
            if (hasFocus()) {
                painter.setPen(QPen(theme()->color(Role::Primary), 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(bar.adjusted(1, 1, -1, -1), BarRadius, BarRadius);
            }
        }

        void mousePressEvent(QMouseEvent* event) override { pick(event->position().x()); }
        void mouseMoveEvent(QMouseEvent* event) override
        {
            if (event->buttons() & Qt::LeftButton) pick(event->position().x());
        }
        void keyPressEvent(QKeyEvent* event) override
        {
            const double step = event->modifiers().testFlag(Qt::ShiftModifier) ? 0.1 : 0.01;
            if (event->key() == Qt::Key_Left) { set(m_color.alphaF() - step); return; }
            if (event->key() == Qt::Key_Right) { set(m_color.alphaF() + step); return; }
            QWidget::keyPressEvent(event);
        }

    private:
        void pick(double x) { set(x / qMax(1, width())); }
        void set(double alpha)
        {
            if (onChanged) onChanged(qBound(0.0, alpha, 1.0));
        }
        QColor m_color;
    };

    // ------------------------------------------------------------- RecentRow

    /** The recent colours: up to eight round swatches, each a button. */
    class ColorPicker::RecentRow : public QWidget
    {
    public:
        explicit RecentRow(QWidget* parent = nullptr)
            : QWidget(parent)
        {
            m_layout = new QHBoxLayout(this);
            m_layout->setContentsMargins(0, 0, 0, 0);
            m_layout->setSpacing(6);
            m_layout->addStretch(1);
            setAccessibleName(ColorPicker::tr("Recent colours"));
        }

        void add(const QColor& color)
        {
            m_colors.removeAll(color);
            m_colors.prepend(color);
            while (m_colors.size() > MaxRecent) m_colors.removeLast();
            rebuild();
        }

        QList<QColor> colors() const { return m_colors; }
        std::function<void(const QColor&)> onPicked;

    private:
        class Swatch : public QAbstractButton
        {
        public:
            Swatch(const QColor& color, QWidget* parent) : QAbstractButton(parent), m_color(color)
            {
                setFixedSize(RecentSize, RecentSize);
                setCursor(Qt::PointingHandCursor);
                setFocusPolicy(Qt::TabFocus);
                setAccessibleName(ColorPicker::tr("Recent colour %1").arg(ColorText::hex(color)));
                setToolTip(ColorText::hex(color));
            }
        protected:
            void paintEvent(QPaintEvent*) override
            {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(QPen(theme()->color(hasFocus() ? Role::Primary : Role::OutlineVariant), hasFocus() ? 2 : 1));
                painter.setBrush(m_color);
                painter.drawEllipse(QRectF(rect()).adjusted(1, 1, -1, -1));
            }
        private:
            QColor m_color;
        };

        void rebuild()
        {
            while (m_layout->count() > 1) {
                QLayoutItem* item = m_layout->takeAt(0);
                delete item->widget();
                delete item;
            }
            int index = 0;
            for (const QColor& color : m_colors) {
                auto* swatch = new Swatch(color, this);
                QObject::connect(swatch, &QAbstractButton::clicked, this, [this, color] { if (onPicked) onPicked(color); });
                m_layout->insertWidget(index++, swatch);
            }
        }

        QHBoxLayout* m_layout = nullptr;
        QList<QColor> m_colors;
    };

    // ----------------------------------------------------------- ColorPicker

    ColorPicker::ColorPicker(QWidget* parent)
        : QWidget(parent)
    {
        buildUi();
        connect(theme(), &Theme::changed, this, [this] { applyTheme(); });
        applyTheme();
        syncFromColor(false);
    }

    ColorPicker::~ColorPicker()
    {
        // The line edits outlive this object's members by a moment during
        // teardown; a focus-out then must not reach applyNotation().
        for (QLineEdit* edit : std::as_const(m_edits)) {
            edit->disconnect(this);
        }
    }

    void ColorPicker::buildUi()
    {
        auto* column = new QVBoxLayout(this);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(8);

        m_field = new Field(this);
        m_field->setObjectName(QStringLiteral("colorPickerField"));
        m_field->onChanged = [this](double saturation, double value) {
            QColor next = QColor::fromHsvF(qBound(0.0, m_color.hsvHueF() < 0 ? 0.0 : m_color.hsvHueF(), 0.9999), saturation, value, m_color.alphaF());
            m_color = next;
            syncFromColor(true);
        };
        column->addWidget(m_field);

        m_hue = new HueBar(this);
        m_hue->setObjectName(QStringLiteral("colorPickerHue"));
        m_hue->onChanged = [this](double hue) {
            const double saturation = m_color.hsvSaturationF();
            const double value = m_color.valueF();
            m_color = QColor::fromHsvF(hue, saturation, value, m_color.alphaF());
            syncFromColor(true);
        };
        column->addWidget(m_hue);

        m_alpha = new AlphaBar(this);
        m_alpha->setObjectName(QStringLiteral("colorPickerAlpha"));
        m_alpha->onChanged = [this](double alpha) {
            m_color.setAlphaF(alpha);
            syncFromColor(true);
        };
        column->addWidget(m_alpha);

        auto* summary = new QHBoxLayout;
        summary->setSpacing(10);
        m_swatch = new QLabel(this);
        m_swatch->setObjectName(QStringLiteral("colorPickerSwatch"));
        m_swatch->setFixedSize(SwatchSize, SwatchSize);
        m_swatch->setAccessibleName(tr("Current colour"));
        summary->addWidget(m_swatch, 0, Qt::AlignTop);
        auto* facts = new QVBoxLayout;
        facts->setSpacing(2);
        m_nameLabel = new QLabel(this);
        m_nameLabel->setObjectName(QStringLiteral("colorPickerName"));
        facts->addWidget(m_nameLabel);
        m_contrast = new QLabel(this);
        m_contrast->setObjectName(QStringLiteral("colorPickerContrast"));
        m_contrast->setWordWrap(true);
        facts->addWidget(m_contrast);
        m_gamutLabel = new QLabel(tr("sRGB · in gamut"), this);
        m_gamutLabel->setObjectName(QStringLiteral("colorPickerGamut"));
        facts->addWidget(m_gamutLabel);
        summary->addLayout(facts, 1);
        column->addLayout(summary);

        // The translator: every notation, each editable, each a copy target.
        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(4);
        int row = 0;
        const auto notations = ColorText::all(m_color);
        for (const auto& pair : notations) {
            auto* label = new QLabel(pair.first, this);
            label->setMinimumWidth(48);
            auto* edit = new QLineEdit(this);
            edit->setObjectName(QStringLiteral("colorPicker_") + pair.first.toLower());
            edit->setAccessibleName(tr("%1 colour value").arg(pair.first));
            const QString key = pair.first;
            connect(edit, &QLineEdit::editingFinished, this, [this, key] { applyNotation(key); });
            m_edits.insert(pair.first, edit);
            grid->addWidget(label, row, 0);
            grid->addWidget(edit, row, 1);
            ++row;
        }
        column->addLayout(grid);

        // The animated rainbow is one of the choices, from the same control.
        auto* rainbowRow = new QHBoxLayout;
        rainbowRow->setSpacing(10);
        m_rainbowCaption = new QLabel(tr("Animated rainbow"), this);
        rainbowRow->addWidget(m_rainbowCaption, 1);
        m_rainbowSwitch = new Switch(this);
        m_rainbowSwitch->setObjectName(QStringLiteral("colorPickerRainbow"));
        m_rainbowSwitch->setAccessibleName(tr("Animated rainbow instead of one colour"));
        connect(m_rainbowSwitch, &QAbstractButton::toggled, this, [this](bool on) {
            m_rainbow = on;
            m_rainbowSpeed->setEnabled(on);
            emit rainbowChanged(m_rainbow, m_rainbowLevel);
        });
        rainbowRow->addWidget(m_rainbowSwitch, 0);
        column->addLayout(rainbowRow);
        m_rainbowSpeed = new Slider(Qt::Horizontal, this);
        m_rainbowSpeed->setObjectName(QStringLiteral("colorPickerRainbowSpeed"));
        m_rainbowSpeed->setAccessibleName(tr("Rainbow speed, 1 slow to 5 brisk"));
        m_rainbowSpeed->setRange(1, 5);
        m_rainbowSpeed->setValue(m_rainbowLevel);
        m_rainbowSpeed->setEnabled(false);
        connect(m_rainbowSpeed, &QSlider::valueChanged, this, [this](int level) {
            m_rainbowLevel = level;
            if (m_rainbow) emit rainbowChanged(true, level);
        });
        column->addWidget(m_rainbowSpeed);

        m_recent = new RecentRow(this);
        m_recent->setObjectName(QStringLiteral("colorPickerRecent"));
        m_recent->onPicked = [this](const QColor& color) { setColor(color); emit colorChanged(m_color); };
        column->addWidget(m_recent);
    }

    QColor ColorPicker::color() const
    {
        return m_color;
    }

    void ColorPicker::setColor(const QColor& color)
    {
        if (!color.isValid()) {
            return;
        }
        m_color = color;
        syncFromColor(false);
    }

    QColor ColorPicker::referenceColor() const
    {
        return m_reference;
    }

    void ColorPicker::setReferenceColor(const QColor& color)
    {
        m_reference = color;
        syncFromColor(false);
    }

    bool ColorPicker::isRainbow() const
    {
        return m_rainbow;
    }

    void ColorPicker::setRainbow(bool rainbow)
    {
        m_rainbowSwitch->setChecked(rainbow);
    }

    int ColorPicker::rainbowLevel() const
    {
        return m_rainbowLevel;
    }

    void ColorPicker::setRainbowLevel(int level)
    {
        m_rainbowSpeed->setValue(qBound(1, level, 5));
    }

    QHash<QString, QLineEdit*> ColorPicker::notationEdits() const
    {
        return m_edits;
    }

    QLabel* ColorPicker::contrastLabel() const
    {
        return m_contrast;
    }

    Switch* ColorPicker::rainbowSwitch() const
    {
        return m_rainbowSwitch;
    }

    Slider* ColorPicker::rainbowSpeed() const
    {
        return m_rainbowSpeed;
    }

    QList<QColor> ColorPicker::recentColors() const
    {
        return m_recent->colors();
    }

    void ColorPicker::addRecentColor(const QColor& color)
    {
        m_recent->add(color);
    }

    void ColorPicker::syncFromColor(bool emitChange)
    {
        if (m_updating) {
            return;
        }
        m_updating = true;
        const double hue = m_color.hsvHueF() < 0 ? 0.0 : m_color.hsvHueF();
        m_field->setHue(hue);
        m_field->setPoint(m_color.hsvSaturationF(), m_color.valueF());
        m_hue->setHue(hue);
        m_alpha->setColor(m_color);
        m_swatch->setStyleSheet(QStringLiteral("background:%1;border:1px solid %2;border-radius:12px;")
                                    .arg(m_color.name(QColor::HexArgb), theme()->hex(Role::OutlineVariant)));
        m_swatch->setAccessibleDescription(ColorText::hex(m_color));
        const auto notations = ColorText::all(m_color);
        for (const auto& pair : notations) {
            if (QLineEdit* edit = m_edits.value(pair.first)) {
                if (!edit->hasFocus()) {
                    edit->setText(pair.second);
                }
            }
        }
        const QString named = ColorText::name(m_color);
        m_nameLabel->setText(named.isEmpty() ? tr("No CSS name for this colour") : tr("CSS name: %1").arg(named));
        QColor opaque = m_color;
        opaque.setAlpha(255);
        const double ratio = ColorText::contrastRatio(opaque, m_reference);
        const QString verdict = ratio >= 7.0 ? tr("AAA") : ratio >= 4.5 ? tr("AA") : ratio >= 3.0 ? tr("AA large text only") : tr("fails AA");
        m_contrast->setText(tr("Contrast %1:1 against %2 · %3").arg(QString::number(ratio, 'f', 2), ColorText::hex(m_reference), verdict));
        m_gamutLabel->setText(m_color.alpha() == 255 ? tr("sRGB · in gamut") : tr("sRGB · in gamut · %1% opaque").arg(qRound(m_color.alphaF() * 100)));
        m_updating = false;
        if (emitChange) {
            emit colorChanged(m_color);
        }
    }

    void ColorPicker::applyNotation(const QString& label)
    {
        QLineEdit* edit = m_edits.value(label);
        if (!edit) {
            return;
        }
        const QColor parsed = ColorText::parse(edit->text());
        if (!parsed.isValid()) {
            // Keep what the user typed in front of them and say it did not parse.
            edit->setAccessibleDescription(tr("Not a %1 colour").arg(label));
            edit->setProperty("invalid", true);
            edit->setStyleSheet(QStringLiteral("border: 2px solid %1;").arg(theme()->hex(Role::Error)));
            return;
        }
        edit->setAccessibleDescription(QString());
        edit->setProperty("invalid", false);
        edit->setStyleSheet(QString());
        // The same colour typed again is not a change, and focus stays put so
        // editingFinished cannot fire a second time from a focus-out.
        if (parsed == m_color) {
            return;
        }
        m_color = parsed;
        syncFromColor(true);
    }

    void ColorPicker::applyTheme()
    {
        const QFont mono = theme()->font(TypeRole::Mono);
        for (QLineEdit* edit : std::as_const(m_edits)) {
            edit->setFont(mono);
        }
        m_nameLabel->setFont(theme()->font(TypeRole::BodySmall));
        m_contrast->setFont(theme()->font(TypeRole::BodySmall));
        m_gamutLabel->setFont(theme()->font(TypeRole::LabelSmall));
        m_rainbowCaption->setFont(theme()->font(TypeRole::BodyMedium));
        syncFromColor(false);
    }

} // namespace Material
