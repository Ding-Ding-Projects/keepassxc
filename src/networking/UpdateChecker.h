/*
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSXC_UPDATECHECK_H
#define KEEPASSXC_UPDATECHECK_H
#include <QObject>
#include <QUrl>

class QNetworkReply;
class QSaveFile;
class QCryptographicHash;

class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    enum class State { Disabled, NotSquirrelInstalled, Idle, Checking, NoUpdate, Available, Downloading, Verifying, Applying, ReadyToRestart, Deferred, Restarting, Failed };
    Q_ENUM(State)
    enum class Failure { None, Offline, Timeout, RedirectRejected, OversizedManifest, MalformedManifest, PackageIdentityMismatch, ArchitectureMismatch, InvalidVersion, InsufficientStorage, Cancelled, ByteCountMismatch, Sha256Mismatch, ReleasesMismatch, UnsafePackage, UpdaterMissing, UpdaterStartFailed, UpdaterApplyFailed, AppliedVersionMissing, RestartRefused, RestartFailed };
    Q_ENUM(Failure)

    struct Candidate {
        QString version;
        QString notesUrl;
        QString packageUrl;
        QString packageFile;
        QString sha256;
        QString releasesSha1;
        quint64 bytes = 0;
    };

    UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker() override;

    void checkForUpdates(bool manuallyRequested);
    void downloadAvailableUpdate();
    void cancelDownload();
    static bool compareVersions(const QString& localVersion, const QString& remoteVersion);
    static UpdateChecker* instance();
    State state() const;
    Failure failure() const;
    Candidate candidate() const;
    static bool transitionAllowed(State from, State to);
    static bool parseManifest(const QByteArray& bytes, Candidate& candidate, Failure& failure);
    static bool verifyPackage(const QString& path, const Candidate& candidate, Failure& failure);

    static const QString ErrorVersion;

signals:
    void updateCheckFinished(bool hasNewVersion, QString version, bool isManuallyRequested);
    void stateChanged(UpdateChecker::State state, UpdateChecker::Failure failure);
    void downloadProgress(quint64 received, quint64 total);
    void updatePackageReady(QString path);

private slots:
    void fetchFinished();
    void fetchReadyRead();

private:
    QNetworkReply* m_reply;
    QNetworkReply* m_downloadReply = nullptr;
    QSaveFile* m_downloadFile = nullptr;
    QCryptographicHash* m_downloadHash = nullptr;
    quint64 m_downloadBytes = 0;
    quint64 m_generation = 0;
    QByteArray m_bytesReceived;
    bool m_isManuallyRequested;
    State m_state = State::Idle;
    Failure m_failure = Failure::None;
    Candidate m_candidate;

    void setState(State state, Failure failure = Failure::None);
    void finishDownload(quint64 generation);
    void failDownload(Failure failure);

    static UpdateChecker* m_instance;

    Q_DISABLE_COPY(UpdateChecker)
};

inline UpdateChecker* updateCheck()
{
    return UpdateChecker::instance();
}

#endif // KEEPASSXC_UPDATECHECK_H
