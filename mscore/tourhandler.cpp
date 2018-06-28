#include "tourhandler.h"
#include "musescore.h"
#include "preferences.h"

namespace Ms {

QHash<QString, Tour> TourHandler::allTours;
QHash<QString, bool> TourHandler::completedTours;

//---------------------------------------------------------
//   TourHandler
//---------------------------------------------------------

TourHandler::TourHandler()
      : QObject(0)
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
      Tour tour;
      while (tourXml.readNextStartElement()) {
            if (tourXml.name() == "Message")
                  tour.append(tourXml.readXml());
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
      QFile completedToursFile(dataPath + "/tours/completedTours.txt");
      if (!completedToursFile.open(QIODevice::ReadOnly))
            return;

      QDataStream in(&completedToursFile);
      in >> completedTours;
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
      QFile completedToursFile(path + "/completedTours.txt");
      completedToursFile.open(QIODevice::WriteOnly);
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
      return false;
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
      if (allTours.contains(tourName) && !completedTours.contains(tourName)) {
            Tour tour = allTours.value(tourName);
            displayTour(tour);
            completedTours[tourName] = true;
            }
      else
            qDebug() << tourName << " does not have a tour.";
      }

void TourHandler::startTour(QAction *action)
      {
      qDebug() << action;
      qDebug() << sender();
      }

//---------------------------------------------------------
//   msgBox
//---------------------------------------------------------

void TourHandler::displayTour(Tour tour)
      {
      QMessageBox mbox;
      for (QString message : tour) {
            mbox.setText(message);
            mbox.exec();
            }
      }

}
