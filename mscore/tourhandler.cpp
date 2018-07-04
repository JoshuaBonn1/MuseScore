#include "tourhandler.h"
#include "musescore.h"
#include "preferences.h"

namespace Ms {

QHash<QString, Tour*> TourHandler::allTours;
//QHash<QString, bool> TourHandler::completedTours;

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
            if (t->isCompleted())
                  completedTours.append(t->getTourName());

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
//   startTour
//---------------------------------------------------------

void TourHandler::startTour(QString tourName)
      {
      if (!preferences.getBool(PREF_UI_APP_STARTUP_SHOWTOURS))
            return;
      if (allTours.contains(tourName)) {
            Tour* tour = allTours.value(tourName);
            if (tour->isCompleted())
                  return;
            displayTour(tour);
            tour->setCompleted(true);
            }
      else
            qDebug() << tourName << " does not have a tour.";
      }

//---------------------------------------------------------
//   msgBox
//---------------------------------------------------------

void TourHandler::displayTour(Tour* tour)
      {
      QMessageBox mbox;
      for (TourMessage tm : tour->getMessages()) {
            mbox.setText(tm.message);
            mbox.exec();
            }
      }

}
