/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#include "Kdbx4Writer.h"

#include <QBuffer>
#include <QFile>

#include "streams/HmacBlockStream.h"
#include "core/Database.h"
#include "core/Metadata.h"
#include "core/CustomData.h"
#include "crypto/CryptoHash.h"
#include "crypto/Random.h"
#include "format/KeePass2RandomStream.h"
#include "format/KdbxXmlWriter.h"
#include "streams/QtIOCompressor"
#include "streams/SymmetricCipherStream.h"

bool Kdbx4Writer::writeDatabase(QIODevice* device, Database* db)
{
    m_error = false;
    m_errorStr.clear();

    SymmetricCipher::Algorithm algo = SymmetricCipher::cipherToAlgorithm(db->cipher());
    if (algo == SymmetricCipher::InvalidAlgorithm) {
        raiseError(tr("Invalid symmetric cipher algorithm."));
        return false;
    }
    int ivSize = SymmetricCipher::algorithmIvSize(algo);
    if (ivSize < 0) {
        raiseError(tr("Invalid symmetric cipher IV size.", "IV = Initialization Vector for symmetric cipher"));
        return false;
    }

    QByteArray masterSeed = randomGen()->randomArray(32);
    QByteArray encryptionIV = randomGen()->randomArray(ivSize);
    QByteArray protectedStreamKey = randomGen()->randomArray(64);
    QByteArray startBytes;
    QByteArray endOfHeader = "\r\n\r\n";

    if (!db->setKey(db->key(), false, true)) {
        raiseError(tr("Unable to calculate master key"));
        return false;
    }

    // generate transformed master key
    CryptoHash hash(CryptoHash::Sha256);
    hash.addData(masterSeed);
    Q_ASSERT(!db->transformedMasterKey().isEmpty());
    hash.addData(db->transformedMasterKey());
    QByteArray finalKey = hash.result();

    // write header
    QByteArray headerData;
    {
        QBuffer header;
        header.open(QIODevice::WriteOnly);

        writeMagicNumbers(&header, KeePass2::SIGNATURE_1, KeePass2::SIGNATURE_2, KeePass2::FILE_VERSION_4);

        CHECK_RETURN_FALSE(writeHeaderField<quint32>(&header, KeePass2::HeaderFieldID::CipherID, db->cipher().toByteArray()));
        CHECK_RETURN_FALSE(writeHeaderField<quint32>(&header, KeePass2::HeaderFieldID::CompressionFlags,
                                                     Endian::sizedIntToBytes(static_cast<int>(db->compressionAlgo()),
                                                                             KeePass2::BYTEORDER)));
        CHECK_RETURN_FALSE(writeHeaderField<quint32>(&header, KeePass2::HeaderFieldID::MasterSeed, masterSeed));
        CHECK_RETURN_FALSE(writeHeaderField<quint32>(&header, KeePass2::HeaderFieldID::EncryptionIV, encryptionIV));

        // convert current Kdf to basic parameters
        QVariantMap kdfParams = KeePass2::kdfToParameters(db->kdf());
        QByteArray kdfParamBytes;
        if (!serializeVariantMap(kdfParams, kdfParamBytes)) {
            //: Translation comment: variant map = data structure for storing meta data
            raiseError(tr("Failed to serialize KDF parameters variant map"));
            return false;
        }

        CHECK_RETURN_FALSE(writeHeaderField<quint32>(&header, KeePass2::HeaderFieldID::KdfParameters, kdfParamBytes));
        QVariantMap publicCustomData = db->publicCustomData();
        if (!publicCustomData.isEmpty()) {
            QByteArray serialized;
            serializeVariantMap(publicCustomData, serialized);
            CHECK_RETURN_FALSE(writeHeaderField<quint32>(
                &header, KeePass2::HeaderFieldID::PublicCustomData, serialized));
        }

        CHECK_RETURN_FALSE(writeHeaderField<quint32>(&header, KeePass2::HeaderFieldID::EndOfHeader, endOfHeader));
        header.close();
        headerData = header.data();
    }
    CHECK_RETURN_FALSE(writeData(device, headerData));

    // hash header
    QByteArray headerHash = CryptoHash::hash(headerData, CryptoHash::Sha256);

    // write HMAC-authenticated cipher stream
    QByteArray hmacKey = KeePass2::hmacKey(masterSeed, db->transformedMasterKey());
    QByteArray headerHmac = CryptoHash::hmac(headerData, HmacBlockStream::getHmacKey(UINT64_MAX, hmacKey),
                                             CryptoHash::Sha256);
    CHECK_RETURN_FALSE(writeData(device, headerHash));
    CHECK_RETURN_FALSE(writeData(device, headerHmac));

    QScopedPointer<HmacBlockStream> hmacBlockStream;
    QScopedPointer<SymmetricCipherStream> cipherStream;

    hmacBlockStream.reset(new HmacBlockStream(device, hmacKey));
    if (!hmacBlockStream->open(QIODevice::WriteOnly)) {
        raiseError(hmacBlockStream->errorString());
        return false;
    }

    cipherStream.reset(new SymmetricCipherStream(hmacBlockStream.data(), algo,
                                                 SymmetricCipher::algorithmMode(algo),
                                                 SymmetricCipher::Encrypt));

    if (!cipherStream->init(finalKey, encryptionIV)) {
        raiseError(cipherStream->errorString());
        return false;
    }
    if (!cipherStream->open(QIODevice::WriteOnly)) {
        raiseError(cipherStream->errorString());
        return false;
    }

    QIODevice* outputDevice = nullptr;
    QScopedPointer<QtIOCompressor> ioCompressor;

    if (db->compressionAlgo() == Database::CompressionNone) {
        outputDevice = cipherStream.data();
    } else {
        ioCompressor.reset(new QtIOCompressor(cipherStream.data()));
        ioCompressor->setStreamFormat(QtIOCompressor::GzipFormat);
        if (!ioCompressor->open(QIODevice::WriteOnly)) {
            raiseError(ioCompressor->errorString());
            return false;
        }
        outputDevice = ioCompressor.data();
    }

    Q_ASSERT(outputDevice);

    CHECK_RETURN_FALSE(writeInnerHeaderField(outputDevice, KeePass2::InnerHeaderFieldID::InnerRandomStreamID,
                                             Endian::sizedIntToBytes(static_cast<int>(KeePass2::ProtectedStreamAlgo::ChaCha20),
                                                                     KeePass2::BYTEORDER)));
    CHECK_RETURN_FALSE(writeInnerHeaderField(outputDevice, KeePass2::InnerHeaderFieldID::InnerRandomStreamKey,
                                             protectedStreamKey));

    // Write attachments to the inner header
    writeAttachments(outputDevice, db);

    CHECK_RETURN_FALSE(writeInnerHeaderField(outputDevice, KeePass2::InnerHeaderFieldID::End, QByteArray()));

    KeePass2RandomStream randomStream(KeePass2::ProtectedStreamAlgo::ChaCha20);
    if (!randomStream.init(protectedStreamKey)) {
        raiseError(randomStream.errorString());
        return false;
    }

    KdbxXmlWriter xmlWriter(KeePass2::FILE_VERSION_4);
    xmlWriter.writeDatabase(outputDevice, db, &randomStream, headerHash);

    // Explicitly close/reset streams so they are flushed and we can detect
    // errors. QIODevice::close() resets errorString() etc.
    if (ioCompressor) {
        ioCompressor->close();
    }
    if (!cipherStream->reset()) {
        raiseError(cipherStream->errorString());
        return false;
    }
    if (!hmacBlockStream->reset()) {
        raiseError(hmacBlockStream->errorString());
        return false;
    }

    if (xmlWriter.hasError()) {
        raiseError(xmlWriter.errorString());
        return false;
    }

    return true;
}

/**
 * Write KDBX4 inner header field.
 *
 * @param device output device
 * @param fieldId field identifier
 * @param data header payload
 * @return true on success
 */
bool Kdbx4Writer::writeInnerHeaderField(QIODevice* device, KeePass2::InnerHeaderFieldID fieldId, const QByteArray& data)
{
    QByteArray fieldIdArr;
    fieldIdArr[0] = static_cast<char>(fieldId);
    CHECK_RETURN_FALSE(writeData(device, fieldIdArr));
    CHECK_RETURN_FALSE(writeData(device, Endian::sizedIntToBytes(static_cast<quint32>(data.size()), KeePass2::BYTEORDER)));
    CHECK_RETURN_FALSE(writeData(device, data));

    return true;
}

void Kdbx4Writer::writeAttachments(QIODevice* device, Database* db)
{
    const QList<Entry*> allEntries = db->rootGroup()->entriesRecursive(true);
    QSet<QByteArray> writtenAttachments;

    for (Entry* entry : allEntries) {
        const QList<QString> attachmentKeys = entry->attachments()->keys();
        for (const QString& key : attachmentKeys) {
            QByteArray data("\x01");
            data.append(entry->attachments()->value(key));

            if (writtenAttachments.contains(data)) {
                continue;
            }

            writeInnerHeaderField(device, KeePass2::InnerHeaderFieldID::Binary, data);
            writtenAttachments.insert(data);
        }
    }
}

/**
 * Serialize variant map to byte array.
 *
 * @param map input variant map
 * @param outputBytes output byte array
 * @return true on success
 */
bool Kdbx4Writer::serializeVariantMap(const QVariantMap& map, QByteArray& outputBytes)
{
    QBuffer buf(&outputBytes);
    buf.open(QIODevice::WriteOnly);
    CHECK_RETURN_FALSE(buf.write(Endian::sizedIntToBytes(KeePass2::VARIANTMAP_VERSION, KeePass2::BYTEORDER)) == 2);

    bool ok;
    QList<QString> keys = map.keys();
    for (const auto& k : keys) {
        KeePass2::VariantMapFieldType fieldType;
        QByteArray data;
        QVariant v = map.value(k);
        switch (static_cast<QMetaType::Type>(v.type())) {
        case QMetaType::Type::Int:
            fieldType = KeePass2::VariantMapFieldType::Int32;
            data = Endian::sizedIntToBytes(v.toInt(&ok), KeePass2::BYTEORDER);
            CHECK_RETURN_FALSE(ok);
            break;
        case QMetaType::Type::UInt:
            fieldType = KeePass2::VariantMapFieldType::UInt32;
            data = Endian::sizedIntToBytes(v.toUInt(&ok), KeePass2::BYTEORDER);
            CHECK_RETURN_FALSE(ok);
            break;
        case QMetaType::Type::LongLong:
            fieldType = KeePass2::VariantMapFieldType::Int64;
            data = Endian::sizedIntToBytes(v.toLongLong(&ok), KeePass2::BYTEORDER);
            CHECK_RETURN_FALSE(ok);
            break;
        case QMetaType::Type::ULongLong:
            fieldType = KeePass2::VariantMapFieldType::UInt64;
            data = Endian::sizedIntToBytes(v.toULongLong(&ok), KeePass2::BYTEORDER);
            CHECK_RETURN_FALSE(ok);
            break;
        case QMetaType::Type::QString:
            fieldType = KeePass2::VariantMapFieldType::String;
            data = v.toString().toUtf8();
            break;
        case QMetaType::Type::Bool:
            fieldType = KeePass2::VariantMapFieldType::Bool;
            data = QByteArray(1, static_cast<char>(v.toBool() ? '\1' : '\0'));
            break;
        case QMetaType::Type::QByteArray:
            fieldType = KeePass2::VariantMapFieldType::ByteArray;
            data = v.toByteArray();
            break;
        default:
            qWarning("Unknown object type %d in QVariantMap", v.type());
            return false;
        }
        QByteArray typeBytes;
        typeBytes[0] = static_cast<char>(fieldType);
        QByteArray nameBytes = k.toUtf8();
        QByteArray nameLenBytes = Endian::sizedIntToBytes(nameBytes.size(), KeePass2::BYTEORDER);
        QByteArray dataLenBytes = Endian::sizedIntToBytes(data.size(), KeePass2::BYTEORDER);

        CHECK_RETURN_FALSE(buf.write(typeBytes) == 1);
        CHECK_RETURN_FALSE(buf.write(nameLenBytes) == 4);
        CHECK_RETURN_FALSE(buf.write(nameBytes) == nameBytes.size());
        CHECK_RETURN_FALSE(buf.write(dataLenBytes) == 4);
        CHECK_RETURN_FALSE(buf.write(data) == data.size());
    }

    QByteArray endBytes;
    endBytes[0] = static_cast<char>(KeePass2::VariantMapFieldType::End);
    CHECK_RETURN_FALSE(buf.write(endBytes) == 1);
    return true;
}
