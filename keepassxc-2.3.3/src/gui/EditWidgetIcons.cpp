/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#include "EditWidgetIcons.h"
#include "ui_EditWidgetIcons.h"

#include <QFileDialog>
#include <QMessageBox>

#include "core/Config.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/Tools.h"
#include "gui/IconModels.h"
#include "gui/MessageBox.h"

#ifdef WITH_XC_NETWORKING
#include <QtNetwork>
#endif

IconStruct::IconStruct()
    : uuid(Uuid())
    , number(0)
{
}

UrlFetchProgressDialog::UrlFetchProgressDialog(const QUrl &url, QWidget *parent)
  : QProgressDialog(parent)
{
    setWindowTitle(tr("Download Progress"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setLabelText(tr("Downloading %1.").arg(url.toDisplayString()));
    setMinimum(0);
    setValue(0);
    setMinimumDuration(0);
    setMinimumSize(QSize(400, 75));
}

void UrlFetchProgressDialog::networkReplyProgress(qint64 bytesRead, qint64 totalBytes)
{
    setMaximum(totalBytes);
    setValue(bytesRead);
}

EditWidgetIcons::EditWidgetIcons(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::EditWidgetIcons())
    , m_database(nullptr)
#ifdef WITH_XC_NETWORKING
    , m_reply(nullptr)
#endif
    , m_defaultIconModel(new DefaultIconModel(this))
    , m_customIconModel(new CustomIconModel(this))
{
    m_ui->setupUi(this);

    m_ui->defaultIconsView->setModel(m_defaultIconModel);
    m_ui->customIconsView->setModel(m_customIconModel);

    connect(m_ui->defaultIconsView, SIGNAL(clicked(QModelIndex)),
            this, SLOT(updateRadioButtonDefaultIcons()));
    connect(m_ui->customIconsView, SIGNAL(clicked(QModelIndex)),
            this, SLOT(updateRadioButtonCustomIcons()));
    connect(m_ui->defaultIconsRadio, SIGNAL(toggled(bool)),
            this, SLOT(updateWidgetsDefaultIcons(bool)));
    connect(m_ui->customIconsRadio, SIGNAL(toggled(bool)),
            this, SLOT(updateWidgetsCustomIcons(bool)));
    connect(m_ui->addButton, SIGNAL(clicked()), SLOT(addCustomIconFromFile()));
    connect(m_ui->deleteButton, SIGNAL(clicked()), SLOT(removeCustomIcon()));
    connect(m_ui->faviconButton, SIGNAL(clicked()), SLOT(downloadFavicon()));

    m_ui->faviconButton->setVisible(false);
}

EditWidgetIcons::~EditWidgetIcons()
{
}

IconStruct EditWidgetIcons::state()
{
    Q_ASSERT(m_database);
    Q_ASSERT(!m_currentUuid.isNull());

    IconStruct iconStruct;
    if (m_ui->defaultIconsRadio->isChecked()) {
        QModelIndex index = m_ui->defaultIconsView->currentIndex();
        if (index.isValid()) {
            iconStruct.number = index.row();
        } else {
            Q_ASSERT(false);
        }
    } else {
        QModelIndex index = m_ui->customIconsView->currentIndex();
        if (index.isValid()) {
            iconStruct.uuid = m_customIconModel->uuidFromIndex(m_ui->customIconsView->currentIndex());
        } else {
            iconStruct.number = -1;
        }
    }

    return iconStruct;
}

void EditWidgetIcons::reset()
{
    m_database = nullptr;
    m_currentUuid = Uuid();
}

void EditWidgetIcons::load(const Uuid& currentUuid, Database* database, const IconStruct& iconStruct, const QString& url)
{
    Q_ASSERT(database);
    Q_ASSERT(!currentUuid.isNull());

    m_database = database;
    m_currentUuid = currentUuid;
    setUrl(url);

    m_customIconModel->setIcons(database->metadata()->customIconsScaledPixmaps(),
                                database->metadata()->customIconsOrder());

    Uuid iconUuid = iconStruct.uuid;
    if (iconUuid.isNull()) {
        int iconNumber = iconStruct.number;
        m_ui->defaultIconsView->setCurrentIndex(m_defaultIconModel->index(iconNumber, 0));
        m_ui->defaultIconsRadio->setChecked(true);
    } else {
        QModelIndex index = m_customIconModel->indexFromUuid(iconUuid);
        if (index.isValid()) {
            m_ui->customIconsView->setCurrentIndex(index);
            m_ui->customIconsRadio->setChecked(true);
        } else {
            m_ui->defaultIconsView->setCurrentIndex(m_defaultIconModel->index(0, 0));
            m_ui->defaultIconsRadio->setChecked(true);
        }
    }
}

void EditWidgetIcons::setUrl(const QString& url)
{
#ifdef WITH_XC_NETWORKING
    m_url = QUrl(url);
    m_ui->faviconButton->setVisible(!url.isEmpty());
#else
    Q_UNUSED(url);
    m_ui->faviconButton->setVisible(false);
#endif
}

#ifdef WITH_XC_NETWORKING
namespace {
    // Try to get the 2nd level domain of the host part of a QUrl. For example,
    // "foo.bar.example.com" would become "example.com", and "foo.bar.example.co.uk"
    // would become "example.co.uk".
    QString getSecondLevelDomain(QUrl url)
    {
        QString fqdn = url.host();
        fqdn.truncate(fqdn.length() - url.topLevelDomain().length());
        QStringList parts = fqdn.split('.');
        QString newdom = parts.takeLast() + url.topLevelDomain();
        return newdom;
    }

    QUrl convertVariantToUrl(QVariant var)
    {
        QUrl url;
        if (var.canConvert<QUrl>())
            url = var.value<QUrl>();
        return url;
    }

    QUrl getRedirectTarget(QNetworkReply *reply)
    {
        QVariant var = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        QUrl url = convertVariantToUrl(var);
        return url;
    }
}
#endif

void EditWidgetIcons::downloadFavicon()
{
#ifdef WITH_XC_NETWORKING
    m_ui->faviconButton->setDisabled(true);

    m_redirects = 0;
    m_urlsToTry.clear();

    QString fullyQualifiedDomain = m_url.host();
    QString secondLevelDomain = getSecondLevelDomain(m_url);

    // Attempt to simply load the favicon.ico file
    if (fullyQualifiedDomain != secondLevelDomain) {
        m_urlsToTry.append(QUrl(m_url.scheme() + "://" + fullyQualifiedDomain + "/favicon.ico"));
    }
    m_urlsToTry.append(QUrl(m_url.scheme() + "://" + secondLevelDomain + "/favicon.ico"));

    // Try to use Google fallback, if enabled
    if (config()->get("security/IconDownloadFallbackToGoogle", false).toBool()) {
        QUrl urlGoogle = QUrl("https://www.google.com/s2/favicons");

        urlGoogle.setQuery("domain=" + QUrl::toPercentEncoding(secondLevelDomain));
        m_urlsToTry.append(urlGoogle);
    }

    startFetchFavicon(m_urlsToTry.takeFirst());
#endif
}

void EditWidgetIcons::fetchReadyRead()
{
#ifdef WITH_XC_NETWORKING
    m_bytesReceived += m_reply->readAll();
#endif
}

void EditWidgetIcons::fetchFinished()
{
#ifdef WITH_XC_NETWORKING
    QImage image;
    bool googleFallbackEnabled = config()->get("security/IconDownloadFallbackToGoogle", false).toBool();
    bool error = (m_reply->error() != QNetworkReply::NoError);
    QUrl redirectTarget = getRedirectTarget(m_reply);

    m_reply->deleteLater();
    m_reply = nullptr;

    if (!error) {
        if (redirectTarget.isValid()) {
            // Redirected, we need to follow it, or fall through if we have
            // done too many redirects already.
            if (m_redirects < 5) {
                m_redirects++;
                if (redirectTarget.isRelative())
                    redirectTarget = m_fetchUrl.resolved(redirectTarget);
                startFetchFavicon(redirectTarget);
                return;
            }
        } else {
            // No redirect, and we theoretically have some icon data now.
            image.loadFromData(m_bytesReceived);
        }
    }

    if (!image.isNull()) {
        addCustomIcon(image);
    } else if (!m_urlsToTry.empty()) {
        m_redirects = 0;
        startFetchFavicon(m_urlsToTry.takeFirst());
        return;
    } else {
        if (!googleFallbackEnabled) {
            emit messageEditEntry(tr("Unable to fetch favicon.") + "\n" +
                                  tr("Hint: You can enable Google as a fallback under Tools>Settings>Security"),
                                  MessageWidget::Error);
        } else {
            emit messageEditEntry(tr("Unable to fetch favicon."), MessageWidget::Error);
        }
    }

    m_ui->faviconButton->setDisabled(false);
#endif
}

void EditWidgetIcons::fetchCanceled()
{
#ifdef WITH_XC_NETWORKING
    m_reply->abort();
#endif
}

void EditWidgetIcons::startFetchFavicon(const QUrl& url)
{
#ifdef WITH_XC_NETWORKING
    m_bytesReceived.clear();

    m_fetchUrl = url;

    QNetworkRequest request(url);

    m_reply = m_netMgr.get(request);
    connect(m_reply, &QNetworkReply::finished, this, &EditWidgetIcons::fetchFinished);
    connect(m_reply, &QIODevice::readyRead, this, &EditWidgetIcons::fetchReadyRead);

    UrlFetchProgressDialog *progress = new UrlFetchProgressDialog(url, this);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_reply, &QNetworkReply::finished, progress, &QProgressDialog::hide);
    connect(m_reply, &QNetworkReply::downloadProgress, progress, &UrlFetchProgressDialog::networkReplyProgress);
    connect(progress, &QProgressDialog::canceled, this, &EditWidgetIcons::fetchCanceled);

    progress->show();
#else
    Q_UNUSED(url);
#endif
}

void EditWidgetIcons::addCustomIconFromFile()
{
    if (m_database) {
        QString filter = QString("%1 (%2);;%3 (*)").arg(tr("Images"),
                    Tools::imageReaderFilter(), tr("All files"));

        QString filename = QFileDialog::getOpenFileName(
                    this, tr("Select Image"), "", filter);
        if (!filename.isEmpty()) {
            auto icon = QImage(filename);
            if (!icon.isNull()) {
                addCustomIcon(QImage(filename));
            } else {
                emit messageEditEntry(tr("Can't read icon"), MessageWidget::Error);
            }
        }
    }
}

void EditWidgetIcons::addCustomIcon(const QImage& icon)
{
    if (m_database) {
        Uuid uuid = m_database->metadata()->findCustomIcon(icon);
        if (uuid.isNull()) {
            uuid = Uuid::random();
            // Don't add an icon larger than 128x128, but retain original size if smaller
            if (icon.width() > 128 || icon.height() > 128) {
                m_database->metadata()->addCustomIcon(uuid, icon.scaled(128, 128));
            } else {
                m_database->metadata()->addCustomIcon(uuid, icon);
            }

            m_customIconModel->setIcons(m_database->metadata()->customIconsScaledPixmaps(),
                                        m_database->metadata()->customIconsOrder());
        } else {
            emit messageEditEntry(tr("Custom icon already exists"), MessageWidget::Information);
        }

        // Select the new or existing icon
        updateRadioButtonCustomIcons();
        QModelIndex index = m_customIconModel->indexFromUuid(uuid);
        m_ui->customIconsView->setCurrentIndex(index);
    }
}

void EditWidgetIcons::removeCustomIcon()
{
    if (m_database) {
        QModelIndex index = m_ui->customIconsView->currentIndex();
        if (index.isValid()) {
            Uuid iconUuid = m_customIconModel->uuidFromIndex(index);

            const QList<Entry*> allEntries = m_database->rootGroup()->entriesRecursive(true);
            QList<Entry*> entriesWithSameIcon;
            QList<Entry*> historyEntriesWithSameIcon;

            for (Entry* entry : allEntries) {
                if (iconUuid == entry->iconUuid()) {
                    // Check if this is a history entry (no assigned group)
                    if (!entry->group()) {
                        historyEntriesWithSameIcon << entry;
                    } else if (m_currentUuid != entry->uuid()) {
                        entriesWithSameIcon << entry;
                    }
                }
            }

            const QList<Group*> allGroups = m_database->rootGroup()->groupsRecursive(true);
            QList<Group*> groupsWithSameIcon;

            for (Group* group : allGroups) {
                if (iconUuid == group->iconUuid() && m_currentUuid != group->uuid()) {
                    groupsWithSameIcon << group;
                }
            }

            int iconUseCount = entriesWithSameIcon.size() + groupsWithSameIcon.size();
            if (iconUseCount > 0) {
                QMessageBox::StandardButton ans = MessageBox::question(this, tr("Confirm Delete"),
                                     tr("This icon is used by %1 entries, and will be replaced "
                                        "by the default icon. Are you sure you want to delete it?")
                                     .arg(iconUseCount), QMessageBox::Yes | QMessageBox::No);

                if (ans == QMessageBox::No) {
                    // Early out, nothing is changed
                    return;
                } else {
                    // Revert matched entries to the default entry icon
                    for (Entry* entry : asConst(entriesWithSameIcon)) {
                        entry->setIcon(Entry::DefaultIconNumber);
                    }

                    // Revert matched groups to the default group icon
                    for (Group* group : asConst(groupsWithSameIcon)) {
                        group->setIcon(Group::DefaultIconNumber);
                    }
                }
            }


            // Remove the icon from history entries
            for (Entry* entry : asConst(historyEntriesWithSameIcon)) {
                entry->setUpdateTimeinfo(false);
                entry->setIcon(0);
                entry->setUpdateTimeinfo(true);
            }

            // Remove the icon from the database
            m_database->metadata()->removeCustomIcon(iconUuid);
            m_customIconModel->setIcons(m_database->metadata()->customIconsScaledPixmaps(),
                                        m_database->metadata()->customIconsOrder());

            // Reset the current icon view
            updateRadioButtonDefaultIcons();

            if (m_database->resolveEntry(m_currentUuid) != nullptr) {
                m_ui->defaultIconsView->setCurrentIndex(m_defaultIconModel->index(Entry::DefaultIconNumber));
            } else {
                m_ui->defaultIconsView->setCurrentIndex(m_defaultIconModel->index(Group::DefaultIconNumber));
            }
        }
    }
}

void EditWidgetIcons::updateWidgetsDefaultIcons(bool check)
{
    if (check) {
        QModelIndex index = m_ui->defaultIconsView->currentIndex();
        if (!index.isValid()) {
            m_ui->defaultIconsView->setCurrentIndex(m_defaultIconModel->index(0, 0));
        } else {
            m_ui->defaultIconsView->setCurrentIndex(index);
        }
        m_ui->customIconsView->selectionModel()->clearSelection();
        m_ui->addButton->setEnabled(false);
        m_ui->deleteButton->setEnabled(false);
    }
}

void EditWidgetIcons::updateWidgetsCustomIcons(bool check)
{
    if (check) {
        QModelIndex index = m_ui->customIconsView->currentIndex();
        if (!index.isValid()) {
            m_ui->customIconsView->setCurrentIndex(m_customIconModel->index(0, 0));
        } else {
            m_ui->customIconsView->setCurrentIndex(index);
        }
        m_ui->defaultIconsView->selectionModel()->clearSelection();
        m_ui->addButton->setEnabled(true);
        m_ui->deleteButton->setEnabled(true);
    }
}

void EditWidgetIcons::updateRadioButtonDefaultIcons()
{
    m_ui->defaultIconsRadio->setChecked(true);
}

void EditWidgetIcons::updateRadioButtonCustomIcons()
{
    m_ui->customIconsRadio->setChecked(true);
}
