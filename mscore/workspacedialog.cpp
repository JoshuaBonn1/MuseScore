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
      retranslateUi(this);
      setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
      MuseScore::restoreGeometry(this);

      connect(buttonBox, SIGNAL(accepted()), SLOT(accepted()));
      connect(buttonBox, SIGNAL(rejected()), SLOT(close()));
      }

void WorkspaceDialog::display()
      {
      if (editMode) {
            componentsCheck->setChecked(Workspace::currentWorkspace->saveComponents);
            toolbarsCheck->setChecked(Workspace::currentWorkspace->saveToolbars);
            menubarCheck->setChecked(Workspace::currentWorkspace->saveMenuBar);
            prefsCheck->setChecked(Workspace::currentWorkspace->savePrefs);
            nameLineEdit->setText(Workspace::currentWorkspace->name());
            setWindowTitle(tr("Edit Workspace"));
            }
      else {
            componentsCheck->setChecked(false);
            toolbarsCheck->setChecked(false);
            menubarCheck->setChecked(false);
            prefsCheck->setChecked(false);
            nameLineEdit->setText("");
            setWindowTitle(tr("Create New Workspace"));
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

      if (!editMode) {
            if (Workspace::currentWorkspace->dirty())
                  Workspace::currentWorkspace->save();
            Workspace::currentWorkspace = Workspace::createNewWorkspace(s);
            }

      Workspace::currentWorkspace->saveComponents = componentsCheck->isChecked();
      Workspace::currentWorkspace->saveToolbars   = toolbarsCheck->isChecked();
      Workspace::currentWorkspace->saveMenuBar    = menubarCheck->isChecked();
      Workspace::currentWorkspace->savePrefs      = prefsCheck->isChecked();

      if (editMode) {
            // I need to delete the old file and rename it to the new name
            if (Workspace::currentWorkspace->name() != s)
                  Workspace::currentWorkspace->rename(s);
            else
                  Workspace::currentWorkspace->save();
            }

      preferences.setPreference(PREF_APP_WORKSPACE, Workspace::currentWorkspace->name());
      PaletteBox* paletteBox = mscore->getPaletteBox();
      paletteBox->updateWorkspaces();
      close();
      }
}
