/*
 *  Copyright (C) 2016 Jonathan White <support@dmapps.us>
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

#include "SearchWidget.h"
#include "ui_SearchWidget.h"

#include <QKeyEvent>
#include <QMenu>
#include <QShortcut>
#include <QToolButton>

#include "core/Config.h"
#include "core/FilePath.h"

SearchWidget::SearchWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::SearchWidget())
{
    m_ui->setupUi(this);

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);

    connect(m_ui->searchEdit, SIGNAL(textChanged(QString)), SLOT(startSearchTimer()));
    connect(m_ui->clearIcon, SIGNAL(triggered(bool)), m_ui->searchEdit, SLOT(clear()));
    connect(m_searchTimer, SIGNAL(timeout()), this, SLOT(startSearch()));
    connect(this, SIGNAL(escapePressed()), m_ui->searchEdit, SLOT(clear()));

    new QShortcut(Qt::CTRL + Qt::Key_F, this, SLOT(searchFocus()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(Qt::Key_Escape, m_ui->searchEdit, SLOT(clear()), nullptr, Qt::ApplicationShortcut);

    m_ui->searchEdit->installEventFilter(this);

    QMenu* searchMenu = new QMenu();
    m_actionCaseSensitive = searchMenu->addAction(tr("Case Sensitive"), this, SLOT(updateCaseSensitive()));
    m_actionCaseSensitive->setObjectName("actionSearchCaseSensitive");
    m_actionCaseSensitive->setCheckable(true);

    m_actionLimitGroup = searchMenu->addAction(tr("Limit search to selected group"), this, SLOT(updateLimitGroup()));
    m_actionLimitGroup->setObjectName("actionSearchLimitGroup");
    m_actionLimitGroup->setCheckable(true);
    m_actionLimitGroup->setChecked(config()->get("SearchLimitGroup", false).toBool());

    m_ui->searchIcon->setIcon(filePath()->icon("actions", "system-search"));
    m_ui->searchIcon->setMenu(searchMenu);
    m_ui->searchEdit->addAction(m_ui->searchIcon, QLineEdit::LeadingPosition);

    m_ui->clearIcon->setIcon(filePath()->icon("actions", "edit-clear-locationbar-rtl"));
    m_ui->clearIcon->setVisible(false);
    m_ui->searchEdit->addAction(m_ui->clearIcon, QLineEdit::TrailingPosition);

    // Fix initial visibility of actions (bug in Qt)
    for (QToolButton* toolButton : m_ui->searchEdit->findChildren<QToolButton*>()) {
        toolButton->setVisible(toolButton->defaultAction()->isVisible());
    }
}

SearchWidget::~SearchWidget()
{
}

bool SearchWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            emit escapePressed();
            return true;
        } else if (keyEvent->matches(QKeySequence::Copy)) {
            // If Control+C is pressed in the search edit when no text
            // is selected, copy the password of the current entry
            if (!m_ui->searchEdit->hasSelectedText()) {
                emit copyPressed();
                return true;
            }
        } else if (keyEvent->matches(QKeySequence::MoveToNextLine)) {
            if (m_ui->searchEdit->cursorPosition() == m_ui->searchEdit->text().length()) {
                // If down is pressed at EOL, move the focus to the entry view
                emit downPressed();
                return true;
            } else {
                // Otherwise move the cursor to EOL
                m_ui->searchEdit->setCursorPosition(m_ui->searchEdit->text().length());
                return true;
            }
        }
    }

    return QObject::eventFilter(obj, event);
}

void SearchWidget::connectSignals(SignalMultiplexer& mx)
{
    mx.connect(this, SIGNAL(search(QString)), SLOT(search(QString)));
    mx.connect(this, SIGNAL(caseSensitiveChanged(bool)), SLOT(setSearchCaseSensitive(bool)));
    mx.connect(this, SIGNAL(limitGroupChanged(bool)), SLOT(setSearchLimitGroup(bool)));
    mx.connect(this, SIGNAL(copyPressed()), SLOT(copyPassword()));
    mx.connect(this, SIGNAL(downPressed()), SLOT(setFocus()));
    mx.connect(m_ui->searchEdit, SIGNAL(returnPressed()), SLOT(switchToEntryEdit()));
}

void SearchWidget::databaseChanged(DatabaseWidget* dbWidget)
{
    if (dbWidget != nullptr) {
        // Set current search text from this database
        m_ui->searchEdit->setText(dbWidget->getCurrentSearch());

        // Enforce search policy
        emit caseSensitiveChanged(m_actionCaseSensitive->isChecked());
        emit limitGroupChanged(m_actionLimitGroup->isChecked());
    } else {
        m_ui->searchEdit->clear();
    }
}

void SearchWidget::startSearchTimer()
{
    if (!m_searchTimer->isActive()) {
        m_searchTimer->stop();
    }
    m_searchTimer->start(100);
}

void SearchWidget::startSearch()
{
    if (!m_searchTimer->isActive()) {
        m_searchTimer->stop();
    }

    bool hasText = m_ui->searchEdit->text().length() > 0;
    m_ui->clearIcon->setVisible(hasText);

    search(m_ui->searchEdit->text());
}

void SearchWidget::updateCaseSensitive()
{
    emit caseSensitiveChanged(m_actionCaseSensitive->isChecked());
}

void SearchWidget::setCaseSensitive(bool state)
{
    m_actionCaseSensitive->setChecked(state);
    updateCaseSensitive();
}

void SearchWidget::updateLimitGroup()
{
    config()->set("SearchLimitGroup", m_actionLimitGroup->isChecked());
    emit limitGroupChanged(m_actionLimitGroup->isChecked());
}

void SearchWidget::setLimitGroup(bool state)
{
    m_actionLimitGroup->setChecked(state);
    updateLimitGroup();
}


void SearchWidget::searchFocus()
{
    m_ui->searchEdit->setFocus();
    m_ui->searchEdit->selectAll();
}
