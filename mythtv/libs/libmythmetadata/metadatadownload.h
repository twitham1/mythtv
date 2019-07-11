#ifndef METADATADOWNLOAD_H
#define METADATADOWNLOAD_H

#include <QStringList>
#include <QMutex>
#include <QEvent>

#include "metadatacommon.h"
#include "mthread.h"

class META_PUBLIC MetadataLookupEvent : public QEvent
{
  public:
    explicit MetadataLookupEvent(MetadataLookupList lul) : QEvent(kEventType),
                                            m_lookupList(lul) {}
    ~MetadataLookupEvent() = default;

    MetadataLookupList m_lookupList;

    static Type kEventType;
};

class META_PUBLIC MetadataLookupFailure : public QEvent
{
  public:
    explicit MetadataLookupFailure(MetadataLookupList lul) : QEvent(kEventType),
                                            m_lookupList(lul) {}
    ~MetadataLookupFailure() = default;

    MetadataLookupList m_lookupList;

    static Type kEventType;
};

class META_PUBLIC MetadataDownload : public MThread
{
  public:

    explicit MetadataDownload(QObject *parent)
        : MThread("MetadataDownload"), m_parent(parent) {}
    ~MetadataDownload();

    void addLookup(MetadataLookup *lookup);
    void prependLookup(MetadataLookup *lookup);
    void cancel();

    static QString GetMovieGrabber();
    static QString GetTelevisionGrabber();
    static QString GetGameGrabber();

    bool runGrabberTest(const QString &grabberpath);
    bool MovieGrabberWorks();
    bool TelevisionGrabberWorks();

  protected:

    void run() override; // MThread

    QString getMXMLPath(const QString& filename);
    QString getNFOPath(const QString& filename);

  private:
    // Video handling
    MetadataLookupList  handleMovie(MetadataLookup* lookup);
    MetadataLookupList  handleTelevision(MetadataLookup* lookup);
    MetadataLookupList  handleVideoUndetermined(MetadataLookup* lookup);
    MetadataLookupList  handleRecordingGeneric(MetadataLookup* lookup);

    MetadataLookupList  handleGame(MetadataLookup* lookup);

    unsigned int        findExactMatchCount(MetadataLookupList list,
                                      const QString &originaltitle,
                                      bool withArt) const;
    MetadataLookup*     findBestMatch(MetadataLookupList list,
                                      const QString &originaltitle) const;
    MetadataLookupList  runGrabber(const QString& cmd, const QStringList& args,
                                   MetadataLookup* lookup,
                                   bool passseas = true);
    MetadataLookupList  readMXML(const QString& MXMLpath,
                                 MetadataLookup* lookup,
                                 bool passseas = true);
    MetadataLookupList  readNFO(const QString& NFOpath, MetadataLookup* lookup);

    QObject            *m_parent {nullptr};
    MetadataLookupList  m_lookupList;
    QMutex              m_mutex;
};

#endif /* METADATADOWNLOAD_H */
