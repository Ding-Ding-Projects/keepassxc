#include "MaterialTabDescriptor.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>

namespace Material
{
    QString tabPersistenceKeyForPath(const QString& path)
    {
        if (path.trimmed().isEmpty()) {
            return {};
        }
        const QFileInfo info(path);
        QString normalized = info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
        normalized = QDir::fromNativeSeparators(QDir::cleanPath(normalized)).toCaseFolded();
        if (normalized.isEmpty()) {
            return {};
        }
        const QByteArray digest = QCryptographicHash::hash(normalized.toUtf8(), QCryptographicHash::Sha256).toHex();
        return QStringLiteral("file:%1").arg(QString::fromLatin1(digest));
    }
} // namespace Material
