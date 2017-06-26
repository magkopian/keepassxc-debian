/*
 *  Copyright (C) 2016 KeePassXC Team <team@keepassxc.org>
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

#include "UnlockDatabaseDialog.h"
#include "UnlockDatabaseWidget.h"

#include "autotype/AutoType.h"
#include "core/Database.h"
#include "gui/DragTabBar.h"

UnlockDatabaseDialog::UnlockDatabaseDialog(QWidget* parent)
    : QDialog(parent)
    , m_view(new UnlockDatabaseWidget(this))
{
    connect(m_view, SIGNAL(editFinished(bool)), this, SLOT(complete(bool)));
}

void UnlockDatabaseDialog::setDBFilename(const QString& filename)
{
    m_view->load(filename);
}

void UnlockDatabaseDialog::clearForms()
{
    m_view->clearForms();
}

Database* UnlockDatabaseDialog::database()
{
    return m_view->database();
}

void UnlockDatabaseDialog::complete(bool r)
{
    if (r) {
        accept();
        emit unlockDone(true);
    } else {
        reject();
    }
}

Database* UnlockDatabaseDialog::openDatabasePrompt(QString databaseFilename)
{

    UnlockDatabaseDialog* unlockDatabaseDialog = new UnlockDatabaseDialog();
    unlockDatabaseDialog->setObjectName("Open database");
    unlockDatabaseDialog->setDBFilename(databaseFilename);
    unlockDatabaseDialog->show();
    unlockDatabaseDialog->exec();

    Database* db = unlockDatabaseDialog->database();
    if (!db) {
        qWarning("Could not open database %s.", qPrintable(databaseFilename));
    }
    delete unlockDatabaseDialog;
    return db;
}
