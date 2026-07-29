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

#include "MaterialDimSum.h"

#include "MaterialButtons.h"
#include "MaterialCard.h"
#include "MaterialElevation.h"
#include "MaterialVoice.h"

#include "core/Config.h"
#include "gui/KMessageWidget.h"

#include <QApplication>
#include <QEasingCurve>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QScreen>
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#ifdef Q_OS_WIN
#include <QLibrary>
#include <windows.h>
#endif

// Q_INIT_RESOURCE declares its helper in the enclosing namespace, so the
// bundled dishes have to be registered from file scope.
static void initDimSumResources()
{
    Q_INIT_RESOURCE(dimsum);
}

namespace Material
{
    namespace
    {
        /** One launch in a hundred, drawn from the system entropy source. */
        constexpr int OddsDenominator = 100;

        /** Room the card leaves itself for the el2 shadow. */
        constexpr int ShadowMargin = 24;
        constexpr int EdgeMargin = 20;
        constexpr int CardWidth = 340;
        constexpr int ArtSize = 56;
        constexpr int CloseSize = 24;

        /** Travel of the slide-in, to the right of the resting position. */
        constexpr int Slide = 28;

        /** How long the card stays put before it fades away. */
        constexpr int Hold = 6000;

        /**
         * The window has to finish painting and settle before anything decorative
         * lands on top of it. Nothing waits on this timer.
         */
        constexpr int StartupGrace = 1500;

        /** State that lasts exactly one launch. */
        bool s_shown = false;
        bool s_suppressed = false;
        bool s_pending = false;
        bool s_drawn = false;
        bool s_draw = false;

        /** The design's emphasised curve, cubic-bezier(.2, 0, 0, 1). */
        QEasingCurve emphasizedCurve()
        {
            QEasingCurve curve(QEasingCurve::BezierSpline);
            curve.addCubicBezierSegment(QPointF(0.2, 0.0), QPointF(0.0, 1.0), QPointF(1.0, 1.0));
            return curve;
        }

        /**
         * The line under the dish, one per playfulness level. Only the copy
         * around the dish is styled this way - the dish's own name is a fact and
         * is written out identically at every level, in both languages.
         */
        const char* const EnglishCaptions[Voice::MaxLevel] = {
            "A dim sum illustration, shown at random on one launch in a hundred.",
            "A dim sum illustration. Back to your database.",
            "A little dim sum, on the house.",
            "Yum cha o'clock. Your passwords will keep.",
            "The trolley came past! Take one before it rolls off. Your database is still locked.",
        };

        const char16_t* const CantoneseCaptions[Voice::MaxLevel] = {
            u"點心插圖，每一百次啟動隨機顯示一次。",
            u"點心插圖。返去你嘅資料庫啦。",
            u"請你食件點心。",
            u"得閒飲茶。密碼喺度等你。",
            u"點心車經過！快啲叫一籠。你個資料庫仲鎖住，唔使急。",
        };

        QString captionFor(Voice::Language language, int level)
        {
            const int index = qBound(Voice::MinLevel, level, Voice::MaxLevel) - 1;
            if (language == Voice::Language::Cantonese) {
                return QString::fromUtf16(CantoneseCaptions[index]);
            }
            return QString::fromUtf8(EnglishCaptions[index]);
        }

        /** The caption in the active language mode, at that language's funny level. */
        QString funnyCaption()
        {
            const Voice::Language language = Voice::language();
            if (language != Voice::Language::Bilingual) {
                return captionFor(language, Voice::funnyLevel(language));
            }
            // Bilingual carries both lines, each styled by its own slider.
            return captionFor(Voice::Language::English, Voice::funnyLevel(Voice::Language::English))
                   + QChar(Voice::BilingualSeparator)
                   + captionFor(Voice::Language::Cantonese, Voice::funnyLevel(Voice::Language::Cantonese));
        }

        /**
         * A desktop asking for less motion. Only Windows exposes the preference
         * to Qt; elsewhere the card still animates, and it is dismissible and
         * short-lived either way.
         */
        bool prefersReducedMotion()
        {
#ifdef Q_OS_WIN
            BOOL animate = TRUE;
            if (::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0)) {
                return !animate;
            }
#endif
            return false;
        }

        /**
         * Quiet hours, focus assist, presentation mode, a full screen application
         * - or a KeePassXC that was told to start out of sight. Anything but a
         * plainly idle desktop means the surprise stays home.
         */
        bool isQuiet()
        {
            if (config()->get(Config::GUI_MinimizeOnStartup).toBool()) {
                return true;
            }
#ifdef Q_OS_WIN
            // QUERY_USER_NOTIFICATION_STATE, resolved at run time so that a
            // decoration does not add a link dependency on the shell.
            constexpr int AcceptsNotifications = 5;
            using QueryStateFn = HRESULT(__stdcall*)(int*);
            static const auto queryState = reinterpret_cast<QueryStateFn>(
                QLibrary::resolve(QStringLiteral("shell32"), "SHQueryUserNotificationState"));
            int state = AcceptsNotifications;
            if (queryState && SUCCEEDED(queryState(&state))) {
                return state != AcceptsNotifications;
            }
#endif
            return false;
        }

        /**
         * A configuration that has never seen a database is a first run, and a
         * first run is the one launch where the user is being asked to trust us
         * with their secrets. A user who has turned the recent list off reads as
         * a first run every time and never gets the surprise; erring towards not
         * showing it is the right way round for a decoration.
         */
        bool isFirstRun()
        {
            if (!QFileInfo::exists(config()->getFileName())) {
                return true;
            }
            return config()->get(Config::LastDatabases).toStringList().isEmpty()
                   && config()->get(Config::LastActiveDatabase).toString().isEmpty();
        }

        /** Whether @p parent belongs to a window that is up, in front and idle. */
        bool isUsableHost(QWidget* parent)
        {
            if (!parent) {
                return false;
            }
            QWidget* window = parent->window();
            if (!window || !window->isVisible() || window->isMinimized()) {
                return false;
            }
            // A dialog on screen means an error, an update or a half finished
            // task, and any of those outranks a joke.
            if (!window->isActiveWindow() || QApplication::activeModalWidget()) {
                return false;
            }
            // So does a message bar: that is how KeePassXC reports a failure, a
            // new release and every other thing the user has to read.
            const auto messages = window->findChildren<KMessageWidget*>();
            return std::none_of(
                messages.cbegin(), messages.cend(), [](const KMessageWidget* bar) { return bar->isVisible(); });
        }

        /** Every suppression rule, with the draw itself left out. */
        bool canShow()
        {
            return !s_shown && !s_suppressed && config()->get(Config::GUI_DimSumSurprise).toBool() && !isFirstRun()
                   && !isQuiet() && !DimSum::catalogue().isEmpty();
        }

        QPixmap renderDish(const QString& asset, int size)
        {
            QSvgRenderer renderer(asset);
            if (!renderer.isValid()) {
                return {};
            }

            const auto* screen = QGuiApplication::primaryScreen();
            const qreal dpr = screen ? screen->devicePixelRatio() : 1.0;

            QPixmap pixmap(QSize(qRound(size * dpr), qRound(size * dpr)));
            pixmap.setDevicePixelRatio(dpr);
            pixmap.fill(Qt::transparent);

            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            renderer.render(&painter, QRectF(0, 0, size, size));
            painter.end();
            return pixmap;
        }
    } // namespace

    // -------------------------------------------------------------- DimSum::Dish

    bool DimSum::Dish::isValid() const
    {
        return !english.isEmpty() && !cantonese.isEmpty() && !asset.isEmpty();
    }

    QString DimSum::Dish::displayName() const
    {
        // Both names always; only their order follows the active language mode.
        if (Voice::language() == Voice::Language::Cantonese) {
            return QStringLiteral("%1 · %2").arg(cantonese, english);
        }
        return QStringLiteral("%1 · %2").arg(english, cantonese);
    }

    // -------------------------------------------------------------------- DimSum

    QVector<DimSum::Dish> DimSum::catalogue()
    {
        static const QVector<Dish> dishes = [] {
            initDimSumResources();

            QVector<Dish> parsed;
            QFile file(QStringLiteral(":/dimsum/dimsum.json"));
            if (!file.open(QIODevice::ReadOnly)) {
                return parsed;
            }

            const QJsonArray array = QJsonDocument::fromJson(file.readAll()).object().value("dishes").toArray();
            parsed.reserve(array.size());
            for (const QJsonValue& value : array) {
                const QJsonObject object = value.toObject();
                const Dish dish{object.value("english").toString(),
                                object.value("cantonese").toString(),
                                object.value("asset").toString()};
                if (dish.isValid()) {
                    parsed.append(dish);
                }
            }
            return parsed;
        }();
        return dishes;
    }

    bool DimSum::shouldShow()
    {
        // One decision per launch, remembered, so that asking twice can never make the surprise
        // more likely than the one percent it advertises.
        //
        // The whole decision latches, not just the random draw. canShow() asks the shell for the
        // user's notification state, which is an out-of-process call costing on the order of a
        // hundred milliseconds, and stats the config file. Running that ahead of the cache made
        // every caller pay for it: a test asking 20,000 times took 42 minutes. It is a startup
        // decision, so evaluating the environment once at the moment of the draw is also the
        // behaviour the one-per-launch rule actually describes.
        if (!s_drawn) {
            s_drawn = true;
            s_draw = canShow() && QRandomGenerator::system()->bounded(OddsDenominator) == 0;
        }
        return s_draw;
    }

    void DimSum::showIfDue(QWidget* parent)
    {
        if (s_pending || !shouldShow()) {
            return;
        }

        // Startup owns the main thread until the window is up. The card waits
        // for it, and re-checks everything when it wakes.
        s_pending = true;
        QPointer<QWidget> host(parent);
        QTimer::singleShot(StartupGrace, qApp, [host] {
            s_pending = false;
            showNow(host);
        });
    }

    bool DimSum::showNow(QWidget* parent)
    {
        if (!canShow() || !isUsableHost(parent)) {
            return false;
        }

        const QVector<Dish> dishes = catalogue();
        const Dish dish = dishes.at(QRandomGenerator::system()->bounded(dishes.size()));

        s_shown = true;
        auto* card = new DimSumCard(dish, parent->window());
        card->present();
        return true;
    }

    bool DimSum::hasShown()
    {
        return s_shown;
    }

    void DimSum::suppress()
    {
        s_suppressed = true;
    }

    // ---------------------------------------------------------------- DimSumCard

    DimSumCard::DimSumCard(const DimSum::Dish& dish, QWidget* parent)
        : QWidget(parent)
        , m_dish(dish)
        , m_host(parent ? parent->window() : nullptr)
        , m_reducedMotion(prefersReducedMotion())
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setFocusPolicy(Qt::NoFocus);
        hide();

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(ShadowMargin, ShadowMargin, ShadowMargin, ShadowMargin);

        m_card = new Card(Card::Variant::Filled, Shape::ExtraLarge, this);
        m_card->setFillRole(Role::SurfaceContainerHigh);
        m_card->setFixedWidth(CardWidth);
        root->addWidget(m_card);

        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(12);

        m_artLabel = new QLabel(m_card);
        m_artLabel->setFixedSize(ArtSize, ArtSize);
        m_artLabel->setPixmap(renderDish(m_dish.asset, ArtSize));
        m_artLabel->setStyleSheet(QStringLiteral("background: transparent;"));
        row->addWidget(m_artLabel, 0, Qt::AlignTop);

        auto* text = new QVBoxLayout;
        text->setContentsMargins(0, 0, 0, 0);
        text->setSpacing(4);

        m_nameLabel = new QLabel(m_dish.displayName(), m_card);
        m_nameLabel->setWordWrap(true);
        text->addWidget(m_nameLabel);

        m_captionLabel = new QLabel(funnyCaption(), m_card);
        m_captionLabel->setWordWrap(true);
        text->addWidget(m_captionLabel);
        row->addLayout(text, 1);

        auto* close = new IconButton(QStringLiteral("close"), m_card);
        close->setDiameter(CloseSize);
        close->setSymbolSize(16);
        close->setFocusPolicy(Qt::NoFocus);
        close->setToolTip(tr("Dismiss"));
        close->setAccessibleName(tr("Dismiss the dim sum card"));
        connect(close, &QAbstractButton::clicked, this, &DimSumCard::dismiss);
        row->addWidget(close, 0, Qt::AlignTop);

        m_card->contentLayout()->addLayout(row);

        // Screen readers get the dish, not "image".
        m_artLabel->setAccessibleName(m_dish.displayName());
        m_artLabel->setAccessibleDescription(tr("An illustration of %1.").arg(m_dish.displayName()));
        m_card->setAccessibleName(tr("Dim sum: %1").arg(m_dish.displayName()));
        m_card->setAccessibleDescription(m_captionLabel->text());

        m_animation = new QPropertyAnimation(this, "transition", this);
        connect(m_animation, &QPropertyAnimation::finished, this, [this] {
            if (m_dismissing) {
                deleteLater();
            } else {
                m_holdTimer->start();
            }
        });

        m_holdTimer = new QTimer(this);
        m_holdTimer->setSingleShot(true);
        m_holdTimer->setInterval(Hold);
        connect(m_holdTimer, &QTimer::timeout, this, &DimSumCard::dismiss);

        auto* fade = new QGraphicsOpacityEffect(this);
        fade->setOpacity(0.0);
        setGraphicsEffect(fade);

        if (m_host) {
            m_host->installEventFilter(this);
        }

        connect(theme(), &Theme::changed, this, &DimSumCard::applyTheme);
        applyTheme();
    }

    DimSumCard::~DimSumCard() = default;

    qreal DimSumCard::transition() const
    {
        return m_transition;
    }

    void DimSumCard::setTransition(qreal value)
    {
        value = qBound(0.0, value, 1.0);
        if (qFuzzyCompare(value + 1.0, m_transition + 1.0)) {
            return;
        }
        m_transition = value;
        if (auto* fade = qobject_cast<QGraphicsOpacityEffect*>(graphicsEffect())) {
            fade->setOpacity(m_transition);
        }
        reposition();
    }

    void DimSumCard::present()
    {
        reposition();
        show();
        raise();

        if (m_reducedMotion) {
            setTransition(1.0);
            m_holdTimer->start();
            return;
        }

        m_animation->stop();
        m_animation->setDuration(Duration::Long);
        m_animation->setEasingCurve(emphasizedCurve());
        m_animation->setStartValue(0.0);
        m_animation->setEndValue(1.0);
        m_animation->start();
        // The hold is armed when the slide finishes, so the card really does sit
        // still for its six seconds.
    }

    void DimSumCard::dismiss()
    {
        if (m_dismissing) {
            return;
        }
        m_dismissing = true;
        m_holdTimer->stop();
        m_animation->stop();

        if (m_reducedMotion) {
            hide();
            deleteLater();
            return;
        }

        m_animation->setDuration(Duration::Medium);
        m_animation->setEasingCurve(QEasingCurve::Linear);
        m_animation->setStartValue(m_transition);
        m_animation->setEndValue(0.0);
        m_animation->start();
    }

    QRect DimSumCard::cardRect() const
    {
        return rect().adjusted(ShadowMargin, ShadowMargin, -ShadowMargin, -ShadowMargin);
    }

    void DimSumCard::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event);

        // The card paints its own surface; only the shadow underneath is ours,
        // and it needs the margin this widget reserves around the card.
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        paintShadow(&painter, cardRect(), Shape::ExtraLarge, 2);
    }

    void DimSumCard::mousePressEvent(QMouseEvent* event)
    {
        if (cardRect().contains(event->position().toPoint())) {
            dismiss();
            event->accept();
            return;
        }
        // The shadow margin is not ours to keep; let the click carry on down.
        event->ignore();
    }

    bool DimSumCard::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == m_host && event->type() == QEvent::Resize) {
            reposition();
        }
        return QWidget::eventFilter(watched, event);
    }

    void DimSumCard::reposition()
    {
        if (!m_host) {
            return;
        }

        const int width = CardWidth + 2 * ShadowMargin;
        const int height = sizeHint().height();
        resize(width, height);

        const int restingX = m_host->width() - width - EdgeMargin + ShadowMargin;
        const int offset = m_reducedMotion ? 0 : qRound(Slide * (1.0 - m_transition));
        move(restingX + offset, m_host->height() - height - EdgeMargin + ShadowMargin);
    }

    void DimSumCard::applyTheme()
    {
        m_nameLabel->setFont(theme()->font(TypeRole::TitleSmall));
        m_nameLabel->setStyleSheet(
            QStringLiteral("background: transparent; color: %1;").arg(theme()->hex(Role::OnSurface)));
        m_captionLabel->setFont(theme()->font(TypeRole::BodySmall));
        m_captionLabel->setStyleSheet(
            QStringLiteral("background: transparent; color: %1;").arg(theme()->hex(Role::OnSurfaceVariant)));
        reposition();
        update();
    }

} // namespace Material
