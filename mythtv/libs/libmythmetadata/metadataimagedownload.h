#ifndef METADATAIMAGEDOWNLOAD_H
#define METADATAIMAGEDOWNLOAD_H

#include <QString>
#include <QStringList>
#include <QMutex>

#include "mthread.h"
#include "mythmetaexp.h"
#include "metadatacommon.h"

typedef struct {
    QString title;
    QVariant data;
    QString url;
} ThumbnailData;

class META_PUBLIC ImageDLEvent : public QEvent
{
  public:
    explicit ImageDLEvent(MetadataLookup *lookup) :
                 QEvent(kEventType),
                 m_item(lookup)
    {
        if (m_item)
        {
            m_item->IncrRef();
        }
    }
    ~ImageDLEvent()
    {
        if (m_item)
        {
            m_item->DecrRef();
            m_item = nullptr;
        }
    }

    MetadataLookup *m_item {nullptr};

    static Type kEventType;
};

class META_PUBLIC ImageDLFailureEvent : public QEvent
{
  public:
    explicit ImageDLFailureEvent(MetadataLookup *lookup) :
                 QEvent(kEventType),
                 m_item(lookup)
    {
        if (m_item)
        {
            m_item->IncrRef();
        }
    }
    ~ImageDLFailureEvent()
    {
        if (m_item)
        {
            m_item->DecrRef();
            m_item = nullptr;
        }
    }

    MetadataLookup *m_item {nullptr};

    static Type kEventType;
};

class META_PUBLIC ThumbnailDLEvent : public QEvent
{
  public:
    explicit ThumbnailDLEvent(ThumbnailData *data) :
                 QEvent(kEventType),
                 m_thumb(data) {}
    ~ThumbnailDLEvent()
    {
        delete m_thumb;
        m_thumb = nullptr;
    }

    ThumbnailData *m_thumb {nullptr};

    static Type kEventType;
};

class META_PUBLIC MetadataImageDownload : public MThread
{
  public:

    explicit MetadataImageDownload(QObject *parent)
        : MThread("MetadataImageDownload"), m_parent(parent) {}
    ~MetadataImageDownload();

    void addThumb(QString title, QString url, QVariant data);
    void addDownloads(MetadataLookup *lookup);
    void cancel();

  protected:

    void run() override; // MThread

  private:

    ThumbnailData*             moreThumbs();

    QObject                   *m_parent;
    MetadataLookupList         m_downloadList;
    QList<ThumbnailData*>      m_thumbnailList;
    QMutex                     m_mutex;
};

META_PUBLIC QString getDownloadFilename(const QString& title, const QString& url);
META_PUBLIC QString getDownloadFilename(VideoArtworkType type, MetadataLookup *lookup,
                                    const QString& url);

META_PUBLIC QString getLocalWritePath(MetadataType metadatatype, VideoArtworkType type);
META_PUBLIC QString getStorageGroupURL(VideoArtworkType type, const QString& host);
META_PUBLIC QString getLocalStorageGroupPath(VideoArtworkType type, const QString& host);
META_PUBLIC QString getStorageGroupName(VideoArtworkType type);

META_PUBLIC void cleanThumbnailCacheDir(void);

#endif /* METADATAIMAGEDOWNLOAD_H */
