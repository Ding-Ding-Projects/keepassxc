#include "gui/material/MaterialFileConverter.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialRegexBuilder.h"
#include "gui/material/MaterialVoice.h"
#include "gui/material/MaterialShell.h"
#include "gui/material/MaterialNavigationRail.h"
#include <QAction>
#include <QToolButton>
#include "core/Config.h"
#include <QAbstractButton>
#include <QApplication>
#include <QListWidget>
#include <QStandardPaths>
#include <QScopeGuard>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

class TestMaterialFileConverter : public QObject
{
    Q_OBJECT
private slots:
    void visibleSearchAndLiveDestinationCopy()
    {
        const auto language = Material::Voice::language();
        const auto restoreLanguage = qScopeGuard([language] { Material::Voice::setLanguage(language); });
        Material::Voice::setLanguage(Material::Voice::Language::English);
        Material::Shell shell;
        auto* converter = new Material::FileConverter(&shell);
        shell.addDestination(QStringLiteral("converter"), converter, QStringLiteral("transform"),
                             Material::Voice::say(QStringLiteral("converter.title")));
        shell.setDestinationCopyKey(QStringLiteral("converter"), QStringLiteral("converter.title"));
        shell.addDestination(QStringLiteral("other"), nullptr, QStringLiteral("help"), QStringLiteral("Other"));
        shell.resize(480, 640);
        shell.show();
        QCoreApplication::processEvents();
        const auto order = shell.destinations();
        auto* bar = converter->searchBar();
        QTRY_VERIFY(bar->isVisible());
        QVERIFY(converter->isSearchVisible());
        bar->setRegexEnabled(true);
        bar->setRegexFlags(QStringLiteral("i"));
        bar->setText(QStringLiteral("^binary"));
        auto* command = shell.findChild<QAction*>(QStringLiteral("materialDestination_converter"));
        auto* button = shell.findChild<QToolButton*>(QStringLiteral("materialBottomDestination_converter"));
        QVERIFY(command && button);
        for (auto mode : {Material::Voice::Language::English, Material::Voice::Language::Cantonese,
                          Material::Voice::Language::Bilingual, Material::Voice::Language::English}) {
            Material::Voice::setLanguage(mode);
            QCoreApplication::processEvents();
            const auto label = Material::Voice::say(QStringLiteral("converter.title"));
            QCOMPARE(command->text(), label);
            QCOMPARE(button->accessibleName(), label);
            QCOMPARE(shell.rail()->destinationLabel(QStringLiteral("converter")), label);
            QCOMPARE(shell.rail()->accessibleDescription(), label);
            QCOMPARE(shell.destinationFeedback(QStringLiteral("converter")), label);
            QCOMPARE(bar->searchLabel(), label);
            QCOMPARE(bar->accessibleName(), label);
            QCOMPARE(bar->placeholder(), label);
            QCOMPARE(bar->text(), QStringLiteral("^binary"));
            QCOMPARE(bar->regexFlags(), QStringLiteral("i"));
            QVERIFY(bar->isRegexEnabled());
            QVERIFY(bar->isVisible());
            QCOMPARE(shell.destinations(), order);
            QCOMPARE(shell.currentDestination(), QStringLiteral("converter"));
            QCOMPARE(shell.destination(QStringLiteral("converter")), static_cast<QWidget*>(converter));
            command->trigger();
            QCOMPARE(shell.currentDestination(), QStringLiteral("converter"));
        }
        QCOMPARE(shell.rail()->destinationLabel(QStringLiteral("other")), QStringLiteral("Other"));
        QCOMPARE(shell.destinationFeedback(QStringLiteral("other")), QStringLiteral("Already showing Other."));
    }
    void cancellationDuringVerification()
    {
        using Converter = Material::FileConverter;
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto source = directory.filePath(QStringLiteral("source.bin"));
        QFile file(source); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QByteArray(65537, 'x')); file.close();
        for (int cancelledPhase : {1, 2}) {
            std::atomic_bool cancel{false};
            int reached = 0;
            const auto destination = directory.filePath(QString::number(cancelledPhase));
            const auto result = Converter::convertWithObserver(source, destination, Converter::Operation::HexEncode, &cancel,
                [&](int phase) { reached = phase; if (phase == cancelledPhase) cancel.store(true); });
            QCOMPARE(reached, cancelledPhase);
            QCOMPARE(result, Converter::Result::Cancelled);
            QVERIFY(!QFileInfo::exists(destination));
        }
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray(65537, 'x'));
    }
    void builderFlagsRefreshCategory()
    {
        const auto language = Material::Voice::language();
        const auto restoreLanguage = qScopeGuard([language] { Material::Voice::setLanguage(language); });
        Material::Voice::setLanguage(Material::Voice::Language::English);
        Material::FileConverter converter;
        converter.show();
        auto* bar = converter.searchBar();
        auto* categories = converter.findChild<QListWidget*>();
        QVERIFY(categories);
        bar->setRegexEnabled(true);
        bar->setRegexFlags(QStringLiteral("i"));
        bar->setText(QStringLiteral("^binary"));
        QVERIFY(!categories->item(7)->isHidden());
        Material::RegexBuilder builder(&converter);
        builder.setPattern(bar->text());
        builder.setFlags(QString());
        connect(&builder, &Material::RegexBuilder::patternApplied, bar, [&builder, bar](const QString& pattern) {
            bar->setRegexFlags(builder.flags());
            bar->setRegexEnabled(true);
            bar->setText(pattern);
        });
        builder.openOverlay();
        QAbstractButton* apply = nullptr;
        for (auto* button : builder.findChildren<QAbstractButton*>())
            if (button->text() == QStringLiteral("Apply to search")) apply = button;
        QVERIFY(apply);
        QTRY_VERIFY(apply->isEnabled());
        apply->click();
        QCOMPARE(bar->text(), QStringLiteral("^binary"));
        QVERIFY(bar->regexFlags().isEmpty());
        QVERIFY(categories->item(7)->isHidden());
    }
    void vectors()
    {
        using Converter = Material::FileConverter;
        const QByteArray bytes = QByteArray::fromHex("000102ff") + QByteArray(65537, 'x');
        for (auto operation : {Converter::Operation::HexEncode, Converter::Operation::Base64Encode}) {
            QBuffer input; input.setData(bytes); QVERIFY(input.open(QIODevice::ReadOnly));
            QBuffer output; QVERIFY(output.open(QIODevice::ReadWrite));
            QCOMPARE(Converter::transform(input, output, operation), Converter::Result::Success);
            QCOMPARE(output.data(), operation == Converter::Operation::HexEncode ? bytes.toHex() : bytes.toBase64());
            QVERIFY(output.seek(0));
            QBuffer restored; QVERIFY(restored.open(QIODevice::ReadWrite));
            QCOMPARE(Converter::transform(output, restored, operation == Converter::Operation::HexEncode
                ? Converter::Operation::HexDecode : Converter::Operation::Base64Decode), Converter::Result::Success);
            QCOMPARE(restored.data(), bytes);
        }
    }
    void strictInvalidAndCancellation()
    {
        using Converter = Material::FileConverter;
        for (const QByteArray value : {QByteArray("0"), QByteArray("zz"), QByteArray("AA"), QByteArray("00\n")}) {
            QBuffer input; input.setData(value); QVERIFY(input.open(QIODevice::ReadOnly));
            QBuffer output; QVERIFY(output.open(QIODevice::WriteOnly));
            QCOMPARE(Converter::transform(input, output, Converter::Operation::HexDecode), Converter::Result::InvalidInput);
        }
        for (const QByteArray value : {QByteArray("Zh=="), QByteArray("Zg="), QByteArray("Zg==\n"), QByteArray("Zg==AAAA")}) {
            QBuffer input; input.setData(value); QVERIFY(input.open(QIODevice::ReadOnly));
            QBuffer output; QVERIFY(output.open(QIODevice::WriteOnly));
            QCOMPARE(Converter::transform(input, output, Converter::Operation::Base64Decode), Converter::Result::InvalidInput);
        }
        std::atomic_bool cancelled{true};
        QBuffer input, output; QVERIFY(input.open(QIODevice::ReadOnly)); QVERIFY(output.open(QIODevice::WriteOnly));
        QCOMPARE(Converter::transform(input, output, Converter::Operation::HexEncode, &cancelled), Converter::Result::Cancelled);
    }
    void atomicDestinationAndSource()
    {
        using Converter = Material::FileConverter;
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto source = directory.filePath(QStringLiteral("source.bin"));
        const auto destination = directory.filePath(QStringLiteral("result.txt"));
        QFile file(source); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("abc"), qint64(3)); file.close();
#ifdef Q_OS_WIN
        QCOMPARE(Converter::convert(source, destination, Converter::Operation::HexEncode), Converter::Result::Success);
        QFile result(destination); QVERIFY(result.open(QIODevice::ReadOnly)); QCOMPARE(result.readAll(), QByteArray("616263")); result.close();
        QVERIFY(QDir(directory.path()).entryList(QStringList{QStringLiteral(".conversion-*")}, QDir::Files | QDir::Hidden).isEmpty());
        const auto racedDestination = directory.filePath(QStringLiteral("created-during-verification.txt"));
        const auto racedResult = Converter::convertWithObserver(source, racedDestination, Converter::Operation::HexEncode, nullptr,
            [&](int phase) {
                if (phase != 2) return;
                QFile occupied(racedDestination);
                QVERIFY(occupied.open(QIODevice::WriteOnly | QIODevice::NewOnly));
                QCOMPARE(occupied.write("existing"), qint64(8));
            });
        QCOMPARE(racedResult, Converter::Result::IoFailure);
        QFile occupied(racedDestination); QVERIFY(occupied.open(QIODevice::ReadOnly));
        QCOMPARE(occupied.readAll(), QByteArray("existing")); occupied.close();
        QVERIFY(QDir(directory.path()).entryList(QStringList{QStringLiteral(".conversion-*")}, QDir::Files | QDir::Hidden).isEmpty());
        QCOMPARE(Converter::convert(source, destination, Converter::Operation::Base64Encode), Converter::Result::UnsafePath);
        QVERIFY(result.open(QIODevice::ReadOnly)); QCOMPARE(result.readAll(), QByteArray("616263"));
#endif
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("abc")); file.close();
        QCOMPARE(Converter::convert(source, source, Converter::Operation::HexEncode), Converter::Result::UnsafePath);
        QFile invalid(directory.filePath(QStringLiteral("invalid.hex"))); QVERIFY(invalid.open(QIODevice::WriteOnly)); invalid.write("0g"); invalid.close();
        const auto rejected = directory.filePath(QStringLiteral("absent.bin"));
        QCOMPARE(Converter::convert(invalid.fileName(), rejected, Converter::Operation::HexDecode), Converter::Result::InvalidInput);
        QVERIFY(!QFileInfo::exists(rejected));
        std::atomic_bool cancel{true};
        QCOMPARE(Converter::convert(source, rejected, Converter::Operation::HexEncode, &cancel), Converter::Result::Cancelled);
        QVERIFY(!QFileInfo::exists(rejected));
    }
};
int main(int argc, char** argv)
{
    QStandardPaths::setTestModeEnabled(true);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QTemporaryDir profile;
    if (!profile.isValid()) return 1;
    QApplication application(argc, argv);
    Config::createConfigFromFile(profile.filePath(QStringLiteral("roaming.ini")), profile.filePath(QStringLiteral("local.ini")));
    TestMaterialFileConverter tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "TestMaterialFileConverter.moc"
