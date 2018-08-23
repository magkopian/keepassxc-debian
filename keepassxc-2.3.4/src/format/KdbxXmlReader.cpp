/*
 * Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 or (at your option)
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "KdbxXmlReader.h"
#include "KeePass2RandomStream.h"
#include "core/Global.h"
#include "core/Tools.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/DatabaseIcons.h"
#include "core/Endian.h"
#include "streams/QtIOCompressor"

#include <QFile>
#include <QBuffer>

/**
 * @param version KDBX version
 */
KdbxXmlReader::KdbxXmlReader(quint32 version)
    : m_kdbxVersion(version)
{
}

/**
 * @param version KDBX version
 * @param binaryPool binary pool
 */
KdbxXmlReader::KdbxXmlReader(quint32 version, const QHash<QString, QByteArray>& binaryPool)
    : m_kdbxVersion(version)
    , m_binaryPool(binaryPool)
{
}

/**
 * Read XML contents from a file into a new database.
 *
 * @param device input file
 * @return pointer to the new database
 */
Database* KdbxXmlReader::readDatabase(const QString& filename)
{
    QFile file(filename);
    file.open(QIODevice::ReadOnly);
    return readDatabase(&file);
}

/**
 * Read XML stream from a device into a new database.
 *
 * @param device input device
 * @return pointer to the new database
 */
Database* KdbxXmlReader::readDatabase(QIODevice* device)
{
    auto db = new Database();
    readDatabase(device, db);
    return db;
}

/**
 * Read XML contents from a device into a given database using a \link KeePass2RandomStream.
 *
 * @param device input device
 * @param db database to read into
 * @param randomStream random stream to use for decryption
 */
#include "QDebug"
void KdbxXmlReader::readDatabase(QIODevice* device, Database* db, KeePass2RandomStream* randomStream)
{
    m_error = false;
    m_errorStr.clear();

    m_xml.clear();
    m_xml.setDevice(device);

    m_db = db;
    m_meta = m_db->metadata();
    m_meta->setUpdateDatetime(false);

    m_randomStream = randomStream;
    m_headerHash.clear();

    m_tmpParent.reset(new Group());

    bool rootGroupParsed = false;

    if (m_xml.hasError()) {
        raiseError(tr("XML parsing failure: %1").arg(m_xml.error()));
        return;
    }

    if (m_xml.readNextStartElement() && m_xml.name() == "KeePassFile") {
        rootGroupParsed = parseKeePassFile();
    }

    if (!rootGroupParsed) {
        raiseError(tr("No root group"));
        return;
    }

    if (!m_tmpParent->children().isEmpty()) {
        qWarning("KdbxXmlReader::readDatabase: found %d invalid group reference(s)",
                 m_tmpParent->children().size());
    }

    if (!m_tmpParent->entries().isEmpty()) {
        qWarning("KdbxXmlReader::readDatabase: found %d invalid entry reference(s)",
                 m_tmpParent->children().size());
    }

    const QSet<QString> poolKeys = m_binaryPool.keys().toSet();
    const QSet<QString> entryKeys = m_binaryMap.keys().toSet();
    const QSet<QString> unmappedKeys = entryKeys - poolKeys;
    const QSet<QString> unusedKeys = poolKeys - entryKeys;

    if (!unmappedKeys.isEmpty()) {
        qWarning("Unmapped keys left.");
    }

    for (const QString& key : unusedKeys) {
        qWarning("KdbxXmlReader::readDatabase: found unused key \"%s\"", qPrintable(key));
    }

    QHash<QString, QPair<Entry*, QString> >::const_iterator i;
    for (i = m_binaryMap.constBegin(); i != m_binaryMap.constEnd(); ++i) {
        const QPair<Entry*, QString>& target = i.value();
        target.first->attachments()->set(target.second, m_binaryPool[i.key()]);
    }

    m_meta->setUpdateDatetime(true);

    QHash<Uuid, Group*>::const_iterator iGroup;
    for (iGroup = m_groups.constBegin(); iGroup != m_groups.constEnd(); ++iGroup) {
        iGroup.value()->setUpdateTimeinfo(true);
    }

    QHash<Uuid, Entry*>::const_iterator iEntry;
    for (iEntry = m_entries.constBegin(); iEntry != m_entries.constEnd(); ++iEntry) {
        iEntry.value()->setUpdateTimeinfo(true);

        const QList<Entry*> historyItems = iEntry.value()->historyItems();
        for (Entry* histEntry : historyItems) {
            histEntry->setUpdateTimeinfo(true);
        }
    }
}

bool KdbxXmlReader::strictMode() const
{
    return m_strictMode;
}

void KdbxXmlReader::setStrictMode(bool strictMode)
{
    m_strictMode = strictMode;
}

bool KdbxXmlReader::hasError() const
{
    return m_error || m_xml.hasError();
}

QString KdbxXmlReader::errorString() const
{
    if (m_error) {
        return m_errorStr;
    }if (m_xml.hasError()) {
        return QString("XML error:\n%1\nLine %2, column %3")
            .arg(m_xml.errorString())
            .arg(m_xml.lineNumber())
            .arg(m_xml.columnNumber());
    }
    return QString();
}

bool KdbxXmlReader::isTrueValue(const QStringRef& value)
{
    return value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0
        || value == "1";
}

void KdbxXmlReader::raiseError(const QString& errorMessage)
{
    m_error = true;
    m_errorStr = errorMessage;
}

QByteArray KdbxXmlReader::headerHash() const
{
    return m_headerHash;
}

bool KdbxXmlReader::parseKeePassFile()
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "KeePassFile");

    bool rootElementFound = false;
    bool rootParsedSuccessfully = false;

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "Meta") {
            parseMeta();
            continue;
        }

        if (m_xml.name() == "Root") {
            if (rootElementFound) {
                rootParsedSuccessfully = false;
                qWarning("Multiple root elements");
            } else {
                rootParsedSuccessfully = parseRoot();
                rootElementFound = true;
            }
            continue;
        }

        skipCurrentElement();
    }

    return rootParsedSuccessfully;
}

void KdbxXmlReader::parseMeta()
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "Meta");

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "Generator") {
            m_meta->setGenerator(readString());
        } else if (m_xml.name() == "HeaderHash") {
            m_headerHash = readBinary();
        } else if (m_xml.name() == "DatabaseName") {
            m_meta->setName(readString());
        } else if (m_xml.name() == "DatabaseNameChanged") {
            m_meta->setNameChanged(readDateTime());
        } else if (m_xml.name() == "DatabaseDescription") {
            m_meta->setDescription(readString());
        } else if (m_xml.name() == "DatabaseDescriptionChanged") {
            m_meta->setDescriptionChanged(readDateTime());
        } else if (m_xml.name() == "DefaultUserName") {
            m_meta->setDefaultUserName(readString());
        } else if (m_xml.name() == "DefaultUserNameChanged") {
            m_meta->setDefaultUserNameChanged(readDateTime());
        } else if (m_xml.name() == "MaintenanceHistoryDays") {
            m_meta->setMaintenanceHistoryDays(readNumber());
        } else if (m_xml.name() == "Color") {
            m_meta->setColor(readColor());
        } else if (m_xml.name() == "MasterKeyChanged") {
            m_meta->setMasterKeyChanged(readDateTime());
        } else if (m_xml.name() == "MasterKeyChangeRec") {
            m_meta->setMasterKeyChangeRec(readNumber());
        } else if (m_xml.name() == "MasterKeyChangeForce") {
            m_meta->setMasterKeyChangeForce(readNumber());
        } else if (m_xml.name() == "MemoryProtection") {
            parseMemoryProtection();
        } else if (m_xml.name() == "CustomIcons") {
            parseCustomIcons();
        } else if (m_xml.name() == "RecycleBinEnabled") {
            m_meta->setRecycleBinEnabled(readBool());
        } else if (m_xml.name() == "RecycleBinUUID") {
            m_meta->setRecycleBin(getGroup(readUuid()));
        } else if (m_xml.name() == "RecycleBinChanged") {
            m_meta->setRecycleBinChanged(readDateTime());
        } else if (m_xml.name() == "EntryTemplatesGroup") {
            m_meta->setEntryTemplatesGroup(getGroup(readUuid()));
        } else if (m_xml.name() == "EntryTemplatesGroupChanged") {
            m_meta->setEntryTemplatesGroupChanged(readDateTime());
        } else if (m_xml.name() == "LastSelectedGroup") {
            m_meta->setLastSelectedGroup(getGroup(readUuid()));
        } else if (m_xml.name() == "LastTopVisibleGroup") {
            m_meta->setLastTopVisibleGroup(getGroup(readUuid()));
        } else if (m_xml.name() == "HistoryMaxItems") {
            int value = readNumber();
            if (value >= -1) {
                m_meta->setHistoryMaxItems(value);
            } else {
                qWarning("HistoryMaxItems invalid number");
            }
        } else if (m_xml.name() == "HistoryMaxSize") {
            int value = readNumber();
            if (value >= -1) {
                m_meta->setHistoryMaxSize(value);
            } else {
                qWarning("HistoryMaxSize invalid number");
            }
        } else if (m_xml.name() == "Binaries") {
            parseBinaries();
        } else if (m_xml.name() == "CustomData") {
            parseCustomData(m_meta->customData());
        } else if (m_xml.name() == "SettingsChanged") {
            m_meta->setSettingsChanged(readDateTime());
        } else {
            skipCurrentElement();
        }
    }
}

void KdbxXmlReader::parseMemoryProtection()
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "MemoryProtection");

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "ProtectTitle") {
            m_meta->setProtectTitle(readBool());
        } else if (m_xml.name() == "ProtectUserName") {
            m_meta->setProtectUsername(readBool());
        } else if (m_xml.name() == "ProtectPassword") {
            m_meta->setProtectPassword(readBool());
        } else if (m_xml.name() == "ProtectURL") {
            m_meta->setProtectUrl(readBool());
        } else if (m_xml.name() == "ProtectNotes") {
            m_meta->setProtectNotes(readBool());
        } else {
            skipCurrentElement();
        }
    }
}

void KdbxXmlReader::parseCustomIcons()
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "CustomIcons");

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "Icon") {
            parseIcon();
        } else {
            skipCurrentElement();
        }
    }
}

void KdbxXmlReader::parseIcon()
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "Icon");

    Uuid uuid;
    QImage icon;
    bool uuidSet = false;
    bool iconSet = false;

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "UUID") {
            uuid = readUuid();
            uuidSet = !uuid.isNull();
        } else if (m_xml.name() == "Data") {
            icon.loadFromData(readBinary());
            iconSet = true;
        } else {
            skipCurrentElement();
        }
    }

    if (uuidSet && iconSet) {
        m_meta->addCustomIcon(uuid, icon);
        return;
    }

    raiseError(tr("Missing icon uuid or data"));
}

void KdbxXmlReader::parseBinaries()
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "Binaries");

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() != "Binary") {
            skipCurrentElement();
            continue;
        }

        QXmlStreamAttributes attr = m_xml.attributes();
        QString id = attr.value("ID").toString();
        QByteArray data = isTrueValue(attr.value("Compressed"))
            ? readCompressedBinary() : readBinary();

        if (m_binaryPool.contains(id)) {
            qWarning("KdbxXmlReader::parseBinaries: overwriting binary item \"%s\"",
                     qPrintable(id));
        }

        m_binaryPool.insert(id, data);
    }
}

void KdbxXmlReader::parseCustomData(CustomData* customData)
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "CustomData");

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "Item") {
            parseCustomDataItem(customData);
            continue;
        }
        skipCurrentElement();
    }
}

void KdbxXmlReader::parseCustomDataItem(CustomData* customData)
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "Item");

    QString key;
    QString value;
    bool keySet = false;
    bool valueSet = false;

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "Key") {
            key = readString();
            keySet = true;
        } else if (m_xml.name() == "Value") {
            value = readString();
            valueSet = true;
        } else {
            skipCurrentElement();
        }
    }

    if (keySet && valueSet) {
        customData->set(key, value);
        return;
    }

    raiseError(tr("Missing custom data key or value"));
}

bool KdbxXmlReader::parseRoot()
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "Root");

    bool groupElementFound = false;
    bool groupParsedSuccessfully = false;

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "Group") {
            if (groupElementFound) {
                groupParsedSuccessfully = false;
                raiseError(tr("Multiple group elements"));
                continue;
            }

            Group* rootGroup = parseGroup();
            if (rootGroup) {
                Group* oldRoot = m_db->rootGroup();
                m_db->setRootGroup(rootGroup);
                delete oldRoot;
                groupParsedSuccessfully = true;
            }

            groupElementFound = true;
        } else if (m_xml.name() == "DeletedObjects") {
            parseDeletedObjects();
        } else {
            skipCurrentElement();
        }
    }

    return groupParsedSuccessfully;
}

Group* KdbxXmlReader::parseGroup()
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "Group");

    auto group = new Group();
    group->setUpdateTimeinfo(false);
    QList<Group*> children;
    QList<Entry*> entries;
    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "UUID") {
            Uuid uuid = readUuid();
            if (uuid.isNull()) {
                if (m_strictMode) {
                    raiseError(tr("Null group uuid"));
                } else {
                    group->setUuid(Uuid::random());
                }
            } else {
                group->setUuid(uuid);
            }
            continue;
        }
        if (m_xml.name() == "Name") {
            group->setName(readString());
            continue;
        }
        if (m_xml.name() == "Notes") {
            group->setNotes(readString());
            continue;
        }
        if (m_xml.name() == "IconID") {
            int iconId = readNumber();
            if (iconId < 0) {
                if (m_strictMode) {
                    raiseError(tr("Invalid group icon number"));
                }
                iconId = 0;
            } else if (iconId >= DatabaseIcons::IconCount) {
                qWarning("KdbxXmlReader::parseGroup: icon id \"%d\" not supported", iconId);
                iconId = DatabaseIcons::IconCount - 1;
            }

            group->setIcon(iconId);
            continue;
        }
        if (m_xml.name() == "CustomIconUUID") {
            Uuid uuid = readUuid();
            if (!uuid.isNull()) {
                group->setIcon(uuid);
            }
            continue;
        }
        if (m_xml.name() == "Times") {
            group->setTimeInfo(parseTimes());
            continue;
        }
        if (m_xml.name() == "IsExpanded") {
            group->setExpanded(readBool());
            continue;
        }
        if (m_xml.name() == "DefaultAutoTypeSequence") {
            group->setDefaultAutoTypeSequence(readString());
            continue;
        }
        if (m_xml.name() == "EnableAutoType") {
            QString str = readString();

            if (str.compare("null", Qt::CaseInsensitive) == 0) {
                group->setAutoTypeEnabled(Group::Inherit);
            } else if (str.compare("true", Qt::CaseInsensitive) == 0) {
                group->setAutoTypeEnabled(Group::Enable);
            } else if (str.compare("false", Qt::CaseInsensitive) == 0) {
                group->setAutoTypeEnabled(Group::Disable);
            } else {
                raiseError(tr("Invalid EnableAutoType value"));
            }
            continue;
        }
        if (m_xml.name() == "EnableSearching") {
            QString str = readString();

            if (str.compare("null", Qt::CaseInsensitive) == 0) {
                group->setSearchingEnabled(Group::Inherit);
            } else if (str.compare("true", Qt::CaseInsensitive) == 0) {
                group->setSearchingEnabled(Group::Enable);
            } else if (str.compare("false", Qt::CaseInsensitive) == 0) {
                group->setSearchingEnabled(Group::Disable);
            } else {
                raiseError(tr("Invalid EnableSearching value"));
            }
            continue;
        }
        if (m_xml.name() == "LastTopVisibleEntry") {
            group->setLastTopVisibleEntry(getEntry(readUuid()));
            continue;
        }
        if (m_xml.name() == "Group") {
            Group* newGroup = parseGroup();
            if (newGroup) {
                children.append(newGroup);
            }
            continue;
        }
        if (m_xml.name() == "Entry") {
            Entry* newEntry = parseEntry(false);
            if (newEntry) {
                entries.append(newEntry);
            }
            continue;
        }
        if (m_xml.name() == "CustomData") {
            parseCustomData(group->customData());
            continue;
        }

        skipCurrentElement();
    }

    if (group->uuid().isNull() && !m_strictMode) {
        group->setUuid(Uuid::random());
    }

    if (!group->uuid().isNull()) {
        Group* tmpGroup = group;
        group = getGroup(tmpGroup->uuid());
        group->copyDataFrom(tmpGroup);
        group->setUpdateTimeinfo(false);
        delete tmpGroup;
    } else if (!hasError()) {
        raiseError(tr("No group uuid found"));
    }

    for (Group* child : asConst(children)) {
        child->setParent(group);
    }

    for (Entry* entry : asConst(entries)) {
        entry->setGroup(group);
    }

    return group;
}

void KdbxXmlReader::parseDeletedObjects()
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "DeletedObjects");

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "DeletedObject") {
            parseDeletedObject();
        } else {
            skipCurrentElement();
        }
    }
}

void KdbxXmlReader::parseDeletedObject()
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "DeletedObject");

    DeletedObject delObj{{}, {}};

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "UUID") {
            Uuid uuid = readUuid();
            if (uuid.isNull()) {
                if (m_strictMode) {
                    raiseError(tr("Null DeleteObject uuid"));
                }
                continue;
            }
            delObj.uuid = uuid;
            continue;
        }
        if (m_xml.name() == "DeletionTime") {
            delObj.deletionTime = readDateTime();
            continue;
        }
        skipCurrentElement();
    }

    if (!delObj.uuid.isNull() && !delObj.deletionTime.isNull()) {
        m_db->addDeletedObject(delObj);
        return;
    }

    if (m_strictMode) {
        raiseError(tr("Missing DeletedObject uuid or time"));
    }
}

Entry* KdbxXmlReader::parseEntry(bool history)
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "Entry");

    auto entry = new Entry();
    entry->setUpdateTimeinfo(false);
    QList<Entry*> historyItems;
    QList<StringPair> binaryRefs;

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "UUID") {
            Uuid uuid = readUuid();
            if (uuid.isNull()) {
                if (m_strictMode) {
                    raiseError(tr("Null entry uuid"));
                } else {
                    entry->setUuid(Uuid::random());
                }
            } else {
                entry->setUuid(uuid);
            }
            continue;
        }
        if (m_xml.name() == "IconID") {
            int iconId = readNumber();
            if (iconId < 0) {
                if (m_strictMode) {
                    raiseError(tr("Invalid entry icon number"));
                }
                iconId = 0;
            }
            entry->setIcon(iconId);
            continue;
        }
        if (m_xml.name() == "CustomIconUUID") {
            Uuid uuid = readUuid();
            if (!uuid.isNull()) {
                entry->setIcon(uuid);
            }
            continue;
        }
        if (m_xml.name() == "ForegroundColor") {
            entry->setForegroundColor(readColor());
            continue;
        }
        if (m_xml.name() == "BackgroundColor") {
            entry->setBackgroundColor(readColor());
            continue;
        }
        if (m_xml.name() == "OverrideURL") {
            entry->setOverrideUrl(readString());
            continue;
        }
        if (m_xml.name() == "Tags") {
            entry->setTags(readString());
            continue;
        }
        if (m_xml.name() == "Times") {
            entry->setTimeInfo(parseTimes());
            continue;
        }
        if (m_xml.name() == "String") {
            parseEntryString(entry);
            continue;
        }
        if (m_xml.name() == "Binary") {
            QPair<QString, QString> ref = parseEntryBinary(entry);
            if (!ref.first.isNull() && !ref.second.isNull()) {
                binaryRefs.append(ref);
            }
            continue;
        }
        if (m_xml.name() == "AutoType") {
            parseAutoType(entry);
            continue;
        }
        if (m_xml.name() == "History") {
            if (history) {
                raiseError(tr("History element in history entry"));
            } else {
                historyItems = parseEntryHistory();
            }
            continue;
        }
        if (m_xml.name() == "CustomData") {
            parseCustomData(entry->customData());
            continue;
        }
        skipCurrentElement();
    }

    if (entry->uuid().isNull() && !m_strictMode) {
        entry->setUuid(Uuid::random());
    }

    if (!entry->uuid().isNull()) {
        if (history) {
            entry->setUpdateTimeinfo(false);
        } else {
            Entry* tmpEntry = entry;

            entry = getEntry(tmpEntry->uuid());
            entry->copyDataFrom(tmpEntry);
            entry->setUpdateTimeinfo(false);

            delete tmpEntry;
        }
    } else if (!hasError()) {
        raiseError(tr("No entry uuid found"));
    }

    for (Entry* historyItem : asConst(historyItems)) {
        if (historyItem->uuid() != entry->uuid()) {
            if (m_strictMode) {
                raiseError(tr("History element with different uuid"));
            } else {
                historyItem->setUuid(entry->uuid());
            }
        }
        entry->addHistoryItem(historyItem);
    }

    for (const StringPair& ref : asConst(binaryRefs)) {
        m_binaryMap.insertMulti(ref.first, qMakePair(entry, ref.second));
    }

    return entry;
}

void KdbxXmlReader::parseEntryString(Entry* entry)
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "String");

    QString key;
    QString value;
    bool protect = false;
    bool keySet = false;
    bool valueSet = false;

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "Key") {
            key = readString();
            keySet = true;
            continue;
        }

        if (m_xml.name() == "Value") {
            QXmlStreamAttributes attr = m_xml.attributes();
            bool isProtected;
            bool protectInMemory;
            value = readString(isProtected, protectInMemory);
            protect = isProtected || protectInMemory;
            valueSet = true;
            continue;
        }

        skipCurrentElement();
    }

    if (keySet && valueSet) {
        // the default attributes are always there so additionally check if it's empty
        if (entry->attributes()->hasKey(key) && !entry->attributes()->value(key).isEmpty()) {
            raiseError(tr("Duplicate custom attribute found"));
            return;
        }
        entry->attributes()->set(key, value, protect);
        return;
    }

    raiseError(tr("Entry string key or value missing"));
}

QPair<QString, QString> KdbxXmlReader::parseEntryBinary(Entry* entry)
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "Binary");

    QPair<QString, QString> poolRef;

    QString key;
    QByteArray value;
    bool keySet = false;
    bool valueSet = false;

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "Key") {
            key = readString();
            keySet = true;
            continue;
        }
        if (m_xml.name() == "Value") {
            QXmlStreamAttributes attr = m_xml.attributes();

            if (attr.hasAttribute("Ref")) {
                poolRef = qMakePair(attr.value("Ref").toString(), key);
                m_xml.skipCurrentElement();
            } else {
                // format compatibility
                value = readBinary();
            }

            valueSet = true;
            continue;
        }
        skipCurrentElement();
    }

    if (keySet && valueSet) {
        if (entry->attachments()->hasKey(key)) {
            raiseError(tr("Duplicate attachment found"));
        } else {
            entry->attachments()->set(key, value);
        }
    } else {
        raiseError(tr("Entry binary key or value missing"));
    }

    return poolRef;
}

void KdbxXmlReader::parseAutoType(Entry* entry)
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "AutoType");

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "Enabled") {
            entry->setAutoTypeEnabled(readBool());
        } else if (m_xml.name() == "DataTransferObfuscation") {
            entry->setAutoTypeObfuscation(readNumber());
        } else if (m_xml.name() == "DefaultSequence") {
            entry->setDefaultAutoTypeSequence(readString());
        } else if (m_xml.name() == "Association") {
            parseAutoTypeAssoc(entry);
        } else {
            skipCurrentElement();
        }
    }
}

void KdbxXmlReader::parseAutoTypeAssoc(Entry* entry)
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "Association");

    AutoTypeAssociations::Association assoc;
    bool windowSet = false;
    bool sequenceSet = false;

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "Window") {
            assoc.window = readString();
            windowSet = true;
        } else if (m_xml.name() == "KeystrokeSequence") {
            assoc.sequence = readString();
            sequenceSet = true;
        } else {
            skipCurrentElement();
        }
    }

    if (windowSet && sequenceSet) {
        entry->autoTypeAssociations()->add(assoc);
        return;
    }
    raiseError(tr("Auto-type association window or sequence missing"));
}

QList<Entry*> KdbxXmlReader::parseEntryHistory()
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "History");

    QList<Entry*> historyItems;

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "Entry") {
            historyItems.append(parseEntry(true));
        } else {
            skipCurrentElement();
        }
    }

    return historyItems;
}

TimeInfo KdbxXmlReader::parseTimes()
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "Times");

    TimeInfo timeInfo;
    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "LastModificationTime") {
            timeInfo.setLastModificationTime(readDateTime());
        } else if (m_xml.name() == "CreationTime") {
            timeInfo.setCreationTime(readDateTime());
        } else if (m_xml.name() == "LastAccessTime") {
            timeInfo.setLastAccessTime(readDateTime());
        } else if (m_xml.name() == "ExpiryTime") {
            timeInfo.setExpiryTime(readDateTime());
        } else if (m_xml.name() == "Expires") {
            timeInfo.setExpires(readBool());
        } else if (m_xml.name() == "UsageCount") {
            timeInfo.setUsageCount(readNumber());
        } else if (m_xml.name() == "LocationChanged") {
            timeInfo.setLocationChanged(readDateTime());
        } else {
            skipCurrentElement();
        }
    }

    return timeInfo;
}

QString KdbxXmlReader::readString()
{
    bool isProtected;
    bool protectInMemory;

    return readString(isProtected, protectInMemory);
}

QString KdbxXmlReader::readString(bool& isProtected, bool& protectInMemory)
{
    QXmlStreamAttributes attr = m_xml.attributes();
    isProtected = isTrueValue(attr.value("Protected"));
    protectInMemory = isTrueValue(attr.value("ProtectInMemory"));
    QString value = m_xml.readElementText();

    if (isProtected && !value.isEmpty()) {
        QByteArray ciphertext = QByteArray::fromBase64(value.toLatin1());
        bool ok;
        QByteArray plaintext = m_randomStream->process(ciphertext, &ok);
        if (!ok) {
            value.clear();
            raiseError(m_randomStream->errorString());
            return value;
        }

        value = QString::fromUtf8(plaintext);
    }

    return value;
}

bool KdbxXmlReader::readBool()
{
    QString str = readString();

    if (str.compare("true", Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (str.compare("false", Qt::CaseInsensitive) == 0) {
        return false;
    }
    if (str.length() == 0) {
        return false;
    }
    raiseError(tr("Invalid bool value"));
    return false;
}

QDateTime KdbxXmlReader::readDateTime()
{
    static QRegularExpression b64regex("^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$");
    QString str = readString();

    if (b64regex.match(str).hasMatch()) {
        QByteArray secsBytes = QByteArray::fromBase64(str.toUtf8()).leftJustified(8, '\0', true).left(8);
        qint64 secs = Endian::bytesToSizedInt<quint64>(secsBytes, KeePass2::BYTEORDER);
        return QDateTime(QDate(1, 1, 1), QTime(0, 0, 0, 0), Qt::UTC).addSecs(secs);
    }

    QDateTime dt = QDateTime::fromString(str, Qt::ISODate);
    if (dt.isValid()) {
        return dt;
    }

    if (m_strictMode) {
        raiseError(tr("Invalid date time value"));
    }

    return QDateTime::currentDateTimeUtc();
}

QColor KdbxXmlReader::readColor()
{
    QString colorStr = readString();

    if (colorStr.isEmpty()) {
        return {};
    }

    if (colorStr.length() != 7 || colorStr[0] != '#') {
        if (m_strictMode) {
            raiseError(tr("Invalid color value"));
        }
        return {};
    }

    QColor color;
    for (int i = 0; i <= 2; ++i) {
        QString rgbPartStr = colorStr.mid(1 + 2 * i, 2);
        bool ok;
        int rgbPart = rgbPartStr.toInt(&ok, 16);
        if (!ok || rgbPart > 255) {
            if (m_strictMode) {
                raiseError(tr("Invalid color rgb part"));
            }
            return {};
        }

        if (i == 0) {
            color.setRed(rgbPart);
        } else if (i == 1) {
            color.setGreen(rgbPart);
        } else {
            color.setBlue(rgbPart);
        }
    }

    return color;
}

int KdbxXmlReader::readNumber()
{
    bool ok;
    int result = readString().toInt(&ok);
    if (!ok) {
        raiseError(tr("Invalid number value"));
    }
    return result;
}

Uuid KdbxXmlReader::readUuid()
{
    QByteArray uuidBin = readBinary();
    if (uuidBin.isEmpty()) {
        return {};
    }
    if (uuidBin.length() != Uuid::Length) {
        if (m_strictMode) {
            raiseError(tr("Invalid uuid value"));
        }
        return {};
    }
    return Uuid(uuidBin);
}

QByteArray KdbxXmlReader::readBinary()
{
    QXmlStreamAttributes attr = m_xml.attributes();
    bool isProtected = isTrueValue(attr.value("Protected"));
    QString value = m_xml.readElementText();
    QByteArray data = QByteArray::fromBase64(value.toLatin1());

    if (isProtected && !data.isEmpty()) {
        bool ok;
        QByteArray plaintext = m_randomStream->process(data, &ok);
        if (!ok) {
            data.clear();
            raiseError(m_randomStream->errorString());
            return data;
        }

        data = plaintext;
    }

    return data;
}

QByteArray KdbxXmlReader::readCompressedBinary()
{
    QByteArray rawData = readBinary();

    QBuffer buffer(&rawData);
    buffer.open(QIODevice::ReadOnly);

    QtIOCompressor compressor(&buffer);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    compressor.open(QIODevice::ReadOnly);

    QByteArray result;
    if (!Tools::readAllFromDevice(&compressor, result)) {
        //: Translator meant is a binary data inside an entry
        raiseError(tr("Unable to decompress binary"));
    }
    return result;
}

Group* KdbxXmlReader::getGroup(const Uuid& uuid)
{
    if (uuid.isNull()) {
        return nullptr;
    }

    if (m_groups.contains(uuid)) {
        return m_groups.value(uuid);
    }

    auto group = new Group();
    group->setUpdateTimeinfo(false);
    group->setUuid(uuid);
    group->setParent(m_tmpParent.data());
    m_groups.insert(uuid, group);
    return group;
}

Entry* KdbxXmlReader::getEntry(const Uuid& uuid)
{
    if (uuid.isNull()) {
        return nullptr;
    }

    if (m_entries.contains(uuid)) {
        return m_entries.value(uuid);
    }

    auto entry = new Entry();
    entry->setUpdateTimeinfo(false);
    entry->setUuid(uuid);
    entry->setGroup(m_tmpParent.data());
    m_entries.insert(uuid, entry);
    return entry;
}

void KdbxXmlReader::skipCurrentElement()
{
    qWarning("KdbxXmlReader::skipCurrentElement: skip element \"%s\"", qPrintable(m_xml.name().toString()));
    m_xml.skipCurrentElement();
}

