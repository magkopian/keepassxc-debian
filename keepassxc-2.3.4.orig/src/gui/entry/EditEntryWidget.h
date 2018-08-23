/*
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_EDITENTRYWIDGET_H
#define KEEPASSX_EDITENTRYWIDGET_H

#include <QModelIndex>
#include <QScopedPointer>
#include <QButtonGroup>

#include "gui/EditWidget.h"
#include "config-keepassx.h"

class AutoTypeAssociations;
class AutoTypeAssociationsModel;
class Database;
class EditWidgetIcons;
class EditWidgetProperties;
class Entry;
class EntryAttributes;
class EntryAttributesModel;
class EntryHistoryModel;
class QButtonGroup;
class QMenu;
class QSortFilterProxyModel;
class QStackedLayout;
#ifdef WITH_XC_SSHAGENT
#include "sshagent/KeeAgentSettings.h"
class OpenSSHKey;
#endif

namespace Ui {
    class EditEntryWidgetAdvanced;
    class EditEntryWidgetAutoType;
    class EditEntryWidgetSSHAgent;
    class EditEntryWidgetMain;
    class EditEntryWidgetHistory;
    class EditWidget;
}

class EditEntryWidget : public EditWidget
{
    Q_OBJECT

public:
    explicit EditEntryWidget(QWidget* parent = nullptr);
    ~EditEntryWidget();

    void loadEntry(Entry* entry, bool create, bool history, const QString& parentName,
                   Database* database);

    void createPresetsMenu(QMenu* expirePresetsMenu);
    QString entryTitle() const;
    void clear();
    bool hasBeenModified() const;

signals:
    void editFinished(bool accepted);
    void historyEntryActivated(Entry* entry);

private slots:
    void acceptEntry();
    bool commitEntry();
    void cancel();
    void togglePasswordGeneratorButton(bool checked);
    void setGeneratedPassword(const QString& password);
    void insertAttribute();
    void editCurrentAttribute();
    void removeCurrentAttribute();
    void updateCurrentAttribute();
    void protectCurrentAttribute(bool state);
    void revealCurrentAttribute();
    void updateAutoTypeEnabled();
    void insertAutoTypeAssoc();
    void removeAutoTypeAssoc();
    void loadCurrentAssoc(const QModelIndex& current);
    void clearCurrentAssoc();
    void applyCurrentAssoc();
    void showHistoryEntry();
    void restoreHistoryEntry();
    void deleteHistoryEntry();
    void deleteAllHistoryEntries();
    void emitHistoryEntryActivated(const QModelIndex& index);
    void histEntryActivated(const QModelIndex& index);
    void updateHistoryButtons(const QModelIndex& current, const QModelIndex& previous);
    void useExpiryPreset(QAction* action);
    void toggleHideNotes(bool visible);
    void pickColor();
#ifdef WITH_XC_SSHAGENT
    void updateSSHAgent();
    void updateSSHAgentAttachment();
    void updateSSHAgentAttachments();
    void updateSSHAgentKeyInfo();
    void browsePrivateKey();
    void addKeyToAgent();
    void removeKeyFromAgent();
    void decryptPrivateKey();
    void copyPublicKey();
#endif

private:
    void setupMain();
    void setupAdvanced();
    void setupIcon();
    void setupAutoType();
#ifdef WITH_XC_SSHAGENT
    void setupSSHAgent();
#endif
    void setupProperties();
    void setupHistory();
    void setupColorButton(bool foreground, const QColor& color);

    bool passwordsEqual();
    void setForms(const Entry* entry, bool restore = false);
    QMenu* createPresetsMenu();
    void updateEntryData(Entry* entry) const;
#ifdef WITH_XC_SSHAGENT
    bool getOpenSSHKey(OpenSSHKey& key, bool decrypt = false);
    void saveSSHAgentConfig();
#endif

    void displayAttribute(QModelIndex index, bool showProtected);

    Entry* m_entry;
    Database* m_database;

    bool m_create;
    bool m_history;
    bool m_saved;
#ifdef WITH_XC_SSHAGENT
    bool m_sshAgentEnabled;
    KeeAgentSettings m_sshAgentSettings;
#endif
    const QScopedPointer<Ui::EditEntryWidgetMain> m_mainUi;
    const QScopedPointer<Ui::EditEntryWidgetAdvanced> m_advancedUi;
    const QScopedPointer<Ui::EditEntryWidgetAutoType> m_autoTypeUi;
    const QScopedPointer<Ui::EditEntryWidgetSSHAgent> m_sshAgentUi;
    const QScopedPointer<Ui::EditEntryWidgetHistory> m_historyUi;
    QWidget* const m_mainWidget;
    QWidget* const m_advancedWidget;
    EditWidgetIcons* const m_iconsWidget;
    QWidget* const m_autoTypeWidget;
    QWidget* const m_sshAgentWidget;
    EditWidgetProperties* const m_editWidgetProperties;
    QWidget* const m_historyWidget;
    EntryAttributes* const m_entryAttributes;
    EntryAttributesModel* const m_attributesModel;
    EntryHistoryModel* const m_historyModel;
    QSortFilterProxyModel* const m_sortModel;
    QPersistentModelIndex m_currentAttribute;
    AutoTypeAssociations* const m_autoTypeAssoc;
    AutoTypeAssociationsModel* const m_autoTypeAssocModel;
    QButtonGroup* const m_autoTypeDefaultSequenceGroup;
    QButtonGroup* const m_autoTypeWindowSequenceGroup;

    Q_DISABLE_COPY(EditEntryWidget)
};

#endif // KEEPASSX_EDITENTRYWIDGET_H
