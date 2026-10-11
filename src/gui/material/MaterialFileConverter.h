#ifndef KEEPASSXC_MATERIALFILECONVERTER_H
#define KEEPASSXC_MATERIALFILECONVERTER_H

#include "MaterialScreen.h"
#include <atomic>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QThread;
class QIODevice;
namespace Material {
class FilledButton;
class FileConverter final : public Screen
{
    Q_OBJECT
public:
    enum class Operation { HexEncode, HexDecode, Base64Encode, Base64Decode };
    enum class Result { Success, InvalidInput, IoFailure, UnsafePath, LimitExceeded, Cancelled, UnsupportedPlatform };
    explicit FileConverter(QWidget* parent = nullptr);
    ~FileConverter() override;
    static constexpr qint64 InputLimit = 64 * 1024 * 1024;
    static Result transform(QIODevice& input, QIODevice& output, Operation operation, std::atomic_bool* cancel = nullptr);
    static Result convert(const QString& source, const QString& destination, Operation operation, std::atomic_bool* cancel = nullptr);
private:
    void refreshCopy();
    void filterCategories();
    void start();
    QComboBox* m_operation;
    QLineEdit* m_source;
    QLineEdit* m_destination;
    QListWidget* m_categories;
    QLabel* m_status;
    FilledButton* m_run;
    QThread* m_worker = nullptr;
    std::atomic_bool m_cancel{false};
    QString m_statusKey;
};
}
#endif
