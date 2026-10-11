#include "MaterialFileConverter.h"
#include "MaterialButtons.h"
#include "MaterialRegexSafety.h"
#include "MaterialSearchBar.h"
#include "MaterialVoice.h"
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QStorageInfo>
#include <QTemporaryFile>
#include <QThread>
#include <QVBoxLayout>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace Material {
namespace {
QString say(const char* key) { return Voice::say(QString::fromLatin1(key)); }
const char* categories[] = {"converter.documents", "converter.images", "converter.audio", "converter.video",
                            "converter.archives", "converter.data", "converter.code", "converter.binary"};
bool safePath(const QString& path)
{
    QFileInfo info(path);
    if (!info.isAbsolute() || path.contains(QChar::Null)) return false;
    for (QFileInfo cursor(info); !cursor.filePath().isEmpty();) {
        if (cursor.isSymLink()) return false;
#ifdef Q_OS_WIN
        const auto native = QDir::toNativeSeparators(cursor.absoluteFilePath());
        const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()));
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
#endif
        const QString parent = cursor.absolutePath();
        if (parent == cursor.absoluteFilePath()) break;
        cursor.setFile(parent);
    }
#ifdef Q_OS_WIN
    if (path.startsWith(QStringLiteral("\\\\")) || path.mid(2).contains(QLatin1Char(':'))) return false;
#endif
    return true;
}
class HashSink final : public QIODevice
{
public:
    HashSink() { open(QIODevice::WriteOnly); }
    QByteArray digest() const { return hash.result(); }
protected:
    qint64 readData(char*, qint64) override { return -1; }
    qint64 writeData(const char* bytes, qint64 size) override { hash.addData(QByteArrayView(bytes, size)); return size; }
private:
    QCryptographicHash hash{QCryptographicHash::Sha256};
};
FileConverter::Operation inverse(FileConverter::Operation op)
{
    using Op = FileConverter::Operation;
    switch (op) {
    case Op::HexEncode: return Op::HexDecode;
    case Op::HexDecode: return Op::HexEncode;
    case Op::Base64Encode: return Op::Base64Decode;
    case Op::Base64Decode: return Op::Base64Encode;
    }
    return Op::HexEncode;
}
}

FileConverter::Result FileConverter::transform(QIODevice& input, QIODevice& output, Operation op, std::atomic_bool* cancel)
{
    const bool decode = op == Operation::HexDecode || op == Operation::Base64Decode;
    const bool hex = op == Operation::HexEncode || op == Operation::HexDecode;
    const int unit = hex ? (decode ? 2 : 1) : (decode ? 4 : 3);
    QByteArray pending;
    qint64 total = 0;
    qint64 written = 0;
    bool padded = false;
    while (true) {
        if (cancel && cancel->load()) return Result::Cancelled;
        const QByteArray chunk = input.read(65536);
        if (chunk.isEmpty() && !input.atEnd()) return Result::IoFailure;
        total += chunk.size();
        if (total > InputLimit * (decode ? 2 : 1)) return Result::LimitExceeded;
        pending.append(chunk);
        const bool final = input.atEnd();
        const qsizetype count = final ? pending.size() : pending.size() / unit * unit;
        QByteArray block = pending.left(count);
        pending.remove(0, count);
        QByteArray result;
        if (!decode) result = hex ? block.toHex() : block.toBase64();
        else if (hex) {
            if (block.size() % 2) return Result::InvalidInput;
            for (char c : block) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return Result::InvalidInput;
            result = QByteArray::fromHex(block);
        } else {
            if (block.size() % 4 || (padded && !block.isEmpty())) return Result::InvalidInput;
            const auto decoded = QByteArray::fromBase64Encoding(block, QByteArray::AbortOnBase64DecodingErrors);
            if (!decoded || decoded.decoded.toBase64() != block) return Result::InvalidInput;
            padded = block.contains('=');
            result = decoded.decoded;
        }
        written += result.size();
        if (written > InputLimit * (decode ? 1 : 2)) return Result::LimitExceeded;
        if (!result.isEmpty() && output.write(result) != result.size()) return Result::IoFailure;
        if (final) return Result::Success;
    }
}

FileConverter::Result FileConverter::convert(const QString& source, const QString& destination, Operation op, std::atomic_bool* cancel)
{
    if (!safePath(source) || !safePath(destination) || QFileInfo::exists(destination)
        || QFileInfo(source).absoluteFilePath() == QFileInfo(destination).absoluteFilePath()) return Result::UnsafePath;
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly) || !QFileInfo(input).isFile()) return Result::IoFailure;
    const QByteArray signature = input.peek(8);
    if (signature.startsWith(QByteArray::fromHex("03d9a29a"))) return Result::InvalidInput;
    const bool decode = op == Operation::HexDecode || op == Operation::Base64Decode;
    if (input.size() > InputLimit * (decode ? 2 : 1)) return Result::LimitExceeded;
    QStorageInfo storage(QFileInfo(destination).absolutePath());
    if (!storage.isValid() || storage.isReadOnly() || storage.bytesAvailable() < input.size() * 2 + 65536) return Result::IoFailure;
    QCryptographicHash sourceHash(QCryptographicHash::Sha256);
    while (!input.atEnd()) {
        if (cancel && cancel->load()) return Result::Cancelled;
        const auto bytes = input.read(65536);
        if (bytes.isEmpty() && !input.atEnd()) return Result::IoFailure;
        sourceHash.addData(bytes);
        if (input.pos() > InputLimit * (decode ? 2 : 1)) return Result::LimitExceeded;
    }
    if (input.error() != QFileDevice::NoError || !input.seek(0)) return Result::IoFailure;
    QTemporaryFile temporary(QFileInfo(destination).absolutePath() + QStringLiteral("/.conversion-XXXXXX"));
    if (!temporary.open()) return Result::IoFailure;
    auto result = transform(input, temporary, op, cancel);
    if (input.error() != QFileDevice::NoError) return Result::IoFailure;
    if (result != Result::Success || !temporary.flush()) return result == Result::Success ? Result::IoFailure : result;
    if (!temporary.seek(0)) return Result::IoFailure;
    HashSink restored;
    result = transform(temporary, restored, inverse(op), cancel);
    if (result != Result::Success || restored.digest() != sourceHash.result()) return Result::InvalidInput;
    // Reopen the staged file, then validate independently of the writer handle.
    const QString staged = temporary.fileName();
    temporary.close();
    QFile reopened(staged);
    if (!reopened.open(QIODevice::ReadOnly)) return Result::IoFailure;
    HashSink reopenedHash;
    result = transform(reopened, reopenedHash, inverse(op), cancel);
    reopened.close();
    if (result != Result::Success || reopenedHash.digest() != sourceHash.result()) return Result::InvalidInput;
    if (cancel && cancel->load()) return Result::Cancelled;
#ifdef Q_OS_WIN
    const QString from = QDir::toNativeSeparators(staged), to = QDir::toNativeSeparators(destination);
    if (!MoveFileExW(reinterpret_cast<LPCWSTR>(from.utf16()), reinterpret_cast<LPCWSTR>(to.utf16()), MOVEFILE_WRITE_THROUGH)) return Result::IoFailure;
    return Result::Success;
#else
    return Result::UnsupportedPlatform;
#endif
}

FileConverter::FileConverter(QWidget* parent) : Screen(parent)
{
    searchBar()->setIdentity(QStringLiteral("converter.catalog"), say("converter.title"));
    m_categories = new QListWidget(this);
    m_categories->setWordWrap(true);
    for (int i = 0; i < 8; ++i) {
        auto* item = new QListWidgetItem(m_categories);
        item->setData(Qt::UserRole, i);
        if (i != 7) item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
    contentLayout()->addWidget(m_categories);
    m_source = new QLineEdit(this); m_source->setReadOnly(true);
    m_destination = new QLineEdit(this); m_destination->setReadOnly(true);
    m_operation = new QComboBox(this);
    for (int i = 0; i < 4; ++i) m_operation->addItem(QString(), i);
    auto* sourceButton = new OutlinedButton(QStringLiteral("folder_open"), QString(), this);
    auto* destinationButton = new OutlinedButton(QStringLiteral("save"), QString(), this);
    sourceButton->setObjectName(QStringLiteral("converterSourcePicker"));
    destinationButton->setObjectName(QStringLiteral("converterDestinationPicker"));
    m_run = new FilledButton(QStringLiteral("transform"), QString(), this);
    auto* cancelButton = new OutlinedButton(QStringLiteral("cancel"), QString(), this);
    cancelButton->setObjectName(QStringLiteral("converterCancel"));
    m_status = new QLabel(this); m_status->setWordWrap(true); m_status->setTextFormat(Qt::PlainText);
    for (QWidget* widget : QList<QWidget*>{sourceButton, m_source, m_operation, destinationButton, m_destination, m_run, cancelButton, m_status}) contentLayout()->addWidget(widget);
    m_statusKey = QStringLiteral("converter.empty");
    connect(sourceButton, &QAbstractButton::clicked, this, [this] {
        const auto file = QFileDialog::getOpenFileName(this, say("converter.source"));
        if (file.isEmpty()) return;
        m_source->setText(file);
        QFile probe(file);
        if (!safePath(file) || !probe.open(QIODevice::ReadOnly)) {
            m_statusKey = QStringLiteral("converter.unsafe");
        } else {
            const auto prefix = probe.read(4096);
            bool hex = !prefix.isEmpty(), base64 = !prefix.isEmpty();
            for (char c : prefix) {
                hex = hex && ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
                base64 = base64 && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                    || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=');
            }
            m_statusKey = prefix.startsWith(QByteArray::fromHex("03d9a29a")) ? QStringLiteral("converter.invalid")
                : hex ? QStringLiteral("converter.detect-hex") : base64 ? QStringLiteral("converter.detect-base64")
                : QStringLiteral("converter.detect-binary");
        }
        refreshCopy();
    });
    connect(destinationButton, &QAbstractButton::clicked, this, [this] { const auto file = QFileDialog::getSaveFileName(this, say("converter.destination")); if (!file.isEmpty()) m_destination->setText(file); });
    connect(m_run, &QAbstractButton::clicked, this, &FileConverter::start);
    connect(cancelButton, &QAbstractButton::clicked, this, [this] { m_cancel.store(true); });
    connect(searchBar(), &SearchBar::textChanged, this, &FileConverter::filterCategories);
    connect(searchBar(), &SearchBar::regexToggled, this, &FileConverter::filterCategories);
    connect(Voice::notifier(), &Voice::Notifier::changed, this, &FileConverter::refreshCopy);
    refreshCopy();
}
FileConverter::~FileConverter() { m_cancel.store(true); if (m_worker) { m_worker->wait(); delete m_worker; } }
void FileConverter::refreshCopy()
{
    setHeadline(say("converter.title")); setSupportingText(say("converter.boundary"));
    for (int i = 0; i < 8; ++i) m_categories->item(i)->setText(say(categories[i]) + (i == 7 ? QString() : QStringLiteral("\n") + say("converter.unavailable")));
    const char* operations[] = {"converter.hex-encode", "converter.hex-decode", "converter.base64-encode", "converter.base64-decode"};
    for (int i = 0; i < 4; ++i) m_operation->setItemText(i, say(operations[i]));
    m_operation->setAccessibleName(say("converter.operation"));
    m_categories->setAccessibleName(say("converter.title"));
    m_source->setAccessibleName(say("converter.source")); m_destination->setAccessibleName(say("converter.destination"));
    findChild<OutlinedButton*>(QStringLiteral("converterSourcePicker"))->setText(say("converter.source"));
    findChild<OutlinedButton*>(QStringLiteral("converterDestinationPicker"))->setText(say("converter.destination"));
    findChild<OutlinedButton*>(QStringLiteral("converterCancel"))->setText(say("converter.cancel"));
    m_run->setText(say("converter.run")); m_status->setText(Voice::say(m_statusKey));
    filterCategories();
}
void FileConverter::filterCategories()
{
    bool matches[8];
    for (int i = 0; i < 8; ++i) {
        const auto text = m_categories->item(i)->text();
        bool match = text.contains(searchBar()->text(), Qt::CaseInsensitive);
        if (searchBar()->isRegexEnabled()) {
            const auto run = runBounded(searchBar()->text(), optionsForFlags(searchBar()->regexFlags()), text);
            if (!run.compiled || run.blocked || run.timedOut) return;
            match = !run.matches.isEmpty();
        }
        matches[i] = match;
    }
    for (int i = 0; i < 8; ++i) m_categories->item(i)->setHidden(!matches[i]);
}
void FileConverter::start()
{
    if (m_worker) return;
    if (m_source->text().isEmpty() || m_destination->text().isEmpty()) { m_statusKey = QStringLiteral("converter.empty"); refreshCopy(); return; }
    const QString source = m_source->text(), destination = m_destination->text();
    const auto operation = static_cast<Operation>(m_operation->currentData().toInt());
    m_cancel.store(false); m_run->setEnabled(false);
    m_statusKey = QStringLiteral("converter.running"); refreshCopy();
    m_worker = QThread::create([this, source, destination, operation] {
        const auto result = convert(source, destination, operation, &m_cancel);
        QMetaObject::invokeMethod(this, [this, result] {
            m_worker->wait(); delete m_worker; m_worker = nullptr;
            m_run->setEnabled(true);
            const char* keys[] = {"converter.success", "converter.invalid", "converter.io", "converter.unsafe",
                                  "converter.limit", "converter.cancelled", "converter.platform"};
            m_statusKey = QString::fromLatin1(keys[static_cast<int>(result)]);
            refreshCopy();
        }, Qt::QueuedConnection);
    });
    m_worker->start();
}
}
