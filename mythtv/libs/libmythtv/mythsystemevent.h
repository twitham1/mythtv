#ifndef MYTH_SYSTEM_EVENT_H_
#define MYTH_SYSTEM_EVENT_H_

#include <QEvent>
#include <QObject>

#include "programinfo.h"
#include "recordinginfo.h"
#include "rawsettingseditor.h"

// Helper commands for formatting and sending a MythSystemEvent
MTV_PUBLIC void SendMythSystemRecEvent(const QString &msg,
                                    const RecordingInfo *pginfo);
MTV_PUBLIC void SendMythSystemPlayEvent(const QString &msg,
                                     const ProgramInfo *pginfo);

/** \class MythSystemEventHandler
 *  \brief Handles incoming MythSystemEvent messages
 *
 *  MythSystemEventHandler handles incoming MythSystemEvent messages and runs
 *  the appropriate event handler command on the local system if one is
 *  configured.
 */
class MTV_PUBLIC MythSystemEventHandler : public QObject
{
    Q_OBJECT

  public:
    // Constructor
    MythSystemEventHandler();

    // Destructor
   ~MythSystemEventHandler();

  private:
    // Helpers for converting incoming events to command lines
    void SubstituteMatches(const QStringList &tokens, QString &command);
    QString EventNameToSetting(const QString &name);

    // Custom Event Handler
    void customEvent(QEvent *e) override; // QObject
};

/** \class MythSystemEventEditor
 *  \brief An editor for MythSystemEvent handler commands
 *
 *  This class extends RawSettingsEditor and automatically populates the
 *  settings list with the MythSystemEvent handler command settings names.
 */
class MTV_PUBLIC MythSystemEventEditor : public RawSettingsEditor
{
    Q_OBJECT

  public:
    MythSystemEventEditor(MythScreenStack *parent, const char *name = nullptr);
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
