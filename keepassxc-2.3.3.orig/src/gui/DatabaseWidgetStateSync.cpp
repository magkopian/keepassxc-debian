/*
 * Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 * Copyright (C) 2014 Felix Geyer <debfx@fobos.de>
 * Copyright (C) 2014 Florian Geyer <blueice@fobos.de>
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

#include "DatabaseWidgetStateSync.h"

#include "core/Config.h"
#include <QCoreApplication>

DatabaseWidgetStateSync::DatabaseWidgetStateSync(QObject* parent)
    : QObject(parent)
    , m_activeDbWidget(nullptr)
    , m_blockUpdates(false)
{
    m_mainSplitterSizes = variantToIntList(config()->get("GUI/SplitterState"));
    m_detailSplitterSizes = variantToIntList(config()->get("GUI/DetailSplitterState"));
    m_hideUsernames = config()->get("GUI/HideUsernames").toBool();
    m_hidePasswords = config()->get("GUI/HidePasswords").toBool();
    m_listViewState = config()->get("GUI/ListViewState").toByteArray();
    m_searchViewState = config()->get("GUI/SearchViewState").toByteArray();

    connect(qApp, &QCoreApplication::aboutToQuit, this, &DatabaseWidgetStateSync::sync);
}

DatabaseWidgetStateSync::~DatabaseWidgetStateSync()
{
}

/**
 * Sync state with persistent storage.
 */
void DatabaseWidgetStateSync::sync()
{
    config()->set("GUI/SplitterState", intListToVariant(m_mainSplitterSizes));
    config()->set("GUI/DetailSplitterState", intListToVariant(m_detailSplitterSizes));
    config()->set("GUI/HideUsernames", m_hideUsernames);
    config()->set("GUI/HidePasswords", m_hidePasswords);
    config()->set("GUI/ListViewState", m_listViewState);
    config()->set("GUI/SearchViewState", m_searchViewState);
    config()->sync();
}

void DatabaseWidgetStateSync::setActive(DatabaseWidget* dbWidget)
{
    if (m_activeDbWidget) {
        disconnect(m_activeDbWidget, nullptr, this, nullptr);
    }

    m_activeDbWidget = dbWidget;

    if (m_activeDbWidget) {
        m_blockUpdates = true;

        if (!m_mainSplitterSizes.isEmpty()) {
            m_activeDbWidget->setMainSplitterSizes(m_mainSplitterSizes);
        }

        if (!m_detailSplitterSizes.isEmpty()) {
            m_activeDbWidget->setDetailSplitterSizes(m_detailSplitterSizes);
        }

        if (m_activeDbWidget->isInSearchMode()) {
            restoreSearchView();
        } else {
            restoreListView();
        }

        m_blockUpdates = false;

        connect(m_activeDbWidget, SIGNAL(mainSplitterSizesChanged()),
                SLOT(updateSplitterSizes()));
        connect(m_activeDbWidget, SIGNAL(detailSplitterSizesChanged()),
                SLOT(updateSplitterSizes()));
        connect(m_activeDbWidget, SIGNAL(entryViewStateChanged()),
                SLOT(updateViewState()));
        connect(m_activeDbWidget, SIGNAL(listModeActivated()),
                SLOT(restoreListView()));
        connect(m_activeDbWidget, SIGNAL(searchModeActivated()),
                SLOT(restoreSearchView()));
        connect(m_activeDbWidget, SIGNAL(listModeAboutToActivate()),
                SLOT(blockUpdates()));
        connect(m_activeDbWidget, SIGNAL(searchModeAboutToActivate()),
                SLOT(blockUpdates()));
    }
}

/**
 * Restore entry view list view state
 *
 * NOTE:
 * States of entry view 'Hide Usernames'/'Hide Passwords' settings are global,
 * i.e. they are the same for both list and search mode
 *
 * NOTE:
 * If m_listViewState is empty, the list view has been activated for the first
 * time after starting with a clean (or invalid) config. Thus, save the current
 * state. Without this, m_listViewState would remain empty until there is an
 * actual view state change (e.g. column is resized)
 */
void DatabaseWidgetStateSync::restoreListView()
{
    m_activeDbWidget->setUsernamesHidden(m_hideUsernames);
    m_activeDbWidget->setPasswordsHidden(m_hidePasswords);

    if (!m_listViewState.isEmpty()) {
        m_activeDbWidget->setEntryViewState(m_listViewState);
    } else {
        m_listViewState = m_activeDbWidget->entryViewState();
    }

    m_blockUpdates = false;
}

/**
 * Restore entry view search view state
 *
 * NOTE:
 * States of entry view 'Hide Usernames'/'Hide Passwords' settings are global,
 * i.e. they are the same for both list and search mode
 *
 * NOTE:
 * If m_searchViewState is empty, the search view has been activated for the
 * first time after starting with a clean (or invalid) config. Thus, save the
 * current state. Without this, m_searchViewState would remain empty until
 * there is an actual view state change (e.g. column is resized)
 */
void DatabaseWidgetStateSync::restoreSearchView()
{
    m_activeDbWidget->setUsernamesHidden(m_hideUsernames);
    m_activeDbWidget->setPasswordsHidden(m_hidePasswords);

    if (!m_searchViewState.isEmpty()) {
        m_activeDbWidget->setEntryViewState(m_searchViewState);
    } else {
        m_searchViewState = m_activeDbWidget->entryViewState();
    }

    m_blockUpdates = false;
}

void DatabaseWidgetStateSync::blockUpdates()
{
    m_blockUpdates = true;
}

void DatabaseWidgetStateSync::updateSplitterSizes()
{
    if (m_blockUpdates) {
        return;
    }

    m_mainSplitterSizes = m_activeDbWidget->mainSplitterSizes();
    m_detailSplitterSizes = m_activeDbWidget->detailSplitterSizes();
}

/**
 * Update entry view list/search view state
 *
 * NOTE:
 * States of entry view 'Hide Usernames'/'Hide Passwords' settings are global,
 * i.e. they are the same for both list and search mode
 */
void DatabaseWidgetStateSync::updateViewState()
{
    if (m_blockUpdates) {
        return;
    }

    m_hideUsernames = m_activeDbWidget->isUsernamesHidden();
    m_hidePasswords = m_activeDbWidget->isPasswordsHidden();

    if (m_activeDbWidget->isInSearchMode()) {
        m_searchViewState = m_activeDbWidget->entryViewState();
    } else {
        m_listViewState = m_activeDbWidget->entryViewState();
    }
}

QList<int> DatabaseWidgetStateSync::variantToIntList(const QVariant& variant)
{
    const QVariantList list = variant.toList();
    QList<int> result;

    for (const QVariant& var : list) {
        bool ok;
        int size = var.toInt(&ok);
        if (ok) {
            result.append(size);
        } else {
            result.clear();
            break;
        }
    }

    return result;
}

QVariant DatabaseWidgetStateSync::intListToVariant(const QList<int>& list)
{
    QVariantList result;

    for (int value : list) {
        result.append(value);
    }

    return result;
}
