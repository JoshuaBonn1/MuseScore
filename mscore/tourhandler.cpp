#include "tourhandler.h"
#include "musescore.h"
#include "preferences.h"

namespace Ms {

QHash<QString, Tour*> TourHandler::allTours;
QHash<QString, Tour*> TourHandler::shortcutToTour;
QMap<QString, QMap<QString, QString>*> TourHandler::eventNameLookup;

//---------------------------------------------------------
//   TourPage
//---------------------------------------------------------

TourPage::TourPage(QWidget* parent, TourMessage tourMessage)
      : QWizardPage(parent)
      {
      setTitle(tr("Tour"));

      QLabel* label = new QLabel(tourMessage.message, this);
      label->setWordWrap(true);
      QVBoxLayout* layout = new QVBoxLayout(this);
      layout->addWidget(label);
      setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
      setLayout(layout);
      }

//---------------------------------------------------------
//   addMessage
//---------------------------------------------------------

void Tour::addMessage(QString m, QList<QString> w)
      {
      TourMessage message(m, w);
      _messages.append(message);
      addPage(new TourPage(this, message));
      }

//---------------------------------------------------------
//   updateOverlay
//---------------------------------------------------------

void Tour::updateOverlay(int newId)
      {
      if (_overlay)
            _overlay->hide();

      // Closed
      if (newId == -1)
            return;

      QList<QWidget*> newWidgets = getWidgetsByNames(_messages[newId].widgetNames);
      _overlay = new OverlayWidget(newWidgets);

      if (newWidgets.isEmpty())
            _overlay->setParent(mscore);
      else
            _overlay->setParent(newWidgets.at(0)->window());

      connect(this, SIGNAL(finished(int)), _overlay, SLOT(hide()));
      _overlay->show();
      }

//---------------------------------------------------------
//   getWidgetsByNames
//---------------------------------------------------------

QList<QWidget*> Tour::getWidgetsByNames(QList<QString> names)
      {
      QList<QWidget*> widgets;
      for (QString name : names) {
            // First check internal storage for widget
            if (hasNameForWidget(name))
                  widgets.append(getWidgetsByName(name));
            else {
                  // If not found, check all widgets by object name
                  auto foundWidgets = mscore->findChildren<QWidget*>(name);
                  widgets.append(foundWidgets);
                  }
            }
      return widgets;
      }


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

      QColor overlayColor = QApplication::palette().color(QPalette::Shadow);
      overlayColor.setAlpha(128);
      p.fillPath(painterPath, overlayColor);
      }

//---------------------------------------------------------
//   showWelcomeTour
//---------------------------------------------------------

void TourHandler::showWelcomeTour()
      {
      if (!delayedWelcomeTour)
            startTour("welcome");
      }

//---------------------------------------------------------
//   showDelayedWelcomeTour
//   delays showing the welcome tour when the user
//   attempts to open a score or create a new score
//---------------------------------------------------------

void TourHandler::showDelayedWelcomeTour()
      {
      if (delayedWelcomeTour)
            startTour("welcome");
      delayedWelcomeTour = false;
      }

//---------------------------------------------------------
//   loadTours
//---------------------------------------------------------

void TourHandler::loadTours()
      {
      QStringList nameFilters;
      nameFilters << "*.tour";
      QString path = mscoreGlobalShare + "tours";
      QDirIterator it(path, nameFilters, QDir::Files, QDirIterator::Subdirectories);

      while (it.hasNext()) {
            QFile* tourFile = new QFile(it.next());
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
//   loadTour
//---------------------------------------------------------

void TourHandler::loadTour(XmlReader& tourXml)
      {
      QString tourName = tourXml.attribute("name");
      QList<QString> shortcuts;
      Tour* tour = new Tour(tourName);
      tour->setParent(mscore);
      while (tourXml.readNextStartElement()) {
            if (tourXml.name() == "Message") {
                  QString text;
                  QList<QString> objectNames;
                  while (tourXml.readNextStartElement()) {
                        if (tourXml.name() == "Text") {
                              QTextDocument doc;
                              QString ttext = qApp->translate("TourXML", tourXml.readXml().toUtf8().data());
                              doc.setHtml(ttext);
                              text = doc.toPlainText().replace("\\n", "\n");
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
//   resetCompletedTours
//---------------------------------------------------------

void TourHandler::resetCompletedTours()
      {
      for (auto tour : allTours)
            tour->setCompleted(false);
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
      else if (eventHandler.contains(obj) && eventHandler.value(obj)->contains(event->type()))
            startTour(eventHandler.value(obj)->value(event->type()));

      return QObject::eventFilter(obj, event);
      }

//---------------------------------------------------------
//   attachTour
//---------------------------------------------------------

void TourHandler::attachTour(QObject* obj, QEvent::Type eventType, QString tourName)
      {
      obj->installEventFilter(this);
      if (!eventHandler.contains(obj))
            eventHandler.insert(obj, new QMap<QEvent::Type, QString>);
      eventHandler.value(obj)->insert(eventType, tourName);
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

      Tour* tour = nullptr;
      if (allTours.contains(lookupString))
            tour = allTours.value(lookupString);
      else if (shortcutToTour.contains(lookupString))
            tour = shortcutToTour.value(lookupString);

      if (tour) {
            if (tour->completed())
                  return;
            tour->open(); // Prefer to show it from here, not a custom function
            tour->setCompleted(true);
            }
      }

}
