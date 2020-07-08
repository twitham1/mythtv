#ifndef SOURCEMANAGER_H
#define SOURCEMANAGER_H

// QT headers
#include <QDir>
#include <QObject>
#include <QMultiMap>
#include <QStringList>

// MythWeather headers
#include "weatherUtils.h"
#include "weatherSource.h"

class WeatherScreen;
class ScriptInfo;
using SourceMap = QMultiMap<long, const WeatherSource*>;

class SourceManager : public QObject
{
    Q_OBJECT

  public:
    SourceManager();
    ~SourceManager() override;
    WeatherSource *needSourceFor(int id, const QString &loc, units_t units);
    QStringList getLocationList(ScriptInfo *si, const QString &str);
    void startTimers();
    void stopTimers();
    void doUpdate(bool forceUpdate = false);
    bool findPossibleSources(QStringList types, QList<ScriptInfo *> &sources);
    void clearSources();
    bool findScripts();
    bool findScriptsDB();
    void setupSources();
    bool connectScreen(uint id, WeatherScreen *screen);
    bool disconnectScreen(WeatherScreen *screen);
    ScriptInfo *getSourceByName(const QString &name);

  private slots:
    void timeout(void) {}

  private:
    QList<ScriptInfo *>      m_scripts;  //all scripts
    QList<WeatherSource *>   m_sources;  //in-use scripts
    SourceMap m_sourcemap;
    void recurseDirs(QDir dir);
};

#endif /* SOURCEMANAGER_H */
