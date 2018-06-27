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

bool TourHandler::eventFilter(QObject *obj, QEvent* event)
      {
      switch(event->type()) {
            case QEvent::MouseButtonPress:
                  qDebug() << obj << event;
                  return false;
            default:
                  return false;
            }
      return false;
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
