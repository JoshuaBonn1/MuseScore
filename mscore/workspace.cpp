//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//
//  Copyright (C) 2011 Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#include "workspace.h"
#include "musescore.h"
#include "libmscore/score.h"
#include "libmscore/imageStore.h"
#include "libmscore/xml.h"
#include "thirdparty/qzip/qzipreader_p.h"
#include "thirdparty/qzip/qzipwriter_p.h"
#include "preferences.h"
#include "palette.h"
#include "palettebox.h"

namespace Ms {

bool Workspace::workspacesRead = false;
Workspace* Workspace::currentWorkspace;

Workspace Workspace::_advancedWorkspace {
      QT_TR_NOOP("Advanced"), QString("Advanced"), false, true
      };

Workspace Workspace::_basicWorkspace {
      QT_TR_NOOP("Basic"), QString("Basic"), false, true
      };

QList<Workspace*> Workspace::_workspaces {
      &_basicWorkspace,
      &_advancedWorkspace
      };

QList<QPair<QAction*, QString>> Workspace::actionToStringList {};

QList<QPair<QMenu*, QString>> Workspace::menuToStringList {};

//---------------------------------------------------------
//   undoWorkspace
//---------------------------------------------------------

void MuseScore::undoWorkspace()
      {
      Workspace::currentWorkspace->read();
      Workspace::currentWorkspace->setDirty(false);
      }

//---------------------------------------------------------
//   showWorkspaceMenu
//---------------------------------------------------------

void MuseScore::showWorkspaceMenu()
      {
      if (workspaces == 0) {
            workspaces = new QActionGroup(this);
            workspaces->setExclusive(true);
            connect(workspaces, SIGNAL(triggered(QAction*)), SLOT(changeWorkspace(QAction*)));
            }
      else {
            for (QAction* a : workspaces->actions())
                  workspaces->removeAction(a);
            }
      menuWorkspaces->clear();

      const QList<Workspace*> pl = Workspace::workspaces();
      for (Workspace* p : pl) {
            QAction* a = workspaces->addAction(qApp->translate("Ms::Workspace", p->name().toUtf8()));
            a->setCheckable(true);
            a->setData(p->path());
            a->setChecked(p->name() == preferences.getString(PREF_APP_WORKSPACE));
            menuWorkspaces->addAction(a);
            }

      menuWorkspaces->addSeparator();
      QAction* a = new QAction(tr("New..."), this);
      connect(a, SIGNAL(triggered()), SLOT(createNewWorkspace()));
      menuWorkspaces->addAction(a);

      a = new QAction(tr("Delete"), this);
      a->setDisabled(Workspace::currentWorkspace->readOnly());
      connect(a, SIGNAL(triggered()), SLOT(deleteWorkspace()));
      menuWorkspaces->addAction(a);

      a = new QAction(tr("Undo Changes"), this);
      a->setDisabled(Workspace::currentWorkspace->readOnly());
      connect(a, SIGNAL(triggered()), SLOT(undoWorkspace()));
      menuWorkspaces->addAction(a);
      }

//---------------------------------------------------------
//   createNewWorkspace
//---------------------------------------------------------

void MuseScore::createNewWorkspace()
      {
      QString s = QInputDialog::getText(this, tr("Read Workspace Name"),
         tr("Workspace name:"));
      if (s.isEmpty())
            return;
      s = s.replace( QRegExp( "[" + QRegExp::escape( "\\/:*?\"<>|" ) + "]" ), "_" ); //FAT/NTFS special chars
      for (;;) {
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
      if (Workspace::currentWorkspace->dirty())
            Workspace::currentWorkspace->save();
      Workspace::currentWorkspace = Workspace::createNewWorkspace(s);
      preferences.setPreference(PREF_APP_WORKSPACE, Workspace::currentWorkspace->name());
      PaletteBox* paletteBox = mscore->getPaletteBox();
      paletteBox->updateWorkspaces();
      }

//---------------------------------------------------------
//   deleteWorkspace
//---------------------------------------------------------

void MuseScore::deleteWorkspace()
      {
      if (!workspaces)
            return;
      QAction* a = workspaces->checkedAction();
      if (!a)
            return;
      Workspace* workspace = 0;
      for (Workspace* p : Workspace::workspaces()) {
            if (p->name() == a->text()) { // no need for qApp->translate since "Basic" and "Advanced" are not deletable
                  workspace = p;
                  break;
                  }
            }
      if (!workspace)
            return;

      QMessageBox::StandardButton reply;
      reply = QMessageBox::question(0,
                 QWidget::tr("Are you sure?"),
                 QWidget::tr("Do you really want to delete the '%1' workspace?").arg(workspace->name()),
                 QMessageBox::Yes | QMessageBox::No,
                 QMessageBox::Yes
                 );
      if (reply != QMessageBox::Yes)
            return;

      Workspace::workspaces().removeOne(workspace);
      QFile f(workspace->path());
      f.remove();
      delete workspace;
      PaletteBox* paletteBox = mscore->getPaletteBox();
      paletteBox->clear();
      Workspace::currentWorkspace = Workspace::workspaces().first();
      preferences.setPreference(PREF_APP_WORKSPACE, Workspace::currentWorkspace->name());
      changeWorkspace(Workspace::currentWorkspace);
      paletteBox = mscore->getPaletteBox();
      paletteBox->updateWorkspaces();
      }

//---------------------------------------------------------
//   changeWorkspace
//---------------------------------------------------------

void MuseScore::changeWorkspace(QAction* a)
      {
      for (Workspace* p :Workspace::workspaces()) {
            if (qApp->translate("Ms::Workspace", p->name().toUtf8()) == a->text()) {
                  changeWorkspace(p);
                  preferences.setPreference(PREF_APP_WORKSPACE, Workspace::currentWorkspace->name());
                  PaletteBox* paletteBox = mscore->getPaletteBox();
                  paletteBox->updateWorkspaces();
                  return;
                  }
            }
      qDebug("   workspace \"%s\" not found", qPrintable(a->text()));
      }

//---------------------------------------------------------
//   changeWorkspace
//---------------------------------------------------------

void MuseScore::changeWorkspace(Workspace* p)
      {
      Workspace::currentWorkspace->save();
      p->read();
      Workspace::currentWorkspace = p;
      preferencesChanged();
      }

//---------------------------------------------------------
//   initWorkspace
//---------------------------------------------------------

void Workspace::initWorkspace()
      {
      for (Workspace* p : Workspace::workspaces()) {
            if (p->name() == preferences.getString(PREF_APP_WORKSPACE)) {
                  currentWorkspace = p;
                  break;
                  }
            }
      if (currentWorkspace == 0)
            currentWorkspace = Workspace::workspaces().at(0);
      }

//---------------------------------------------------------
//   writeFailed
//---------------------------------------------------------

static void writeFailed(const QString& _path)
      {
      QString s = qApp->translate("Workspace", "Writing Workspace File\n%1\nfailed: ");
      QMessageBox::critical(mscore, qApp->translate("Workspace", "Writing Workspace File"), s.arg(_path));
      }

//---------------------------------------------------------
//   Workspace
//---------------------------------------------------------

Workspace::Workspace()
   : QObject(0)
      {
      _dirty = false;
      _readOnly = false;
      }

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void Workspace::write()
      {
      if (_path.isEmpty()) {
            QString ext(".workspace");
            QDir dir;
            dir.mkpath(dataPath);
            _path = dataPath + "/workspaces";
            dir.mkpath(_path);
            _path += "/" + _name + ext;
            }
      MQZipWriter f(_path);
      f.setCreationPermissions(
         QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
         | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser
         | QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup
         | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther);

      if (f.status() != MQZipWriter::NoError) {
            writeFailed(_path);
            return;
            }

      QBuffer cbuf;
      cbuf.open(QIODevice::ReadWrite);
      XmlWriter xml(gscore, &cbuf);
      xml.header();
      xml.stag("container");
      xml.stag("rootfiles");
      xml.stag(QString("rootfile full-path=\"%1\"").arg(XmlWriter::xmlString("workspace.xml")));
      xml.etag();
      for (ImageStoreItem* ip : imageStore) {
            if (!ip->isUsed(gscore))
                  continue;
            QString dstPath = QString("Pictures/") + ip->hashName();
            xml.tag("file", dstPath);
            }
      xml.etag();
      xml.etag();
      cbuf.seek(0);
      f.addFile("META-INF/container.xml", cbuf.data());

      // save images
      for (ImageStoreItem* ip : imageStore) {
            if (!ip->isUsed(gscore))
                  continue;
            QString dstPath = QString("Pictures/") + ip->hashName();
            f.addFile(dstPath, ip->buffer());
            }
      {
      QBuffer cbuf;
      cbuf.open(QIODevice::ReadWrite);
      XmlWriter xml(gscore, &cbuf);
      xml.setClipboardmode(true);
      xml.header();
      xml.stag("museScore version=\"" MSC_VERSION "\"");
      xml.stag("Workspace");
      // xml.tag("name", _name);
      PaletteBox* pb = mscore->getPaletteBox();
      pb->write(xml);

      // write toolbar settings
      xml.stag("Toolbar name=\"noteInput\"");
      for (auto i : *mscore->noteInputMenuEntries())
            xml.tag("action", i);
      xml.etag();

      xml.stag("Preferences");
      for (auto pref : preferences.getWorkspaceRelevantPreferences()) {
            if (pref.second.defaultValue().isValid()) {
                  QString pref_first = QString::fromStdString(pref.first);
                  xml.tag("Preference name=\"" + pref_first + "\"", pref.second.defaultValue());
                  }
            }
      xml.etag();

      writeMenuBar(&cbuf);

      xml.etag();
      xml.etag();
      f.addFile("workspace.xml", cbuf.data());
      cbuf.close();
      }

      if (f.status() != MQZipWriter::NoError)
            writeFailed(_path);
      }

void Workspace::writeMenuBar(QBuffer* cbuf) {
      // Loop through each menu in menubar. For each menu, call writeMenu.
      XmlWriter xml(gscore, cbuf);
      xml.stag("MenuBar");
      QMenuBar* mb = mscore->menuBar();
      for (QAction* action : mb->actions()) {
            if (action->isSeparator())
                  xml.tag("action", "");
            else if (action->menu()) {
                  xml.stag("Menu name=\"" + findStringFromMenu(action->menu()) + "\"");
                  writeMenu(cbuf, action->menu());
                  xml.etag();
                  }
            else
                  xml.tag("action", findStringFromAction(action));
            }
      xml.etag();
      }

void Workspace::writeMenu(QBuffer* cbuf, QMenu* menu) {
      XmlWriter xml(gscore, cbuf);
      // Recursively save QMenu
      for (QAction* action : menu->actions()) {
            if (action->isSeparator())
                  xml.tag("action", "");
            else if (action->menu()) {
//                  QString str = Shortcut::findShortcutFromText(action->text());
                  xml.stag("Menu name=\"" + findStringFromMenu(action->menu()) + "\"");
                  writeMenu(cbuf, action->menu());
                  xml.etag();
                  }
            else {
                  xml.tag("action", findStringFromAction(action));
                  }
            }
      }

extern QString readRootFile(MQZipReader*, QList<QString>&);

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void Workspace::read()
      {
      if (_path == "Advanced") {
            mscore->setAdvancedPalette();
            for (Palette* p : mscore->getPaletteBox()->palettes())
                  p->setSystemPalette(true);
            mscore->setNoteInputMenuEntries(MuseScore::advancedNoteInputMenuEntries());
            mscore->populateNoteInputMenu();
            loadDefaultMenuBar();
            return;
            }
      if (_path == "Basic") {
            mscore->setBasicPalette();
            for (Palette* p : mscore->getPaletteBox()->palettes())
                  p->setSystemPalette(true);
            mscore->setNoteInputMenuEntries(MuseScore::basicNoteInputMenuEntries());
            mscore->populateNoteInputMenu();
            loadDefaultMenuBar();
            return;
            }
      if (_path.isEmpty() || !QFile(_path).exists()) {
            qDebug("cannot read workspace <%s>", qPrintable(_path));
            mscore->setAdvancedPalette();       // set default palette
            loadDefaultMenuBar();
            return;
            }
      QFileInfo fi(_path);
      _readOnly = !fi.isWritable();

      MQZipReader f(_path);
      QList<QString> images;
      QString rootfile = readRootFile(&f, images);
      //
      // load images
      //
      for (const QString& s : images)
            imageStore.add(s, f.fileData(s));

      if (rootfile.isEmpty()) {
            qDebug("can't find rootfile in: %s", qPrintable(_path));
            return;
            }

      QByteArray ba = f.fileData(rootfile);
      XmlReader e(ba);

      while (e.readNextStartElement()) {
            if (e.name() == "museScore") {
                  while (e.readNextStartElement()) {
                        if (e.name() == "Workspace")
                              read(e);
                        else
                              e.unknown();
                        }
                  }
            }
      preferences.save();
      }

void Workspace::read(XmlReader& e)
      {
      bool niToolbar = false;
      while (e.readNextStartElement()) {
            const QStringRef& tag(e.name());
            if (tag == "name")
                  e.readElementText();
            else if (tag == "PaletteBox") {
                  PaletteBox* paletteBox = mscore->getPaletteBox();
                  paletteBox->clear();
                  paletteBox->read(e);
                  QList<Palette*> pl = paletteBox->palettes();
                  foreach (Palette* p, pl) {
                        p->setSystemPalette(_readOnly);
                        connect(paletteBox, SIGNAL(changed()), SLOT(setDirty()));
                        }
                  }
            else if (tag == "Toolbar") {
                  QString name = e.attribute("name");
                  std::list<const char*> l;
                  while (e.readNextStartElement()) {
                        const QStringRef& tag(e.name());
                        if (tag == "action") {
                              QString s = e.readElementText();
                              for (auto k : mscore->allNoteInputMenuEntries()) {
                                    if (k == s) {
                                          l.push_back(k);
                                          break;
                                          }
                                    }
                              }
                        else
                              e.unknown();
                        }
                  if (name == "noteInput") {
                        mscore->setNoteInputMenuEntries(l);
                        mscore->populateNoteInputMenu();
                        niToolbar = true;
                        }
                  }
            else if (tag == "Preferences") {
                  while (e.readNextStartElement()) {
                        QString preference_name = e.attribute("name");
                        switch (preferences.defaultValue(preference_name).type()) {
                              case QVariant::Int:
                                    {
                                    int new_int = e.readInt();
                                    preferences.setPreference(preference_name, new_int);
                                    }
                                    break;
                              case QVariant::Color:
                                    {
                                    QColor new_color = e.readColor();
                                    qDebug() << preference_name << " = " << new_color;

                                    preferences.setPreference(preference_name, new_color);
                                    }
                                    break;
                              case QVariant::String:
                                    {
                                    QString new_string = e.readXml();
                                    preferences.setPreference(preference_name, new_string);
                                    }
                                    break;
                              case QVariant::Bool:
                                    {
                                    bool new_bool = e.readBool();

                                    preferences.setPreference(preference_name, new_bool);
                                    }
                                    break;
                              default:
                                    qDebug() << preferences.defaultValue(preference_name).type() << " not handled.";
                                    e.unknown();
                              }
                        }
                  }
            else if (tag == "MenuBar") {
                  QMenuBar* mb = mscore->menuBar();
                  mb->clear();
                  while (e.readNextStartElement()) {
                        if (e.hasAttribute("name")) { // is a menu
                              QString menu_id = e.attribute("name");
                              QMenu* menu = findMenuFromString(menu_id);
                              if (menu) {
                                    menu->clear();
                                    mb->addMenu(menu);
                                    readMenu(e, menu);
                                    }
                              else {
                                    menu = new QMenu(menu_id);
                                    mb->addMenu(menu);
                                    readMenu(e, menu);
                                    }
                              }
                        else { // is an action
                              QString action_id = e.readXml();
                              if (action_id.isEmpty())
                                    mb->addSeparator();
                              else {
                                    QAction* action = findActionFromString(action_id);
                                    mb->addAction(action);
                                    }
                              }
                        }
                  }
            else
                  e.unknown();
            }
      if (!niToolbar) {
            mscore->setNoteInputMenuEntries(mscore->allNoteInputMenuEntries());
            mscore->populateNoteInputMenu();
            }
      }

//---------------------------------------------------------
//   readMenu
//---------------------------------------------------------

void Workspace::readMenu(XmlReader& e, QMenu* menu)
      {
      while (e.readNextStartElement()) {
            if (e.hasAttribute("name")) { // is a menu
                  QString menu_id = e.attribute("name");
                  QMenu* new_menu = findMenuFromString(menu_id);
                  if (new_menu) {
                        new_menu->clear();
                        menu->addMenu(new_menu);
                        readMenu(e, new_menu);
                        }
                  else {
                        new_menu = new QMenu(menu_id);
                        menu->addMenu(new_menu);
                        readMenu(e, new_menu);
                        }
                  }
            else { // is an action
                  QString action_id = e.readXml();
                  if (action_id.isEmpty())
                        menu->addSeparator();
                  else {
                        QAction* action = findActionFromString(action_id);
                        menu->addAction(action);
                        }
                  }
            }
      }

//---------------------------------------------------------
//   save
//---------------------------------------------------------

void Workspace::save()
      {
      if (_readOnly)
            return;
      PaletteBox* pb = mscore->getPaletteBox();
      if (pb)
            write();
      }

//---------------------------------------------------------
//   workspaces
//---------------------------------------------------------

QList<Workspace*>& Workspace::workspaces()
      {
      if (!workspacesRead) {
            QStringList path;
            path << mscoreGlobalShare + "workspaces";
            path << dataPath + "/workspaces";
            QStringList nameFilters;
            nameFilters << "*.workspace";

            for (const QString& s : path) {
                  QDir dir(s);
                  QStringList pl = dir.entryList(nameFilters, QDir::Files, QDir::Name);

                  foreach (const QString& entry, pl) {
                        Workspace* p = 0;
                        QFileInfo fi(s + "/" + entry);
                        QString name(fi.completeBaseName());
                        for (Workspace* w : _workspaces) {
                              if (w->name() == name) {
                                    p = w;
                                    break;
                                    }
                              }
                        if (!p)
                              p = new Workspace;
                        p->setPath(s + "/" + entry);
                        p->setName(name);
                        p->setReadOnly(!fi.isWritable());
                        _workspaces.append(p);
                        }
                  }
            workspacesRead = true;
            }
      return _workspaces;
      }

//---------------------------------------------------------
//   createNewWorkspace
//---------------------------------------------------------

Workspace* Workspace::createNewWorkspace(const QString& name)
      {
      Workspace* p = new Workspace;
      p->setName(name);
      p->setPath("");
      p->setDirty(false);
      p->setReadOnly(false);
      p->write();

      // all palettes in new workspace are editable

      PaletteBox* paletteBox = mscore->getPaletteBox();
      QList<Palette*> pl = paletteBox->palettes();
      for (Palette* p : pl) {
            p->setSystemPalette(false);
            for (int i = 0; i < p->size(); ++i)
                  p->setCellReadOnly(i, false);
            }

      _workspaces.append(p);
      return p;
      }

//---------------------------------------------------------
//   addActionAndString
//---------------------------------------------------------

void Workspace::addActionAndString(QAction* action, QString string)
      {
      QPair<QAction*, QString> pair;
      pair.first = action;
      pair.second = string;
      actionToStringList.append(pair);
      }

//---------------------------------------------------------
//   addRemainingFromMenuBar
//---------------------------------------------------------

void Workspace::addRemainingFromMenuBar(QMenuBar* mb)
      {
      // Loop through each menu in menubar. For each menu, call writeMenu.
      for (QAction* action : mb->actions()) {
            if (action->isSeparator())
                  continue;
            else if (action->menu())
                  addRemainingFromMenu(action->menu());
            else if (!action->data().toString().isEmpty())
                  addActionAndString(action, action->data().toString());
            }
      qDebug() << "Done";
      }

//---------------------------------------------------------
//   addRemainingFromMenu
//---------------------------------------------------------

void Workspace::addRemainingFromMenu(QMenu* menu)
      {
      // Recursively save QMenu
      for (QAction* action : menu->actions()) {
            if (action->isSeparator())
                  continue;
            else if (action->menu())
                  addRemainingFromMenu(action->menu());
            else if (!action->data().toString().isEmpty())
                  addActionAndString(action, action->data().toString());
            }
      }

//---------------------------------------------------------
//   findActionFromString
//---------------------------------------------------------

QAction* Workspace::findActionFromString(QString string)
      {
      for (auto pair : actionToStringList) {
            if (pair.second == string)
                  return pair.first;
            }
      return 0;
      }

//---------------------------------------------------------
//   findStringFromAction
//---------------------------------------------------------

QString Workspace::findStringFromAction(QAction* action)
      {
      for (auto pair : actionToStringList) {
            if (pair.first == action)
                  return pair.second;
            }
      return 0;
      }

//---------------------------------------------------------
//   addMenuAndString
//---------------------------------------------------------

void Workspace::addMenuAndString(QMenu* menu, QString string)
      {
      QPair<QMenu*, QString> pair;
      pair.first = menu;
      pair.second = string;
      menuToStringList.append(pair);
      }

//---------------------------------------------------------
//   findMenuFromString
//---------------------------------------------------------

QMenu* Workspace::findMenuFromString(QString string)
      {
      for (auto pair : menuToStringList) {
            if (pair.second == string)
                  return pair.first;
            }
      return 0;
      }

//---------------------------------------------------------
//   findStringFromMenu
//---------------------------------------------------------

QString Workspace::findStringFromMenu(QMenu* menu)
      {
      for (auto pair : menuToStringList) {
            if (pair.first == menu)
                  return pair.second;
            }
      return 0;
      }

//---------------------------------------------------------
//   loadDefaultMenuBar
//---------------------------------------------------------

void Workspace::loadDefaultMenuBar() {
      QMenuBar* mb = mscore->menuBar();
      mb->clear();

      //---------------------
      //    Menu File
      //---------------------

      QMenu* menu = findMenuFromString("menu-file");
      menu->clear();
      mb->addMenu(menu);

      menu->addAction(findActionFromString("startcenter"));
      menu->addAction(findActionFromString("file-new"));
      menu->addAction(findActionFromString("file-open"));

      menu->addMenu(findMenuFromString("menu-open-recent"));

      menu->addSeparator();
      menu->addAction(findActionFromString("file-save"));
      menu->addAction(findActionFromString("file-save-as"));
      menu->addAction(findActionFromString("file-save-a-copy"));
      menu->addAction(findActionFromString("file-save-selection"));
      if (enableExperimental)
            menu->addAction(findActionFromString("file-save-online"));
      menu->addAction(findActionFromString("file-export"));
      menu->addAction(findActionFromString("file-part-export"));
      menu->addAction(findActionFromString("file-import-pdf"));
      menu->addSeparator();
      menu->addAction(findActionFromString("file-close"));
      menu->addSeparator();
      menu->addAction(findActionFromString("parts"));
      menu->addAction(findActionFromString("album"));
      if (enableExperimental)
            menu->addAction(findActionFromString("layer"));
      menu->addSeparator();
      menu->addAction(findActionFromString("edit-info"));
      if (enableExperimental)
            menu->addAction(findActionFromString("media"));
      menu->addSeparator();
      menu->addAction(findActionFromString("print"));
#ifndef Q_OS_MAC
      menu->addSeparator();
      menu->addAction(findActionFromString("quit"));
#endif

      //---------------------
      //    Menu Edit
      //---------------------

      menu = findMenuFromString("menu-edit");
      menu->clear();
      mb->addMenu(menu);

      menu->addAction(findActionFromString("undo"));
      menu->addAction(findActionFromString("redo"));
      menu->addSeparator();
      menu->addAction(findActionFromString("cut"));
      menu->addAction(findActionFromString("copy"));
      menu->addAction(findActionFromString("paste"));
      menu->addAction(findActionFromString("swap"));
      menu->addAction(findActionFromString("delete"));
      menu->addSeparator();
      menu->addAction(findActionFromString("select-all"));
      menu->addAction(findActionFromString("select-section"));
      menu->addAction(findActionFromString("find"));
      menu->addSeparator();
      menu->addAction(findActionFromString("instruments"));

#ifdef NDEBUG
      if (enableExperimental) {
#endif
            menu->addSeparator();
            menu->addAction(findActionFromString("debugger"));
#ifdef NDEBUG
            }
#endif

      menu->addSeparator();
      menu->addAction(findActionFromString("preference-dialog"));

      //---------------------
      //    Menu View
      //---------------------

      menu = findMenuFromString("menu-view");
      menu->clear();
      mb->addMenu(menu);

      menu->addAction(findActionFromString("toggle-palette"));
      menu->addAction(findActionFromString("masterpalette"));
      menu->addAction(findActionFromString("inspector"));

#ifdef OMR
      menu->addAction(findActionFromString("omr"));
#endif

      menu->addAction(findActionFromString("toggle-playpanel"));
      menu->addAction(findActionFromString("toggle-navigator"));
      menu->addAction(findActionFromString("toggle-timeline"));
      menu->addAction(findActionFromString("toggle-mixer"));
      menu->addAction(findActionFromString("synth-control"));
      menu->addAction(findActionFromString("toggle-selection-window"));
      menu->addAction(findActionFromString("toggle-piano"));
      menu->addSeparator();
      menu->addAction(findActionFromString("zoomin"));
      menu->addAction(findActionFromString("zoomout"));
      menu->addSeparator();

      menu = findMenuFromString("menu-toolbars");

      menu->addAction(findActionFromString("toggle-fileoperations"));
      menu->addAction(findActionFromString("toggle-transport"));
      menu->addAction(findActionFromString("toggle-concertpitch"));
      menu->addAction(findActionFromString("toggle-imagecapture"));
      menu->addAction(findActionFromString("toggle-noteinput"));
      menu->addSeparator();
      menu->addAction(findActionFromString("edit-toolbars"));

      menu = findMenuFromString("menu-view");
      menu->addMenu(findMenuFromString("menu-toolbars"));
      menu->addMenu(findMenuFromString("menu-workspaces"));

      menu->addAction(findActionFromString("toggle-statusbar"));
      menu->addSeparator();
      menu->addAction(findActionFromString("split-h"));
      menu->addAction(findActionFromString("split-v"));
      menu->addSeparator();
      menu->addAction(findActionFromString("show-invisible"));
      menu->addAction(findActionFromString("show-unprintable"));
      menu->addAction(findActionFromString("show-frames"));
      menu->addAction(findActionFromString("show-pageborders"));
      menu->addAction(findActionFromString("mark-irregular"));
      menu->addSeparator();

#ifndef Q_OS_MAC
      menu->addAction(findActionFromString("fullscreen"));
#endif

      //---------------------
      //    Menu Add
      //---------------------

      menu = findMenuFromString("menu-add");
      menu->clear();
      mb->addMenu(menu);

      menu->addMenu(findMenuFromString("menu-add-pitch"));
      menu->addMenu(findMenuFromString("menu-add-interval"));
      menu->addMenu(findMenuFromString("menu-tuplet"));
      menu->addSeparator();
      menu->addMenu(findMenuFromString("menu-add-measures"));
      menu->addMenu(findMenuFromString("menu-add-frames"));
      menu->addMenu(findMenuFromString("menu-add-text"));
      menu->addMenu(findMenuFromString("menu-add-lines"));

      menu = findMenuFromString("menu-add-pitch");
      menu->clear();
      menu->addAction(findActionFromString("note-input"));
      menu->addSeparator();
      for (int i = 0; i < 7; ++i) {
            char buffer[8];
            sprintf(buffer, "note-%c", "cdefgab"[i]);
            menu->addAction(findActionFromString(buffer));
            }
      menu->addSeparator();
      for (int i = 0; i < 7; ++i) {
            char buffer[8];
            sprintf(buffer, "chord-%c", "cdefgab"[i]);
            menu->addAction(findActionFromString(buffer));
            }

      menu = findMenuFromString("menu-add-interval");
      menu->clear();
      for (int i = 1; i < 10; ++i) {
            char buffer[16];
            sprintf(buffer, "interval%d", i);
            menu->addAction(findActionFromString(buffer));
            }
      menu->addSeparator();
      for (int i = 2; i < 10; ++i) {
            char buffer[16];
            sprintf(buffer, "interval-%d", i);
            menu->addAction(findActionFromString(buffer));
            }

      menu = findMenuFromString("menu-tuplet");
      menu->clear();
      for (auto i : { "duplet", "triplet", "quadruplet", "quintuplet", "sextuplet",
            "septuplet", "octuplet", "nonuplet", "tuplet-dialog" })
            menu->addAction(findActionFromString(i));

      menu = findMenuFromString("menu-add-measures");
      menu->clear();
      menu->addAction(findActionFromString("insert-measure"));
      menu->addAction(findActionFromString("insert-measures"));
      menu->addSeparator();
      menu->addAction(findActionFromString("append-measure"));
      menu->addAction(findActionFromString("append-measures"));

      menu = findMenuFromString("menu-add-frames");
      menu->clear();
      menu->addAction(findActionFromString("insert-hbox"));
      menu->addAction(findActionFromString("insert-vbox"));
      menu->addAction(findActionFromString("insert-textframe"));
      if (enableExperimental)
            menu->addAction(findActionFromString("insert-fretframe"));
      menu->addSeparator();
      menu->addAction(findActionFromString("append-hbox"));
      menu->addAction(findActionFromString("append-vbox"));
      menu->addAction(findActionFromString("append-textframe"));

      menu = findMenuFromString("menu-add-text");
      menu->clear();
      menu->addAction(findActionFromString("title-text"));
      menu->addAction(findActionFromString("subtitle-text"));
      menu->addAction(findActionFromString("composer-text"));
      menu->addAction(findActionFromString("poet-text"));
      menu->addAction(findActionFromString("part-text"));
      menu->addSeparator();
      menu->addAction(findActionFromString("system-text"));
      menu->addAction(findActionFromString("staff-text"));
      menu->addAction(findActionFromString("expression-text"));
      menu->addAction(findActionFromString("chord-text"));
      menu->addAction(findActionFromString("rehearsalmark-text"));
      menu->addAction(findActionFromString("instrument-change-text"));
      menu->addAction(findActionFromString("fingering-text"));
      menu->addSeparator();
      menu->addAction(findActionFromString("lyrics"));
      menu->addAction(findActionFromString("figured-bass"));
      menu->addAction(findActionFromString("tempo"));

      menu = findMenuFromString("menu-add-lines");
      menu->clear();
      menu->addAction(findActionFromString("add-slur"));
      menu->addAction(findActionFromString("add-hairpin"));
      menu->addAction(findActionFromString("add-hairpin-reverse"));
      menu->addAction(findActionFromString("add-8va"));
      menu->addAction(findActionFromString("add-8vb"));
      menu->addAction(findActionFromString("add-noteline"));

      //---------------------
      //    Menu Format
      //---------------------

      menu = findMenuFromString("menu-format");
      menu->clear();
      mb->addMenu(menu);

      menu->addAction(findActionFromString("edit-style"));
      menu->addAction(findActionFromString("page-settings"));
      menu->addSeparator();

      menu->addAction(findActionFromString("add-remove-breaks"));

      QMenu* subMenu = findMenuFromString("menu-stretch");
      subMenu->clear();
      for (auto i : { "stretch+", "stretch-", "reset-stretch" })
            subMenu->addAction(findActionFromString(i));
      menu->addMenu(subMenu);
      menu->addSeparator();

      menu->addAction(findActionFromString("reset-beammode"));
      menu->addAction(findActionFromString("reset"));
      menu->addSeparator();

      if (enableExperimental)
            menu->addAction(findActionFromString("edit-harmony"));

      menu->addSeparator();
      menu->addAction(findActionFromString("load-style"));
      menu->addAction(findActionFromString("save-style"));

      //---------------------
      //    Menu Tools
      //---------------------

      menu = findMenuFromString("menu-tools");
      menu->clear();
      mb->addMenu(menu);

      menu->addAction(findActionFromString("transpose"));
      menu->addSeparator();
      menu->addAction(findActionFromString("explode"));
      menu->addAction(findActionFromString("implode"));

      subMenu = findMenuFromString("menu-voices");
      subMenu->clear();
      for (auto i : { "voice-x12", "voice-x13", "voice-x14", "voice-x23", "voice-x24", "voice-x34" })
            subMenu->addAction(findActionFromString(i));
      menu->addMenu(subMenu);
      menu->addSeparator();

      menu->addAction(findActionFromString("slash-fill"));
      menu->addAction(findActionFromString("slash-rhythm"));
      menu->addSeparator();

      menu->addAction(findActionFromString("pitch-spell"));
      menu->addAction(findActionFromString("reset-groupings"));
      menu->addAction(findActionFromString("resequence-rehearsal-marks"));
      menu->addSeparator();

      menu->addAction(findActionFromString("copy-lyrics-to-clipboard"));
      menu->addAction(findActionFromString("fotomode"));

      menu->addAction(findActionFromString("del-empty-measures"));

      //---------------------
      //    Menu Plugins
      //---------------------

      menu = findMenuFromString("menu-plugins");
      menu->clear();
      mb->addMenu(menu);

      menu->addAction(findActionFromString("plugin-manager"));
      menu->addAction(findActionFromString("plugin-creator"));
      menu->addSeparator();

      //---------------------
      //    Menu Debug
      //---------------------

#ifndef NDEBUG
      menu = findMenuFromString("menu-debug");
      menu->clear();
      mb->addMenu(menu);

      menu->addAction(findActionFromString("no-horizontal-stretch"));
      menu->addAction(findActionFromString("no-vertical-stretch"));
      menu->addSeparator();
      menu->addAction(findActionFromString("show-segment-shapes"));
      menu->addAction(findActionFromString("show-measure-shapes"));
      menu->addAction(findActionFromString("show-bounding-rect"));
      menu->addAction(findActionFromString("show-corrupted-measures"));
      menu->addAction(findActionFromString("relayout"));
      menu->addAction(findActionFromString("autoplace-slurs"));
#endif

      //---------------------
      //    Menu Help
      //---------------------

      mb->addSeparator();
      menu = findMenuFromString("menu-help");
      menu->clear();
      mb->addMenu(menu);

#if 0
      if (_helpEngine) {
            HelpQuery* hw = new HelpQuery(menu);
            menu->addAction(hw);
            connect(menu, SIGNAL(aboutToShow()), hw, SLOT(setFocus()));
            }
#endif

      menu->addAction(findActionFromString("online-handbook"));
      menu->addSeparator();
      menu->addAction(findActionFromString("about"));
      menu->addAction(findActionFromString("about-qt"));
      menu->addAction(findActionFromString("about-musicxml"));

#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
#if not defined(FOR_WINSTORE)
      menu->addAction(findActionFromString("check-update"));
#endif
#endif
      menu->addSeparator();
      menu->addAction(findActionFromString("ask-help"));
      menu->addAction(findActionFromString("report-bug"));
      menu->addSeparator();
      menu->addAction(findActionFromString("resource-manager"));
      menu->addSeparator();
      menu->addAction(findActionFromString("revert-factory"));
      }

}

