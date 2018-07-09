#ifndef TOURHANDLER_H
#define TOURHANDLER_H

#include "libmscore/xml.h"

namespace Ms {

struct TourMessage {
  QString message;
  QString widgetName;
  void init(QString m, QString w) { message = m; widgetName = w; }
};

class Tour
      {
      QList<TourMessage> _messages;
      QMultiMap<QString, QWidget*> nameToWidget;
      QString _tourName;
      bool _completed = false;

   public:
      Tour(QString name) { _tourName = name; }

      void addMessage(QString m, QString w) { TourMessage message;
                                              message.init(m, w);
                                              _messages.append(message); }
      QList<TourMessage> messages()         { return _messages;          }

      QList<QWidget*> getWidgetsByName(QString n)  { return nameToWidget.values(n); }
      void addNameAndWidget(QString n, QWidget* w) { nameToWidget.insert(n, w);    }
      void clearWidgets()                          { nameToWidget.clear();         }

      void setTourName(QString n)   { _tourName = n;     }
      QString tourName()            { return _tourName;  }

      void setCompleted(bool c)     { _completed = c;    }
      bool completed()              { return _completed; }
      };

class TourHandler : public QObject
      {
      Q_OBJECT

      QMap<QObject*, QMap<QEvent::Type, QString>*>* eventHandler;

      void loadTour(XmlReader& tourXml);

      static void displayTour(Tour* tour);
      static void positionMessage(QList<QWidget*> widgets, QMessageBox* mbox);
      static QHash<QString, Tour*> allTours;

   public:
      TourHandler(QObject* parent);
      void loadTours();
      void readCompletedTours();
      void writeCompletedTours();

      bool eventFilter(QObject *obj, QEvent* event);
      static void startTour(QString tourName);
      void attachTour(QObject* obj, QEvent::Type eventType, QString tourName);

      static void addWidgetToTour(QString tourName, QWidget* widget, QString widgetName);
      static void clearWidgetsFromTour(QString tourName);
      };

}

#endif // TOURHANDLER_H
