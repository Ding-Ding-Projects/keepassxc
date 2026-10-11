/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef KEEPASSXC_MATERIALMOTIONAUTHORING_H
#define KEEPASSXC_MATERIALMOTIONAUTHORING_H
#include <QJsonObject>
#include <QStringList>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QWidget>
class QTableWidget;
class QLabel;
class QSpinBox;
namespace Material {
QString motionCopy(const QString& english);
class MotionDocument {
public:
    static QStringList properties();
    static bool validate(const QJsonObject& document, QString* error = nullptr);
    static QJsonObject sample(const QJsonObject& document, double milliseconds, const QString& state, bool reduced);
    static int duration(const QJsonObject& document);
    static QJsonObject empty();
};
class MotionPlayer : public QObject {
    Q_OBJECT
public:
    explicit MotionPlayer(QObject* parent = nullptr);
    ~MotionPlayer() override;
    bool setDocument(const QJsonObject& document, const QString& state = QStringLiteral("normal"));
    void seek(int milliseconds);
    void play(bool reverse = false);
    void pause();
    void stop();
    void setReducedMotion(bool reduced);
    int position() const { return m_position; }
    bool running() const { return m_timer.isActive(); }
    QJsonObject document() const { return m_document; }
signals:
    void frame(const QJsonObject& values);
    void positionChanged(int milliseconds);
private:
    void render();
    QJsonObject m_document;
    QString m_state = QStringLiteral("normal");
    QTimer m_timer;
    QElapsedTimer m_clock;
    int m_position = 0;
    int m_origin = 0;
    int m_direction = 1;
    bool m_reduced = false;
};
class MotionEditor : public QWidget {
    Q_OBJECT
public:
    explicit MotionEditor(QWidget* parent = nullptr);
    void setTarget(const QString& key);
    void stopPreview();
protected:
    void hideEvent(QHideEvent* event) override;
private:
    void setStatus(const QString& source);
    void refresh();
    void showFrames();
    void saveTables();
    void replaceDocument(const QJsonObject& document, bool history = true);
    QString m_key;
    QJsonObject m_document;
    QList<QJsonObject> m_undo, m_redo;
    QTableWidget* m_tracks;
    QTableWidget* m_frames;
    QLabel* m_status;
    QLabel* m_preview;
    QSpinBox* m_seek;
    MotionPlayer* m_player;
    bool m_loading = false;
};
}
#endif
