#ifndef TOURHANDLER_H
#define TOURHANDLER_H
#define Tour QList<QString>

#include "libmscore/xml.h"

namespace Ms {

class TourHandler : public QObject
      {
      Q_OBJECT

      void loadTour(XmlReader& tourXml);

      static void displayTour(Tour tour);
      static QHash<QString, Tour> allTours;
      static QHash<QString, bool> completedTours;

   public:
      TourHandler();
      void loadTours();

      bool eventFilter(QObject *obj, QEvent* event);
      static void startTour(QString tourName);
      };

}

#endif // TOURHANDLER_H
