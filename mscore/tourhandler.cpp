#include "tourhandler.h"
#include "musescore.h"
#include "preferences.h"

namespace Ms {

QHash<QString, Tour*> TourHandler::allTours;

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
      QString tourFileLocation = ":/data/tourList.xml";
      QFile* tourFile = new QFile(tourFileLocation);
      tourFile->open(QIODevice::ReadOnly);
      XmlReader tourXml(tourFile);

      while (tourXml.readNextStartElement()) {
            if (tourXml.name() == "TourCollection") {
                  while (tourXml.readNextStartElement()) {
                        if (tourXml.name() == "Tour")
                              loadTour(tourXml);
                        else
                              tourXml.unknown();
                        }
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
      Tour* tour = new Tour(tourName);
      while (tourXml.readNextStartElement()) {
            if (tourXml.name() == "Message")
                  tour->addMessage(tourXml.readXml(), tourXml.attribute("widget"));
            else
                  tourXml.unknown();
            }

      allTours[tourName] = tour;
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
      if (eventHandler->contains(obj) && eventHandler->value(obj)->contains(event->type()))
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
//---------------------------------------------------------

void TourHandler::startTour(QString tourName)
      {
      if (!preferences.getBool(PREF_UI_APP_STARTUP_SHOWTOURS))
            return;
      if (allTours.contains(tourName)) {
            Tour* tour = allTours.value(tourName);
            if (tour->completed())
                  return;
            displayTour(tour);
            //tour->setCompleted(true);
            }
      else
            qDebug() << tourName << " does not have a tour.";
      }

//---------------------------------------------------------
//   getDisplayPoints
//---------------------------------------------------------

void TourHandler::positionMessage(QList<QWidget*> widgets, QMessageBox* mbox)
      {
      // Loads some information into the size of the mbox, a bit of a hack
      mbox->show();
      // Create a "box" to see where the msgbox should go
      QSize s = widgets.at(0)->frameSize();
      QPoint topLeft = widgets.at(0)->mapToGlobal(QPoint(0, 0));
      QPoint bottomRight = widgets.at(0)->mapToGlobal(QPoint(s.width(), s.height()));
      for (QWidget* w : widgets) {
            QSize size = w->frameSize();
            QPoint wTopLeft = w->mapToGlobal(QPoint(0, 0));
            QPoint wBottomRight = w->mapToGlobal(QPoint(size.width(), size.height()));

            topLeft.setX(qMin(topLeft.x(), wTopLeft.x()));
            topLeft.setY(qMin(topLeft.y(), wTopLeft.y()));
            bottomRight.setX(qMax(bottomRight.x(), wBottomRight.x()));
            bottomRight.setY(qMax(bottomRight.y(), wBottomRight.y()));
            }
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

      mbox->move(displayPoint);
      }

//---------------------------------------------------------
//   displayTour
//---------------------------------------------------------

void TourHandler::displayTour(Tour* tour)
      {
//      QMessageBox mbox(mscore);
      for (TourMessage tm : tour->messages()) {
            QMessageBox* mbox = new QMessageBox(mscore);
            // Save the original styles
            QList<QString> originalStyles;
            // Used for message box placement
            QWidget* widgetWindow = nullptr;
            for (QWidget* w : tour->getWidgetsByName(tm.widgetName)) {
                  widgetWindow = w->window();
                  originalStyles.append(w->styleSheet());
                  QString windowColor = QApplication::palette().color(QPalette::Window).name();
                  QString highlightColor = QApplication::palette().color(QPalette::Highlight).name();
//                  w->setStyleSheet(w->styleSheet() + "background: qradialgradient(cx:0.5, cy:0.5, radius: 0.5,"
//                                   "fx:0.5, fy:0.5, stop:0 " + highlightColor + ", stop:1 " + windowColor + ");");
                  }
            mbox->setText(tm.message);

            if (!widgetWindow) {
                  mbox->exec();
                  }
            else {
                  positionMessage(tour->getWidgetsByName(tm.widgetName), mbox);
//                  QPoint displayPoint = getDisplayPoint(tour->getWidgetsByName(tm.widgetName), mbox);
//                  mbox->move(displayPoint);
//                  mbox->move(QPoint(200, 200));
                  mbox->exec();
                  }

            // Load the original styles
            for (QWidget* w : tour->getWidgetsByName(tm.widgetName))
                  w->setStyleSheet(originalStyles.takeFirst());
            }
      }

}
