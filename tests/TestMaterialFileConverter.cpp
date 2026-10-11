#include "gui/material/MaterialFileConverter.h"
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

class TestMaterialFileConverter : public QObject
{
    Q_OBJECT
private slots:
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
QTEST_GUILESS_MAIN(TestMaterialFileConverter)
#include "TestMaterialFileConverter.moc"
