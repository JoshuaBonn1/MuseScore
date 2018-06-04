//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2002-2018 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include "workspacedialog.h"
#include "workspace.h"
#include "preferences.h"
#include "musescore.h"
#include "palettebox.h"

namespace Ms {

WorkspaceDialog::WorkspaceDialog(QWidget* parent)
   : QDialog(parent)
      {
      setObjectName("WorkspaceDialog");
      setupUi(this);
      setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
      MuseScore::restoreGeometry(this);

      connect(buttonBox, SIGNAL(accepted()), SLOT(accepted()));
      }

void WorkspaceDialog::display()
      {
      if (editMode) {
            componentsCheck->setChecked(Workspace::currentWorkspace->saveComponents);
            toolbarsCheck->setChecked(Workspace::currentWorkspace->saveToolbars);
            menubarCheck->setChecked(Workspace::currentWorkspace->saveMenuBar);
            prefsCheck->setChecked(Workspace::currentWorkspace->savePrefs);
            nameLineEdit->setText(Workspace::currentWorkspace->name());
            }
      else {
            componentsCheck->setChecked(false);
            toolbarsCheck->setChecked(false);
            menubarCheck->setChecked(false);
            prefsCheck->setChecked(false);
            nameLineEdit->setText("");
            }
      show();
      }

void WorkspaceDialog::accepted()
      {
      QString s = nameLineEdit->text();
      if (s.isEmpty())
            return;
      s = s.replace( QRegExp( "[" + QRegExp::escape( "\\/:*?\"<>|" ) + "]" ), "_" ); //FAT/NTFS special chars

      for (;;) {
            if (editMode && s == Workspace::currentWorkspace->name())
                  break;
            bool notFound = true;
            for (Workspace* p : Workspace::workspaces()) {
                  if ((qApp->translate("Ms::Workspace", p->name().toUtf8()).toLower() == s.toLower()) ||
                     (s.toLower() == QString("basic")) || (s.toLower() == QString("advanced"))) {
                        notFound = false;
                        break;
                        }
                  }
            if (!notFound) {
                  s = QInputDialog::getText(this,
                     tr("Read Workspace Name"),
                     tr("'%1' does already exist,\nplease choose a different name:").arg(s)
                     );
                  if (s.isEmpty())
                        return;
                  s = s.replace( QRegExp( "[" + QRegExp::escape( "\\/:*?\"<>|" ) + "]" ), "_" ); //FAT/NTFS special chars
                  }
            else
                  break;
            }

      // When this is accepted, I need to update the workspace. This will include the name and everything... So, I should check if it is in edit mode or creation mode?
      // That should be pretty easy, just set a flag whenever it is called.
      if (editMode) {
            // update all flags and name
            Workspace::currentWorkspace->saveComponents = componentsCheck->isChecked();
            Workspace::currentWorkspace->saveToolbars   = toolbarsCheck->isChecked();
            Workspace::currentWorkspace->saveMenuBar    = menubarCheck->isChecked();
            Workspace::currentWorkspace->savePrefs      = prefsCheck->isChecked();

            Workspace::currentWorkspace->save();
            preferences.setPreference(PREF_APP_WORKSPACE, Workspace::currentWorkspace->name());
            PaletteBox* paletteBox = mscore->getPaletteBox();
            paletteBox->updateWorkspaces();
            }
      else {
            // Create new workspace with name given
            // Update all flags
            if (Workspace::currentWorkspace->dirty())
                  Workspace::currentWorkspace->save();
            Workspace::currentWorkspace = Workspace::createNewWorkspace(s);
            Workspace::currentWorkspace->saveComponents = componentsCheck->isChecked();
            Workspace::currentWorkspace->saveToolbars   = toolbarsCheck->isChecked();
            Workspace::currentWorkspace->saveMenuBar    = menubarCheck->isChecked();
            Workspace::currentWorkspace->savePrefs      = prefsCheck->isChecked();

            preferences.setPreference(PREF_APP_WORKSPACE, Workspace::currentWorkspace->name());
            PaletteBox* paletteBox = mscore->getPaletteBox();
            paletteBox->updateWorkspaces();
            }
      close();
      }
}

//componentsCheck->setChecked(Workspace::currentWorkspace->saveComponents);
//toolbarsCheck->setChecked(Workspace::currentWorkspace->saveToolbars);
//menubarCheck->setChecked(Workspace::currentWorkspace->saveMenuBar);
//prefsCheck->setChecked(Workspace::currentWorkspace->savePrefs);
