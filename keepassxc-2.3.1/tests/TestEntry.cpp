/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2013 Felix Geyer <debfx@fobos.de>
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

#include <QScopedPointer>

#include "TestEntry.h"
#include "TestGlobal.h"
#include "crypto/Crypto.h"

QTEST_GUILESS_MAIN(TestEntry)

void TestEntry::initTestCase()
{
    QVERIFY(Crypto::init());
}

void TestEntry::testHistoryItemDeletion()
{
    QScopedPointer<Entry> entry(new Entry());
    QPointer<Entry> historyEntry = new Entry();

    entry->addHistoryItem(historyEntry);
    QCOMPARE(entry->historyItems().size(), 1);

    QList<Entry*> historyEntriesToRemove;
    historyEntriesToRemove.append(historyEntry);
    entry->removeHistoryItems(historyEntriesToRemove);
    QCOMPARE(entry->historyItems().size(), 0);
    QVERIFY(historyEntry.isNull());
}

void TestEntry::testCopyDataFrom()
{
    QScopedPointer<Entry> entry(new Entry());

    entry->setTitle("testtitle");
    entry->attributes()->set("attr1", "abc");
    entry->attributes()->set("attr2", "def");

    entry->attachments()->set("test", "123");
    entry->attachments()->set("test2", "456");

    AutoTypeAssociations::Association assoc;
    assoc.window = "1";
    assoc.sequence = "2";
    entry->autoTypeAssociations()->add(assoc);
    assoc.window = "3";
    assoc.sequence = "4";
    entry->autoTypeAssociations()->add(assoc);

    QScopedPointer<Entry> entry2(new Entry());
    entry2->copyDataFrom(entry.data());

    QCOMPARE(entry2->title(), QString("testtitle"));
    QCOMPARE(entry2->attributes()->value("attr1"), QString("abc"));
    QCOMPARE(entry2->attributes()->value("attr2"), QString("def"));

    QCOMPARE(entry2->attachments()->keys().size(), 2);
    QCOMPARE(entry2->attachments()->value("test"), QByteArray("123"));
    QCOMPARE(entry2->attachments()->value("test2"), QByteArray("456"));

    QCOMPARE(entry2->autoTypeAssociations()->size(), 2);
    QCOMPARE(entry2->autoTypeAssociations()->get(0).window, QString("1"));
    QCOMPARE(entry2->autoTypeAssociations()->get(1).window, QString("3"));
}

void TestEntry::testClone()
{
    QScopedPointer<Entry> entryOrg(new Entry());
    entryOrg->setUuid(Uuid::random());
    entryOrg->setTitle("Original Title");
    entryOrg->beginUpdate();
    entryOrg->setTitle("New Title");
    entryOrg->endUpdate();
    TimeInfo entryOrgTime = entryOrg->timeInfo();
    QDateTime dateTime;
    dateTime.setTimeSpec(Qt::UTC);
    dateTime.setTime_t(60);
    entryOrgTime.setCreationTime(dateTime);
    entryOrg->setTimeInfo(entryOrgTime);

    QScopedPointer<Entry> entryCloneNone(entryOrg->clone(Entry::CloneNoFlags));
    QCOMPARE(entryCloneNone->uuid(), entryOrg->uuid());
    QCOMPARE(entryCloneNone->title(), QString("New Title"));
    QCOMPARE(entryCloneNone->historyItems().size(), 0);
    QCOMPARE(entryCloneNone->timeInfo().creationTime(), entryOrg->timeInfo().creationTime());

    QScopedPointer<Entry> entryCloneNewUuid(entryOrg->clone(Entry::CloneNewUuid));
    QVERIFY(entryCloneNewUuid->uuid() != entryOrg->uuid());
    QVERIFY(!entryCloneNewUuid->uuid().isNull());
    QCOMPARE(entryCloneNewUuid->title(), QString("New Title"));
    QCOMPARE(entryCloneNewUuid->historyItems().size(), 0);
    QCOMPARE(entryCloneNewUuid->timeInfo().creationTime(), entryOrg->timeInfo().creationTime());

    QScopedPointer<Entry> entryCloneResetTime(entryOrg->clone(Entry::CloneResetTimeInfo));
    QCOMPARE(entryCloneResetTime->uuid(), entryOrg->uuid());
    QCOMPARE(entryCloneResetTime->title(), QString("New Title"));
    QCOMPARE(entryCloneResetTime->historyItems().size(), 0);
    QVERIFY(entryCloneResetTime->timeInfo().creationTime() != entryOrg->timeInfo().creationTime());

    QScopedPointer<Entry> entryCloneHistory(entryOrg->clone(Entry::CloneIncludeHistory));
    QCOMPARE(entryCloneHistory->uuid(), entryOrg->uuid());
    QCOMPARE(entryCloneHistory->title(), QString("New Title"));
    QCOMPARE(entryCloneHistory->historyItems().size(), 1);
    QCOMPARE(entryCloneHistory->historyItems().at(0)->title(), QString("Original Title"));
    QCOMPARE(entryCloneHistory->timeInfo().creationTime(), entryOrg->timeInfo().creationTime());

    Database db;
    auto* entryOrgClone = entryOrg->clone(Entry::CloneNoFlags);
    entryOrgClone->setGroup(db.rootGroup());

    Entry* entryCloneUserRef = entryOrgClone->clone(Entry::CloneUserAsRef);
    entryCloneUserRef->setGroup(db.rootGroup());
    QCOMPARE(entryCloneUserRef->uuid(), entryOrgClone->uuid());
    QCOMPARE(entryCloneUserRef->title(), QString("New Title"));
    QCOMPARE(entryCloneUserRef->historyItems().size(), 0);
    QCOMPARE(entryCloneUserRef->timeInfo().creationTime(), entryOrgClone->timeInfo().creationTime());
    QVERIFY(entryCloneUserRef->attributes()->isReference(EntryAttributes::UserNameKey));
    QCOMPARE(entryCloneUserRef->resolvePlaceholder(entryCloneUserRef->username()), entryOrgClone->username());

    Entry* entryClonePassRef = entryOrgClone->clone(Entry::ClonePassAsRef);
    entryClonePassRef->setGroup(db.rootGroup());
    QCOMPARE(entryClonePassRef->uuid(), entryOrgClone->uuid());
    QCOMPARE(entryClonePassRef->title(), QString("New Title"));
    QCOMPARE(entryClonePassRef->historyItems().size(), 0);
    QCOMPARE(entryClonePassRef->timeInfo().creationTime(), entryOrgClone->timeInfo().creationTime());
    QVERIFY(entryClonePassRef->attributes()->isReference(EntryAttributes::PasswordKey));
    QCOMPARE(entryClonePassRef->resolvePlaceholder(entryCloneUserRef->password()), entryOrg->password());
}

void TestEntry::testResolveUrl()
{
    QScopedPointer<Entry> entry(new Entry());
    QString testUrl("www.google.com");
    QString testCmd("cmd://firefox " + testUrl);
    QString testComplexCmd("cmd://firefox --start-now --url 'http://" + testUrl + "' --quit");
    QString nonHttpUrl("ftp://google.com");
    QString noUrl("random text inserted here");

    // Test standard URL's
    QCOMPARE(entry->resolveUrl(""), QString(""));
    QCOMPARE(entry->resolveUrl(testUrl), "https://" + testUrl);
    QCOMPARE(entry->resolveUrl("http://" + testUrl), "http://" + testUrl);
    // Test cmd:// with no URL
    QCOMPARE(entry->resolveUrl("cmd://firefox"), QString(""));
    QCOMPARE(entry->resolveUrl("cmd://firefox --no-url"), QString(""));
    // Test cmd:// with URL's
    QCOMPARE(entry->resolveUrl(testCmd), "https://" + testUrl);
    QCOMPARE(entry->resolveUrl(testComplexCmd), "http://" + testUrl);
    // Test non-http URL
    QCOMPARE(entry->resolveUrl(nonHttpUrl), QString(""));
    // Test no URL
    QCOMPARE(entry->resolveUrl(noUrl), QString(""));
}

void TestEntry::testResolveUrlPlaceholders()
{
    Entry entry;
    entry.setUrl("https://user:pw@keepassxc.org:80/path/example.php?q=e&s=t+2#fragment");

    QString rmvscm("//user:pw@keepassxc.org:80/path/example.php?q=e&s=t+2#fragment"); // Entry URL without scheme name.
    QString scm("https"); // Scheme name of the entry URL.
    QString host("keepassxc.org"); // Host component of the entry URL.
    QString port("80"); // Port number of the entry URL.
    QString path("/path/example.php"); // Path component of the entry URL.
    QString query("q=e&s=t+2"); // Query information of the entry URL.
    QString userinfo("user:pw"); // User information of the entry URL.
    QString username("user"); // User name of the entry URL.
    QString password("pw"); // Password of the entry URL.
    QString fragment("fragment"); // Fragment of the entry URL.

    QCOMPARE(entry.resolvePlaceholder("{URL:RMVSCM}"), rmvscm);
    QCOMPARE(entry.resolvePlaceholder("{URL:WITHOUTSCHEME}"), rmvscm);
    QCOMPARE(entry.resolvePlaceholder("{URL:SCM}"), scm);
    QCOMPARE(entry.resolvePlaceholder("{URL:SCHEME}"), scm);
    QCOMPARE(entry.resolvePlaceholder("{URL:HOST}"), host);
    QCOMPARE(entry.resolvePlaceholder("{URL:PORT}"), port);
    QCOMPARE(entry.resolvePlaceholder("{URL:PATH}"), path);
    QCOMPARE(entry.resolvePlaceholder("{URL:QUERY}"), query);
    QCOMPARE(entry.resolvePlaceholder("{URL:USERINFO}"), userinfo);
    QCOMPARE(entry.resolvePlaceholder("{URL:USERNAME}"), username);
    QCOMPARE(entry.resolvePlaceholder("{URL:PASSWORD}"), password);
    QCOMPARE(entry.resolvePlaceholder("{URL:FRAGMENT}"), fragment);
}

void TestEntry::testResolveRecursivePlaceholders()
{
    Database db;
    auto* root = db.rootGroup();

    auto* entry1 = new Entry();
    entry1->setGroup(root);
    entry1->setUuid(Uuid::random());
    entry1->setTitle("{USERNAME}");
    entry1->setUsername("{PASSWORD}");
    entry1->setPassword("{URL}");
    entry1->setUrl("{S:CustomTitle}");
    entry1->attributes()->set("CustomTitle", "RecursiveValue");
    QCOMPARE(entry1->resolveMultiplePlaceholders(entry1->title()), QString("RecursiveValue"));

    auto* entry2 = new Entry();
    entry2->setGroup(root);
    entry2->setUuid(Uuid::random());
    entry2->setTitle("Entry2Title");
    entry2->setUsername("{S:CustomUserNameAttribute}");
    entry2->setPassword(QString("{REF:P@I:%1}").arg(entry1->uuid().toHex()));
    entry2->setUrl("http://{S:IpAddress}:{S:Port}/{S:Uri}");
    entry2->attributes()->set("CustomUserNameAttribute", "CustomUserNameValue");
    entry2->attributes()->set("IpAddress", "127.0.0.1");
    entry2->attributes()->set("Port", "1234");
    entry2->attributes()->set("Uri", "uri/path");

    auto* entry3 = new Entry();
    entry3->setGroup(root);
    entry3->setUuid(Uuid::random());
    entry3->setTitle(QString("{REF:T@I:%1}").arg(entry2->uuid().toHex()));
    entry3->setUsername(QString("{REF:U@I:%1}").arg(entry2->uuid().toHex()));
    entry3->setPassword(QString("{REF:P@I:%1}").arg(entry2->uuid().toHex()));
    entry3->setUrl(QString("{REF:A@I:%1}").arg(entry2->uuid().toHex()));

    QCOMPARE(entry3->resolveMultiplePlaceholders(entry3->title()), QString("Entry2Title"));
    QCOMPARE(entry3->resolveMultiplePlaceholders(entry3->username()), QString("CustomUserNameValue"));
    QCOMPARE(entry3->resolveMultiplePlaceholders(entry3->password()), QString("RecursiveValue"));
    QCOMPARE(entry3->resolveMultiplePlaceholders(entry3->url()), QString("http://127.0.0.1:1234/uri/path"));

    auto* entry4 = new Entry();
    entry4->setGroup(root);
    entry4->setUuid(Uuid::random());
    entry4->setTitle(QString("{REF:T@I:%1}").arg(entry3->uuid().toHex()));
    entry4->setUsername(QString("{REF:U@I:%1}").arg(entry3->uuid().toHex()));
    entry4->setPassword(QString("{REF:P@I:%1}").arg(entry3->uuid().toHex()));
    entry4->setUrl(QString("{REF:A@I:%1}").arg(entry3->uuid().toHex()));

    QCOMPARE(entry4->resolveMultiplePlaceholders(entry4->title()), QString("Entry2Title"));
    QCOMPARE(entry4->resolveMultiplePlaceholders(entry4->username()), QString("CustomUserNameValue"));
    QCOMPARE(entry4->resolveMultiplePlaceholders(entry4->password()), QString("RecursiveValue"));
    QCOMPARE(entry4->resolveMultiplePlaceholders(entry4->url()), QString("http://127.0.0.1:1234/uri/path"));

    auto* entry5 = new Entry();
    entry5->setGroup(root);
    entry5->setUuid(Uuid::random());
    entry5->attributes()->set("Scheme", "http");
    entry5->attributes()->set("Host", "host.org");
    entry5->attributes()->set("Port", "2017");
    entry5->attributes()->set("Path", "/some/path");
    entry5->attributes()->set("UserName", "username");
    entry5->attributes()->set("Password", "password");
    entry5->attributes()->set("Query", "q=e&t=s");
    entry5->attributes()->set("Fragment", "fragment");
    entry5->setUrl("{S:Scheme}://{S:UserName}:{S:Password}@{S:Host}:{S:Port}{S:Path}?{S:Query}#{S:Fragment}");
    entry5->setTitle("title+{URL:Path}+{URL:Fragment}+title");

    const QString url("http://username:password@host.org:2017/some/path?q=e&t=s#fragment");
    QCOMPARE(entry5->resolveMultiplePlaceholders(entry5->url()), url);
    QCOMPARE(entry5->resolveMultiplePlaceholders(entry5->title()), QString("title+/some/path+fragment+title"));

    auto* entry6 = new Entry();
    entry6->setGroup(root);
    entry6->setUuid(Uuid::random());
    entry6->setTitle(QString("{REF:T@I:%1}").arg(entry3->uuid().toHex()));
    entry6->setUsername(QString("{TITLE}"));
    entry6->setPassword(QString("{PASSWORD}"));

    QCOMPARE(entry6->resolvePlaceholder(entry6->title()), QString("Entry2Title"));
    QCOMPARE(entry6->resolvePlaceholder(entry6->username()), QString("Entry2Title"));
    QCOMPARE(entry6->resolvePlaceholder(entry6->password()), QString("{PASSWORD}"));

    auto* entry7 = new Entry();
    entry7->setGroup(root);
    entry7->setUuid(Uuid::random());
    entry7->setTitle(QString("{REF:T@I:%1} and something else").arg(entry3->uuid().toHex()));
    entry7->setUsername(QString("{TITLE}"));
    entry7->setPassword(QString("PASSWORD"));

    QCOMPARE(entry7->resolvePlaceholder(entry7->title()), QString("Entry2Title and something else"));
    QCOMPARE(entry7->resolvePlaceholder(entry7->username()), QString("Entry2Title and something else"));
    QCOMPARE(entry7->resolvePlaceholder(entry7->password()), QString("PASSWORD"));
}

void TestEntry::testResolveReferencePlaceholders()
{
    Database db;
    auto* root = db.rootGroup();

    auto* entry1 = new Entry();
    entry1->setGroup(root);
    entry1->setUuid(Uuid::random());
    entry1->setTitle("Title1");
    entry1->setUsername("Username1");
    entry1->setPassword("Password1");
    entry1->setUrl("Url1");
    entry1->setNotes("Notes1");
    entry1->attributes()->set("CustomAttribute1", "CustomAttributeValue1");

    auto* group = new Group();
    group->setParent(root);
    auto* entry2 = new Entry();
    entry2->setGroup(group);
    entry2->setUuid(Uuid::random());
    entry2->setTitle("Title2");
    entry2->setUsername("Username2");
    entry2->setPassword("Password2");
    entry2->setUrl("Url2");
    entry2->setNotes("Notes2");
    entry2->attributes()->set("CustomAttribute2", "CustomAttributeValue2");

    auto* entry3 = new Entry();
    entry3->setGroup(group);
    entry3->setUuid(Uuid::random());
    entry3->setTitle("{S:AttributeTitle}");
    entry3->setUsername("{S:AttributeUsername}");
    entry3->setPassword("{S:AttributePassword}");
    entry3->setUrl("{S:AttributeUrl}");
    entry3->setNotes("{S:AttributeNotes}");
    entry3->attributes()->set("AttributeTitle", "TitleValue");
    entry3->attributes()->set("AttributeUsername", "UsernameValue");
    entry3->attributes()->set("AttributePassword", "PasswordValue");
    entry3->attributes()->set("AttributeUrl", "UrlValue");
    entry3->attributes()->set("AttributeNotes", "NotesValue");

    auto* tstEntry = new Entry();
    tstEntry->setGroup(root);
    tstEntry->setUuid(Uuid::random());

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@I:%1}").arg(entry1->uuid().toHex())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@T:%1}").arg(entry1->title())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@U:%1}").arg(entry1->username())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@P:%1}").arg(entry1->password())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@A:%1}").arg(entry1->url())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@N:%1}").arg(entry1->notes())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@O:%1}").arg(entry1->attributes()->value("CustomAttribute1"))), entry1->title());

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@I:%1}").arg(entry1->uuid().toHex())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@T:%1}").arg(entry1->title())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:U@U:%1}").arg(entry1->username())), entry1->username());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:P@P:%1}").arg(entry1->password())), entry1->password());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:A@A:%1}").arg(entry1->url())), entry1->url());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:N@N:%1}").arg(entry1->notes())), entry1->notes());

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@I:%1}").arg(entry2->uuid().toHex())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@T:%1}").arg(entry2->title())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@U:%1}").arg(entry2->username())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@P:%1}").arg(entry2->password())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@A:%1}").arg(entry2->url())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@N:%1}").arg(entry2->notes())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@O:%1}").arg(entry2->attributes()->value("CustomAttribute2"))), entry2->title());

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@T:%1}").arg(entry2->title())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:U@U:%1}").arg(entry2->username())), entry2->username());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:P@P:%1}").arg(entry2->password())), entry2->password());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:A@A:%1}").arg(entry2->url())), entry2->url());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:N@N:%1}").arg(entry2->notes())), entry2->notes());

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@I:%1}").arg(entry3->uuid().toHex())), entry3->attributes()->value("AttributeTitle"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:U@I:%1}").arg(entry3->uuid().toHex())), entry3->attributes()->value("AttributeUsername"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:P@I:%1}").arg(entry3->uuid().toHex())), entry3->attributes()->value("AttributePassword"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:A@I:%1}").arg(entry3->uuid().toHex())), entry3->attributes()->value("AttributeUrl"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:N@I:%1}").arg(entry3->uuid().toHex())), entry3->attributes()->value("AttributeNotes"));

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@I:%1}").arg(entry3->uuid().toHex().toUpper())), entry3->attributes()->value("AttributeTitle"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:U@I:%1}").arg(entry3->uuid().toHex().toUpper())), entry3->attributes()->value("AttributeUsername"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:P@I:%1}").arg(entry3->uuid().toHex().toUpper())), entry3->attributes()->value("AttributePassword"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:A@I:%1}").arg(entry3->uuid().toHex().toUpper())), entry3->attributes()->value("AttributeUrl"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:N@I:%1}").arg(entry3->uuid().toHex().toUpper())), entry3->attributes()->value("AttributeNotes"));

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:t@i:%1}").arg(entry3->uuid().toHex().toLower())), entry3->attributes()->value("AttributeTitle"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:u@i:%1}").arg(entry3->uuid().toHex().toLower())), entry3->attributes()->value("AttributeUsername"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:p@i:%1}").arg(entry3->uuid().toHex().toLower())), entry3->attributes()->value("AttributePassword"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:a@i:%1}").arg(entry3->uuid().toHex().toLower())), entry3->attributes()->value("AttributeUrl"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:n@i:%1}").arg(entry3->uuid().toHex().toLower())), entry3->attributes()->value("AttributeNotes"));
}

void TestEntry::testResolveNonIdPlaceholdersToUuid()
{
    Database db;
    auto* root = db.rootGroup();

    auto* referencedEntryTitle = new Entry();
    referencedEntryTitle->setGroup(root);
    referencedEntryTitle->setTitle("myTitle");
    referencedEntryTitle->setUuid(Uuid::random());

    auto* referencedEntryUsername = new Entry();
    referencedEntryUsername->setGroup(root);
    referencedEntryUsername->setUsername("myUser");
    referencedEntryUsername->setUuid(Uuid::random());

    auto* referencedEntryPassword = new Entry();
    referencedEntryPassword->setGroup(root);
    referencedEntryPassword->setPassword("myPassword");
    referencedEntryPassword->setUuid(Uuid::random());

    auto* referencedEntryUrl = new Entry();
    referencedEntryUrl->setGroup(root);
    referencedEntryUrl->setUrl("myUrl");
    referencedEntryUrl->setUuid(Uuid::random());

    auto* referencedEntryNotes = new Entry();
    referencedEntryNotes->setGroup(root);
    referencedEntryNotes->setNotes("myNotes");
    referencedEntryNotes->setUuid(Uuid::random());

    const QList<QChar> placeholders{'T', 'U', 'P', 'A', 'N'};
    for (const QChar& searchIn : placeholders) {
        const Entry* referencedEntry = nullptr;
        QString newEntryNotesRaw("{REF:I@%1:%2}");

        switch(searchIn.toLatin1()) {
            case 'T':
                referencedEntry = referencedEntryTitle;
                newEntryNotesRaw = newEntryNotesRaw.arg(searchIn, referencedEntry->title());
                break;
            case 'U':
                referencedEntry = referencedEntryUsername;
                newEntryNotesRaw = newEntryNotesRaw.arg(searchIn, referencedEntry->username());
                break;
            case 'P':
                referencedEntry = referencedEntryPassword;
                newEntryNotesRaw = newEntryNotesRaw.arg(searchIn, referencedEntry->password());
                break;
            case 'A':
                referencedEntry = referencedEntryUrl;
                newEntryNotesRaw = newEntryNotesRaw.arg(searchIn, referencedEntry->url());
                break;
            case 'N':
                referencedEntry = referencedEntryNotes;
                newEntryNotesRaw = newEntryNotesRaw.arg(searchIn, referencedEntry->notes());
                break;
            default:
                break;
        }

        auto* newEntry = new Entry();
        newEntry->setGroup(root);
        newEntry->setNotes(newEntryNotesRaw);

        const QString newEntryNotesResolved =
            newEntry->resolveMultiplePlaceholders(newEntry->notes());
        QCOMPARE(newEntryNotesResolved, QString(referencedEntry->uuid().toHex()));
    }
}

void TestEntry::testResolveClonedEntry()
{
    Database db;
    auto* root = db.rootGroup();

    auto* original = new Entry();
    original->setGroup(root);
    original->setUuid(Uuid::random());
    original->setTitle("Title");
    original->setUsername("SomeUsername");
    original->setPassword("SomePassword");

    QCOMPARE(original->resolveMultiplePlaceholders(original->username()), original->username());
    QCOMPARE(original->resolveMultiplePlaceholders(original->password()), original->password());

    // Top-level clones.
    Entry* clone1 = original->clone(Entry::CloneNewUuid);
    clone1->setGroup(root);
    Entry* clone2 = original->clone(Entry::CloneUserAsRef | Entry::CloneNewUuid);
    clone2->setGroup(root);
    Entry* clone3 = original->clone(Entry::ClonePassAsRef | Entry::CloneNewUuid);
    clone3->setGroup(root);
    Entry* clone4 = original->clone(Entry::CloneUserAsRef | Entry::ClonePassAsRef | Entry::CloneNewUuid);
    clone4->setGroup(root);

    QCOMPARE(clone1->resolveMultiplePlaceholders(clone1->username()), original->username());
    QCOMPARE(clone1->resolveMultiplePlaceholders(clone1->password()), original->password());
    QCOMPARE(clone2->resolveMultiplePlaceholders(clone2->username()), original->username());
    QCOMPARE(clone2->resolveMultiplePlaceholders(clone2->password()), original->password());
    QCOMPARE(clone3->resolveMultiplePlaceholders(clone3->username()), original->username());
    QCOMPARE(clone3->resolveMultiplePlaceholders(clone3->password()), original->password());
    QCOMPARE(clone4->resolveMultiplePlaceholders(clone4->username()), original->username());
    QCOMPARE(clone4->resolveMultiplePlaceholders(clone4->password()), original->password());

    // Second-level clones.
    Entry* cclone1 = clone4->clone(Entry::CloneNewUuid);
    cclone1->setGroup(root);
    Entry* cclone2 = clone4->clone(Entry::CloneUserAsRef | Entry::CloneNewUuid);
    cclone2->setGroup(root);
    Entry* cclone3 = clone4->clone(Entry::ClonePassAsRef | Entry::CloneNewUuid);
    cclone3->setGroup(root);
    Entry* cclone4 = clone4->clone(Entry::CloneUserAsRef | Entry::ClonePassAsRef | Entry::CloneNewUuid);
    cclone4->setGroup(root);

    QCOMPARE(cclone1->resolveMultiplePlaceholders(cclone1->username()), original->username());
    QCOMPARE(cclone1->resolveMultiplePlaceholders(cclone1->password()), original->password());
    QCOMPARE(cclone2->resolveMultiplePlaceholders(cclone2->username()), original->username());
    QCOMPARE(cclone2->resolveMultiplePlaceholders(cclone2->password()), original->password());
    QCOMPARE(cclone3->resolveMultiplePlaceholders(cclone3->username()), original->username());
    QCOMPARE(cclone3->resolveMultiplePlaceholders(cclone3->password()), original->password());
    QCOMPARE(cclone4->resolveMultiplePlaceholders(cclone4->username()), original->username());
    QCOMPARE(cclone4->resolveMultiplePlaceholders(cclone4->password()), original->password());

    // Change the original's attributes and make sure that the changes are tracked.
    QString oldUsername = original->username();
    QString oldPassword = original->password();
    original->setUsername("DifferentUsername");
    original->setPassword("DifferentPassword");

    QCOMPARE(clone1->resolveMultiplePlaceholders(clone1->username()), oldUsername);
    QCOMPARE(clone1->resolveMultiplePlaceholders(clone1->password()), oldPassword);
    QCOMPARE(clone2->resolveMultiplePlaceholders(clone2->username()), original->username());
    QCOMPARE(clone2->resolveMultiplePlaceholders(clone2->password()), oldPassword);
    QCOMPARE(clone3->resolveMultiplePlaceholders(clone3->username()), oldUsername);
    QCOMPARE(clone3->resolveMultiplePlaceholders(clone3->password()), original->password());
    QCOMPARE(clone4->resolveMultiplePlaceholders(clone4->username()), original->username());
    QCOMPARE(clone4->resolveMultiplePlaceholders(clone4->password()), original->password());

    QCOMPARE(cclone1->resolveMultiplePlaceholders(cclone1->username()), original->username());
    QCOMPARE(cclone1->resolveMultiplePlaceholders(cclone1->password()), original->password());
    QCOMPARE(cclone2->resolveMultiplePlaceholders(cclone2->username()), original->username());
    QCOMPARE(cclone2->resolveMultiplePlaceholders(cclone2->password()), original->password());
    QCOMPARE(cclone3->resolveMultiplePlaceholders(cclone3->username()), original->username());
    QCOMPARE(cclone3->resolveMultiplePlaceholders(cclone3->password()), original->password());
    QCOMPARE(cclone4->resolveMultiplePlaceholders(cclone4->username()), original->username());
    QCOMPARE(cclone4->resolveMultiplePlaceholders(cclone4->password()), original->password());
}
