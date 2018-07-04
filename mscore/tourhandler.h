#ifndef TOURHANDLER_H
#define TOURHANDLER_H
//#define Tour QList<QString>

#include "libmscore/xml.h"

namespace Ms {

struct TourMessage {
  QString message;
  QString widgetName;
  void init(QString m, QString w) { message = m; widgetName = w; }
};

class Tour
      {
      QList<TourMessage> messages;
      QMap<QString, QWidget*> nameToWidget;
      QString tourName;
      bool completed = false;

   public:
      Tour(QString name)        { tourName = name; }

      void addMessage(QString m, QString w) { TourMessage message;
                                              message.init(m, w);
                                              messages.append(message); }
      QList<TourMessage> getMessages()      { return messages;          }

      QWidget* getWidget(QString n)                { return nameToWidget.value(n); }
      void addNameAndWidget(QString n, QWidget* w) { nameToWidget.insert(n, w);    }

      void setTourName(QString n)   { tourName = n;     }
      QString getTourName()         { return tourName;  }

      void setCompleted(bool c)     { completed = c;    }
      bool isCompleted()            { return completed; }
      };

class TourHandler : public QObject
      {
      Q_OBJECT

      QMap<QObject*, QMap<QEvent::Type, QString>*>* eventHandler;

      void loadTour(XmlReader& tourXml);

      static void displayTour(Tour* tour);
      static QHash<QString, Tour*> allTours;
//      static QHash<QString, bool> completedTours;

   public:
      TourHandler(QObject* parent);
      void loadTours();
      void readCompletedTours();
      void writeCompletedTours();

      bool eventFilter(QObject *obj, QEvent* event);
      static void startTour(QString tourName);
      void attachTour(QObject* obj, QEvent::Type eventType, QString tourName);
      };

}

#endif // TOURHANDLER_H
