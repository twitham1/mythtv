//
//  mythnotification.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 25/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef __MythTV__mythnotification__
#define __MythTV__mythnotification__

#include <QMutex>
#include <QMap>
#include <QImage>

#include "mythevent.h"
#include "mythuiexp.h"

typedef QMap<QString,QString> DMAP;
typedef unsigned int    VNMask;

class MUI_PUBLIC MythNotification : public MythEvent
{
  public:

    static Type New;
    static Type Update;
    static Type Info;
    static Type Error;
    static Type Warning;
    static Type Check;
    static Type Busy;

    MythNotification(Type type, void *parent = nullptr)
        : MythEvent(type, "NOTIFICATION"), m_parent(parent) {}

    MythNotification(int id, void *parent)
        : MythEvent(Update, "NOTIFICATION"), m_id(id), m_parent(parent) {}

    MythNotification(const QString &title, const QString &author,
                     const QString &details = QString())
        : MythEvent(New, "NOTIFICATION"), m_description(title)
    {
        DMAP map;
        map["minm"] = title;
        map["asar"] = author;
        map["asal"] = details;
        m_metadata = map;
        ToStringList();
    }

    MythNotification(Type type, const QString &title, const QString &author,
                     const QString &details = QString(),
                     const QString &extra   = QString())
        : MythEvent(type, "NOTIFICATION"), m_description(title)
    {
        DMAP map;
        map["minm"] = title;
        map["asar"] = author;
        map["asal"] = details;
        map["asfm"] = extra;
        m_metadata = map;
        ToStringList();
    }

    MythNotification(Type type, const DMAP &metadata)
        : MythEvent(type, "NOTIFICATION"), m_metadata(metadata)
    {
        ToStringList();
    }

    explicit MythNotification(const MythEvent &me)
        : MythEvent(me)
    {
        FromStringList();
    }

    virtual ~MythNotification()
    {
    }

    MythEvent *clone(void) const override // MythEvent
        { return new MythNotification(*this); }

    /** Priority enum
     * A notification can be given a priority. Display order of notification
     * will be sorted according to the priority level
     */
    enum Priority {
        kDefault    = 0,
        kLow,
        kMedium,
        kHigh,
        kHigher,
        kHighest,
    };

    /** Visibility enum
     * A notification can be given visibility mask allowing to not be visible
     * under some circumstances, like the screen currently being displayed.
     * This is used to prevent redundant information appearing more than once:
     * like in MythMusic, there's no need to show music notifications
     */
    enum Visibility {
        kNone       = 0,
        kAll        = ~0,
        kPlayback   = (1 << 0),
        kSettings   = (1 << 1),
        kWizard     = (1 << 2),
        kVideos     = (1 << 3),
        kMusic      = (1 << 4),
        kRecordings = (1 << 5),
    };

    // Setter
    /**
     * Optional MythNotification elements
     */
    /**
     * contains the application registration id
     * Required to update an existing notification screen owned by an application
     */
    void SetId(int id);
    /**
     * contains the parent address. Required if id is set
     * Id provided must match the parent address as provided during the
     * MythNotificationCenter registration, otherwise the id value will be
     * ignored
     */
    void SetParent(void *parent)            { m_parent = parent; }
    /**
     * a notification may request to be displayed in full screen,
     * this request may not be fullfilled should the theme not handle full screen
     * notification
     */
    void SetFullScreen(bool f)             { m_fullScreen = f; ToStringList(); }
    /**
     * contains a short description of the notification
     */
    void SetDescription(const QString &desc)
                                      { m_description = desc; ToStringList();  }
    /**
     * metadata of the notification.
     * In DMAP format. DMAP can contains various information such as artist,
     * album name, author name, genre etc..
     */
    void SetMetaData(const DMAP &data)   { m_metadata = data; ToStringList(); }
    /**
     * contains a duration during which the notification will be displayed for.
     * The duration is informative only as the MythNotificationCenter will
     * determine automatically how long a notification can be displayed for
     * and will depend on priority, visibility and other factors
     */
    void SetDuration(int duration)     { m_duration = duration; ToStringList(); }
    /**
     * contains an alternative notification style.
     * Should a style be defined, the Notification Center will attempt to load
     * an alternative theme and fall back to the default one if unsuccessful
     */
    void SetStyle(const QString &style)     { m_style = style; ToStringList(); }
    /**
     * define a bitmask of Visibility
     */
    void SetVisibility(VNMask n)            { m_visibility = n; ToStringList(); }
    /**
     * For future use, not implemented at this stage
     */
    void SetPriority(Priority n)             { m_priority = n; ToStringList(); }

    /**
     * return Type object from type name
     */
    static Type TypeFromString(const QString &type);

    void ToStringList(void);
    bool FromStringList(void);

    // Getter
    int         GetId(void) const           { return m_id; }
    void       *GetParent(void) const       { return m_parent; }
    bool        GetFullScreen(void) const   { return m_fullScreen; }
    QString     GetDescription(void) const  { return m_description; }
    DMAP        GetMetaData(void) const     { return m_metadata; }
    int         GetDuration(void) const     { return m_duration; };
    QString     GetStyle(void) const        { return m_style; }
    VNMask      GetVisibility(void) const   { return m_visibility; }
    Priority    GetPriority(void) const     { return m_priority; }

  protected:
    MythNotification(const MythNotification &o)
        : MythEvent(o),
        m_id(o.m_id),      m_parent(o.m_parent),   m_fullScreen(o.m_fullScreen),
        m_description(o.m_description),
        m_duration(o.m_duration),                  m_metadata(o.m_metadata),
        m_style(o.m_style),
        m_visibility(o.m_visibility),              m_priority(o.m_priority)
    {
        ToStringList();
    }

#ifndef _MSC_VER
    MythNotification &operator=(const MythNotification&);
#endif

  protected:
    int         m_id          {-1};
    void       *m_parent      {nullptr};
    bool        m_fullScreen  {false};
    QString     m_description;
    int         m_duration    {0};
    DMAP        m_metadata;
    QString     m_style;
    VNMask      m_visibility  {(VNMask)kAll};
    Priority    m_priority    {kDefault};
};

class MUI_PUBLIC MythImageNotification : public virtual MythNotification
{
  public:
    MythImageNotification(Type type, const QImage &image)
        : MythNotification(type), m_image(image)
    {
    }

    MythImageNotification(Type type, const QString &imagePath)
        : MythNotification(type), m_imagePath(imagePath)
    {
    }

    MythImageNotification(Type type, const QImage &image, const DMAP &metadata)
        : MythNotification(type, metadata), m_image(image)
    {
    }

    MythImageNotification(Type type, const QString &imagePath, const DMAP &metadata)
        : MythNotification(type, metadata), m_imagePath(imagePath)
    {
    }

    MythEvent *clone(void) const override // MythNotification
        { return new MythImageNotification(*this); }

    // Setter
    /**
     * image to be displayed with the notification
     */
    void SetImage(const QImage &image)      { m_image = image; }
    /**
     * image filename to be displayed with the notification
     */
    void SetImagePath(const QString &image) { m_imagePath = image; }

    //Getter
    QImage GetImage(void) const             { return m_image; }
    QString GetImagePath(void) const        { return m_imagePath; }

  protected:
    MythImageNotification(const MythImageNotification &o)
        : MythNotification(o), m_image(o.m_image), m_imagePath(o.m_imagePath)
    {
    }

  protected:
    QImage      m_image;
    QString     m_imagePath;
};

class MUI_PUBLIC MythPlaybackNotification : public virtual MythNotification
{
  public:
    MythPlaybackNotification(Type type, float progress, const QString &progressText)
        : MythNotification(type), m_progress(progress), m_progressText(progressText)
    {
    }

    MythPlaybackNotification(Type type, float progress, const QString &progressText,
                             const DMAP &metadata)
        : MythNotification(type, metadata),
        m_progress(progress), m_progressText(progressText)
    {
    }

    MythPlaybackNotification(Type type, int duration, int position)
        : MythNotification(type)
    {
        m_progress      = (float)position / (float)duration;
        m_progressText  = stringFromSeconds(duration);
    }

    MythEvent *clone(void) const override // MythNotification
        { return new MythPlaybackNotification(*this); }

    // Setter
    /**
     * current playback position to be displayed with the notification.
     * Value to be between 0 <= x <= 1.
     * Note: x < 0 means no progress bar to be displayed.
     */
    void SetProgress(float progress)        { m_progress = progress; }
    /**
     * text to be displayed with the notification as duration or progress.
     */
    void SetProgressText(const QString &text) { m_progressText = text; }

    //Getter
    float GetProgress(void) const           { return m_progress; }
    QString GetProgressText(void) const     { return m_progressText; }

    // utility methods
    static QString stringFromSeconds(int time);

  protected:
    MythPlaybackNotification(const MythPlaybackNotification &o)
        : MythNotification(o),
        m_progress(o.m_progress), m_progressText(o.m_progressText)
    {
    }

  protected:
    float       m_progress;
    QString     m_progressText;
};

class MUI_PUBLIC MythMediaNotification : public MythImageNotification,
                                         public MythPlaybackNotification
{
  public:
    MythMediaNotification(Type type, const QImage &image, const DMAP &metadata,
                          float progress, const QString &durationText)
        : MythNotification(type, metadata), MythImageNotification(type, image),
        MythPlaybackNotification(type, progress, durationText)
    {
    }

    MythMediaNotification(Type type, const QImage &image, const DMAP &metadata,
                          int duration, int position)
        : MythNotification(type, metadata), MythImageNotification(type, image),
        MythPlaybackNotification(type, duration, position)
    {
    }

    MythMediaNotification(Type type, const QString &imagePath, const DMAP &metadata,
                          float progress, const QString &durationText)
        : MythNotification(type, metadata), MythImageNotification(type, imagePath),
        MythPlaybackNotification(type, progress, durationText)
    {
    }

    MythMediaNotification(Type type, const QString &imagePath, const DMAP &metadata,
                          int duration, int position)
        : MythNotification(type, metadata), MythImageNotification(type, imagePath),
        MythPlaybackNotification(type, duration, position)
    {
    }

    MythEvent *clone(void) const override // MythImageNotification
        { return new MythMediaNotification(*this); }

  protected:
    MythMediaNotification(const MythMediaNotification &o)
        : MythNotification(o), MythImageNotification(o), MythPlaybackNotification(o)
    {
    }
};

class MUI_PUBLIC MythErrorNotification : public MythNotification
{
  public:
    MythErrorNotification(const QString &title, const QString &author,
                          const QString &details = QString())
        : MythNotification(Error, title, author, details)
    {
        SetDuration(10);
    }
};

class MUI_PUBLIC MythWarningNotification : public MythNotification
{
  public:
    MythWarningNotification(const QString &title, const QString &author,
                            const QString &details = QString())
    : MythNotification(Warning, title, author, details)
    {
        SetDuration(10);
    }
};

class MUI_PUBLIC MythCheckNotification : public MythNotification
{
  public:
    MythCheckNotification(const QString &title, const QString &author,
                          const QString &details = QString())
    : MythNotification(Check, title, author, details)
    {
        SetDuration(5);
    }
};

class MUI_PUBLIC MythBusyNotification : public MythNotification
{
  public:
    MythBusyNotification(const QString &title, const QString &author,
                         const QString &details = QString())
    : MythNotification(Busy, title, author, details) { }
};

#endif /* defined(__MythTV__mythnotification__) */
