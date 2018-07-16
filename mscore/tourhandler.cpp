#include "tourhandler.h"
#include "musescore.h"
#include "preferences.h"

namespace Ms {

QHash<QString, Tour*> TourHandler::allTours;
QHash<QString, Tour*> TourHandler::shortcutToTour;
QMap<QString, QMap<QString, QString>*> TourHandler::eventNameLookup;

//---------------------------------------------------------
//   OverlayWidget
//---------------------------------------------------------

OverlayWidget::OverlayWidget(QList<QWidget*> widgetList, QWidget* parent)
      : QWidget{parent}
      {
      widgets = widgetList;
      newParent();
      }

//---------------------------------------------------------
//   newParent
//---------------------------------------------------------

void OverlayWidget::newParent()
      {
      if (!parent())
            return;
      parent()->installEventFilter(this);
      resize(qobject_cast<QWidget*>(parent())->size());
      raise();
      }

//---------------------------------------------------------
//   eventFilter
//---------------------------------------------------------

bool OverlayWidget::eventFilter(QObject* obj, QEvent* ev)
      {
      if (obj == parent()) {
            if (ev->type() == QEvent::Resize)
                  resize(static_cast<QResizeEvent*>(ev)->size());
            else if (ev->type() == QEvent::ChildAdded)
                  raise();
            }
      return QWidget::eventFilter(obj, ev);
      }

//---------------------------------------------------------
//   event
//---------------------------------------------------------

bool OverlayWidget::event(QEvent* ev)
      {
      if (ev->type() == QEvent::ParentAboutToChange) {
            if (parent()) parent()->removeEventFilter(this);
            }
      else if (ev->type() == QEvent::ParentChange)
            newParent();
      return QWidget::event(ev);
      }

//---------------------------------------------------------
//   paintEvent
//---------------------------------------------------------

void OverlayWidget::paintEvent(QPaintEvent *)
      {
      QPainterPath painterPath = QPainterPath();
      QPainter p(this);
      QWidget* parentWindow = qobject_cast<QWidget*>(parent());
      if (parentWindow)
            painterPath.addRect(parentWindow->rect());

      QPainterPath subPath = QPainterPath();
      for (QWidget* w : widgets) {
            if (w->isVisible()) {
                  // Add widget and children visible region mapped to the parentWindow
                  QRegion region = w->visibleRegion() + w->childrenRegion();
                  region.translate(w->mapTo(parentWindow, QPoint(0, 0)));
                  subPath.addRegion(region);
                  }
            }
      painterPath -= subPath;

      p.fillPath(painterPath, QColor(100, 100, 100, 128));
      }

//---------------------------------------------------------
//   TourHandler
//---------------------------------------------------------

TourHandler::TourHandler(QObject* parent)
      : QObject(parent)
      {
      eventHandler = new QMap<QObject*, QMap<QEvent::Type, QString>*>;
      }

//---------------------------------------------------------
//   loadTours
//---------------------------------------------------------

void TourHandler::loadTours()
      {
      QStringList nameFilters;
      nameFilters << "*.tour";
      QString path = mscoreGlobalShare + "tours";

      QDir dir(path);
      QStringList pl = dir.entryList(nameFilters, QDir::Files, QDir::Name);

      for (const QString& entry : pl) {
            QFile* tourFile = new QFile(path + "/" + entry);
            tourFile->open(QIODevice::ReadOnly);
            XmlReader tourXml(tourFile);
            while (tourXml.readNextStartElement()) {
                  if (tourXml.name() == "Tour")
                        loadTour(tourXml);
                  else
                        tourXml.unknown();
                  }
            }

      readCompletedTours();
      }

//---------------------------------------------------------
//   loadTours
//---------------------------------------------------------

void TourHandler::loadTour(XmlReader& tourXml)
      {
      QString tourName = tourXml.attribute("name");
      QList<QString> shortcuts;
      Tour* tour = new Tour(tourName);
      while (tourXml.readNextStartElement()) {
            if (tourXml.name() == "Message") {
                  QString text;
                  QList<QString> objectNames;
                  while (tourXml.readNextStartElement()) {
                        if (tourXml.name() == "Text") {
                              QTextDocument doc;
                              doc.setHtml(tourXml.readXml());
                              text = doc.toPlainText();
                              }
                        else if (tourXml.name() == "Widget")
                              objectNames.append(tourXml.readXml());
                        else
                              tourXml.unknown();
                        }
                  tour->addMessage(text, objectNames);
                  }
            else if (tourXml.name() == "Event") {
                  QString name = tourXml.attribute("objectName");
                  QString event = tourXml.readXml();
                  if (!eventNameLookup.contains(name))
                        eventNameLookup.insert(name, new QMap<QString, QString>);
                  eventNameLookup.value(name)->insert(event, tourName);
                  }
            else if (tourXml.name() == "Shortcut") {
                  shortcuts.append(tourXml.readXml());
                  }
            else
                  tourXml.unknown();
            }

      allTours[tourName] = tour;
      for (QString s : shortcuts)
            shortcutToTour[s] = tour;
      }

//---------------------------------------------------------
//   readCompletedTours
//---------------------------------------------------------

void TourHandler::readCompletedTours()
      {
      QFile completedToursFile(dataPath + "/tours/completedTours.list");
      if (!completedToursFile.open(QIODevice::ReadOnly))
            return;

      QDataStream in(&completedToursFile);
      QList<QString> completedTours;
      in >> completedTours;

      for (QString tourName : completedTours)
            if (allTours.contains(tourName))
                  allTours.value(tourName)->setCompleted(true);
      }

//---------------------------------------------------------
//   writeCompletedTours
//---------------------------------------------------------

void TourHandler::writeCompletedTours()
      {
      QDir dir;
      dir.mkpath(dataPath);
      QString path = dataPath + "/tours";
      dir.mkpath(path);
      QFile completedToursFile(path + "/completedTours.list");
      completedToursFile.open(QIODevice::WriteOnly);

      QList<QString> completedTours;

      for (Tour* t : allTours.values())
            if (t->completed())
                  completedTours.append(t->tourName());

      QDataStream out(&completedToursFile);
      out << completedTours;
      }

//---------------------------------------------------------
//   eventFilter
//---------------------------------------------------------

bool TourHandler::eventFilter(QObject* obj, QEvent* event)
      {
      QString eventString = QVariant::fromValue(event->type()).value<QString>();

      if (eventNameLookup.contains(obj->objectName()) &&
          eventNameLookup.value(obj->objectName())->contains(eventString))
            startTour(eventNameLookup.value(obj->objectName())->value(eventString));
      else if (eventHandler->contains(obj) && eventHandler->value(obj)->contains(event->type()))
            startTour(eventHandler->value(obj)->value(event->type()));

      return QObject::eventFilter(obj, event);
      }

//---------------------------------------------------------
//   attachTour
//---------------------------------------------------------

void TourHandler::attachTour(QObject* obj, QEvent::Type eventType, QString tourName)
      {
      obj->installEventFilter(this);
      if (!eventHandler->contains(obj))
            eventHandler->insert(obj, new QMap<QEvent::Type, QString>);
      eventHandler->value(obj)->insert(eventType, tourName);
      }

//---------------------------------------------------------
//   addWidgetToTour
//---------------------------------------------------------

void TourHandler::addWidgetToTour(QString tourName, QWidget* widget, QString widgetName)
      {
      if (allTours.contains(tourName)) {
            Tour* tour = allTours.value(tourName);
            tour->addNameAndWidget(widgetName, widget);
            }
      else
            qDebug() << tourName << "does not have a tour.";
      }

//---------------------------------------------------------
//   clearWidgetsFromTour
//---------------------------------------------------------

void TourHandler::clearWidgetsFromTour(QString tourName)
      {
      if (allTours.contains(tourName))
            allTours.value(tourName)->clearWidgets();
      else
            qDebug() << tourName << "does not have a tour.";
      }

//---------------------------------------------------------
//   startTour
//   lookup string can be a tour name or a shortcut name
//---------------------------------------------------------

void TourHandler::startTour(QString lookupString)
      {
      if (!preferences.getBool(PREF_UI_APP_STARTUP_SHOWTOURS))
            return;
      if (allTours.contains(lookupString)) {
            Tour* tour = allTours.value(lookupString);
            if (tour->completed())
                  return;
            displayTour(tour);
//            tour->setCompleted(true);
            }
      else if (shortcutToTour.contains(lookupString)) {
            Tour* tour = shortcutToTour.value(lookupString);
            if (tour->completed())
                  return;
            displayTour(tour);
//            tour->setCompleted(true);
            }
      }

//---------------------------------------------------------
//   getDisplayPoints
//---------------------------------------------------------

void TourHandler::positionMessage(QList<QWidget*> widgets, QMessageBox* mbox)
      {
      // Loads some information into the size of the mbox, a bit of a hack
      mbox->show();

      // Create a "box" to see where the msgbox should go
      bool set = false;
      QPoint topLeft;
      QPoint bottomRight;

      for (QWidget* w : widgets) {
            if (!w->isVisible())
                  continue;
            QSize size = w->frameSize();
            QPoint wTopLeft = w->mapToGlobal(QPoint(0, 0));
            QPoint wBottomRight = w->mapToGlobal(QPoint(size.width(), size.height()));

            if (!set) {
                  topLeft = wTopLeft;
                  bottomRight = wBottomRight;
                  set = true;
                  }
            else {
                  topLeft.setX(qMin(topLeft.x(), wTopLeft.x()));
                  topLeft.setY(qMin(topLeft.y(), wTopLeft.y()));
                  bottomRight.setX(qMax(bottomRight.x(), wBottomRight.x()));
                  bottomRight.setY(qMax(bottomRight.y(), wBottomRight.y()));
                  }
            }
      if (!set)
            return; // Should display in center
      QRect widgetsBox(topLeft, bottomRight);

      // Next find where the mbox goes around the widgetsBox
      QWidget* mainWindow = widgets.at(0)->window();
      int midX = (int)((float) mainWindow->frameGeometry().width() / 2.0);
      int midY = (int)((float) mainWindow->frameGeometry().height() / 2.0);

      // The longer side decides which side the mbox goes on.
      bool topBottom = (widgetsBox.height() < widgetsBox.width());

      QPoint displayPoint(0, 0);
      if (topBottom) {
            bool displayAbove = (topLeft.y() > midY);
            if (displayAbove) {
                  int mBoxHeight = mbox->size().height() + 15; // hack! Replace with custom message box
                  int y = topLeft.y();
                  displayPoint.setY(y - mBoxHeight);
                  }
            else
                  displayPoint.setY(bottomRight.y());
            float x1 = (float) widgetsBox.width() / 2.0;
            float x2 = (float) mbox->size().width() / 2.0;
            int x = (int) (x1 - x2) + widgetsBox.x();
            displayPoint.setX(x);
            }
      else {
            bool displayLeft = (topLeft.x() > midX);
            if (displayLeft) {
                  int mBoxWidth = mbox->size().width();
                  int x = topLeft.x();
                  displayPoint.setX(x - mBoxWidth);
                  }
            else
                  displayPoint.setX(bottomRight.x());
            float y1 = (float) widgetsBox.height() / 2.0;
            float y2 = (float) mbox->size().height() / 2.0;
            int y = (int) (y1 - y2) + widgetsBox.y();
            displayPoint.setY(y);
            }

      // Make sure the box is within the screen
      QRect screenGeometry = QGuiApplication::primaryScreen()->geometry();
      displayPoint.setX(qMax(displayPoint.x(), 0));
      displayPoint.setY(qMax(displayPoint.y(), 0));
      displayPoint.setX(qMin(displayPoint.x(), screenGeometry.width() - mbox->size().width()));
      displayPoint.setY(qMin(displayPoint.y(), screenGeometry.height() - mbox->size().height() - 15));

      mbox->move(displayPoint);
      }

//---------------------------------------------------------
//   displayTour
//---------------------------------------------------------

QList<QWidget*> TourHandler::getWidgetsByNames(Tour* tour, QList<QString> names)
      {
      QList<QWidget*> widgets;
      for (QString name : names) {
            // First check internal storage for widget
            if (tour->hasNameForWidget(name))
                  widgets.append(tour->getWidgetsByName(name));
            else {
                  // If not found, check all widgets by object name
                  auto foundWidgets = mscore->findChildren<QWidget*>(name);
                  widgets.append(foundWidgets);
                  }
            }
      return widgets;
      }

//---------------------------------------------------------
//   displayTour
//---------------------------------------------------------

void TourHandler::displayTour(Tour* tour)
      {
      int i = 0;
      bool next = true;
      bool showTours = true;
      QList<TourMessage> tourMessages = tour->messages();
      while (i != tourMessages.size()) {
            // Set up the message box buttons
            QMessageBox* mbox = new QMessageBox(mscore);
            QPushButton* backButton = nullptr;
            QPushButton* nextButton = nullptr;

            mbox->addButton(tr("Close"), QMessageBox::RejectRole);
            if (i != 0)
                  backButton = mbox->addButton(tr("< Back"), QMessageBox::NoRole);
            if (i != tourMessages.size() - 1)
                  nextButton = mbox->addButton(tr("Next >"), QMessageBox::YesRole);
            else
                  nextButton = mbox->addButton(tr("End"), QMessageBox::YesRole);

            // Sets default to last pressed button
            if (next)
                  mbox->setDefaultButton(nextButton);
            else
                  mbox->setDefaultButton(backButton);

            // Add text (translation?)
            mbox->setText(tourMessages[i].message);

            // Add "Do not show again" checkbox
            QCheckBox* showToursBox = new QCheckBox(tr("Do not show me tours"), mbox);
            showToursBox->setChecked(!showTours);
            mbox->setCheckBox(showToursBox);

            // Display the message box, position it if needed
            QList<QWidget*> tourWidgets = getWidgetsByNames(tour, tourMessages[i].widgetNames);
            OverlayWidget* overlay = new OverlayWidget(tourWidgets);
            if (tourWidgets.isEmpty())
                  overlay->setParent(mscore);
            else {
                  overlay->setParent(tourWidgets.at(0)->window());
                  positionMessage(tourWidgets, mbox);
                  }
            overlay->show();
            mbox->exec();
            overlay->hide();
            showTours = !(showToursBox->isChecked());

            // Handle the button presses
            if (mbox->clickedButton() == nextButton) {
                  i++;
                  next = true;
                  }
            else if (mbox->clickedButton() == backButton) {
                  i--;
                  next = false;
                  }
            else
                  break;
            }
      preferences.setPreference(PREF_UI_APP_STARTUP_SHOWTOURS, showTours);
      }

}
