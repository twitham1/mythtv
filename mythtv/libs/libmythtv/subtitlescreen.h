// -*- Mode: c++ -*-

#ifndef SUBTITLESCREEN_H
#define SUBTITLESCREEN_H

#include <QStringList>
#include <QRegExp>
#include <QVector>
#include <QFont>
#include <QHash>
#include <QRect>
#include <QSize>

#ifdef USING_LIBASS
extern "C" {
#include <ass/ass.h>
}
#endif

#include "mythscreentype.h"
#include "subtitlereader.h"
#include "mythplayer.h"
#include "mythuishape.h"
#include "mythuisimpletext.h"
#include "mythuiimage.h"

class SubtitleScreen;

class FormattedTextChunk
{
public:
    FormattedTextChunk(const QString &t,
                       const CC708CharacterAttribute &formatting,
                       SubtitleScreen *p)
        : m_text(t), m_format(formatting), m_parent(p) {}
    FormattedTextChunk(void) = default;

    QSize CalcSize(float layoutSpacing = 0.0F) const;
    void CalcPadding(bool isFirst, bool isLast, int &left, int &right) const;
    bool Split(FormattedTextChunk &newChunk);
    QString ToLogString(void) const;
    bool PreRender(bool isFirst, bool isLast, int &x, int y, int height);

    QString               m_text;
    CC708CharacterAttribute m_format; // const
    const SubtitleScreen *m_parent {nullptr}; // where fonts and sizes are kept

    // The following are calculated by PreRender().
    QString               m_bgShapeName;
    QRect                 m_bgShapeRect;
    MythFontProperties   *m_textFont {nullptr};
    QString               m_textName;
    QRect                 m_textRect;
};

class FormattedTextLine
{
public:
    FormattedTextLine(int x = -1, int y = -1, int o_x = -1, int o_y = -1)
        : m_xIndent(x), m_yIndent(y), m_origX(o_x), m_origY(o_y) {}

    QSize CalcSize(float layoutSpacing = 0.0F) const;

    QList<FormattedTextChunk> chunks;
    int m_xIndent {-1}; // -1 means TBD i.e. centered
    int m_yIndent {-1}; // -1 means TBD i.e. relative to bottom
    int m_origX   {-1}; // original, unscaled coordinates
    int m_origY   {-1}; // original, unscaled coordinates
};

class FormattedTextSubtitle
{
protected:
    FormattedTextSubtitle(const QString &base, const QRect &safearea,
                          uint64_t start, uint64_t duration,
                          SubtitleScreen *p) :
        m_base(base), m_safeArea(safearea),
        m_start(start), m_duration(duration), m_subScreen(p) {}
    FormattedTextSubtitle(void) = default;
public:
    virtual ~FormattedTextSubtitle() = default;
    // These are the steps that can be done outside of the UI thread
    // and the decoder thread.
    virtual void WrapLongLines(void) {}
    virtual void Layout(void);
    virtual void PreRender(void);
    // This is the step that can only be done in the UI thread.
    virtual void Draw(void);
    virtual int CacheNum(void) const { return -1; }
    QStringList ToSRT(void) const;

protected:
    QString         m_base;
    QVector<FormattedTextLine> m_lines;
    const QRect     m_safeArea;
    uint64_t        m_start        {0};
    uint64_t        m_duration     {0};
    SubtitleScreen *m_subScreen    {nullptr}; // where fonts and sizes are kept
    int             m_xAnchorPoint {0}; // 0=left, 1=center, 2=right
    int             m_yAnchorPoint {0}; // 0=top,  1=center, 2=bottom
    int             m_xAnchor      {0}; // pixels from left
    int             m_yAnchor      {0}; // pixels from top
    QRect           m_bounds;
};

class FormattedTextSubtitleSRT : public FormattedTextSubtitle
{
public:
    FormattedTextSubtitleSRT(const QString &base,
                             const QRect &safearea,
                             uint64_t start,
                             uint64_t duration,
                             SubtitleScreen *p,
                             const QStringList &subs) :
        FormattedTextSubtitle(base, safearea, start, duration, p)
    {
        Init(subs);
    }
    void WrapLongLines(void) override; // FormattedTextSubtitle
private:
    void Init(const QStringList &subs);
};

class FormattedTextSubtitle608 : public FormattedTextSubtitle
{
public:
    FormattedTextSubtitle608(const vector<CC608Text*> &buffers,
                             const QString &base = "",
                             const QRect &safearea = QRect(),
                             SubtitleScreen *p = nullptr) :
        FormattedTextSubtitle(base, safearea, 0, 0, p)
    {
        Init(buffers);
    }
    void Layout(void) override; // FormattedTextSubtitle
private:
    void Init(const vector<CC608Text*> &buffers);
};

class FormattedTextSubtitle708 : public FormattedTextSubtitle
{
public:
    FormattedTextSubtitle708(const CC708Window &win,
                             int num,
                             const vector<CC708String*> &list,
                             const QString &base = "",
                             const QRect &safearea = QRect(),
                             SubtitleScreen *p = nullptr,
                             float aspect = 1.77777f) :
        FormattedTextSubtitle(base, safearea, 0, 0, p),
        m_num(num),
        m_bgFillAlpha(win.GetFillAlpha()),
        m_bgFillColor(win.GetFillColor())
    {
        Init(win, list, aspect);
    }
    void Draw(void) override; // FormattedTextSubtitle
    int CacheNum(void) const override // FormattedTextSubtitle
        { return m_num; }
private:
    void Init(const CC708Window &win,
              const vector<CC708String*> &list,
              float aspect);
    int m_num;
    uint m_bgFillAlpha;
    QColor m_bgFillColor;
};

class SubtitleScreen : public MythScreenType
{
public:
    SubtitleScreen(MythPlayer *player, const char * name, int fontStretch);
    virtual ~SubtitleScreen();

    void EnableSubtitles(int type, bool forced_only = false);
    void DisableForcedSubtitles(void);
    int  EnabledSubtitleType(void) { return m_subtitleType; }

    void ClearAllSubtitles(void);
    void ClearNonDisplayedSubtitles(void);
    void ClearDisplayedSubtitles(void);
    void DisplayDVDButton(AVSubtitle* dvdButton, QRect &buttonPos);

    void SetZoom(int percent);
    int GetZoom(void) const;
    void SetDelay(int ms);
    int GetDelay(void) const;

    class SubtitleFormat *GetSubtitleFormat(void) { return m_format; }
    void Clear708Cache(uint64_t mask);
    void SetElementAdded(void);
    void SetElementResized(void);
    void SetElementDeleted(void);

    QSize CalcTextSize(const QString &text,
                       const CC708CharacterAttribute &format,
                       float layoutSpacing) const;
    void CalcPadding(const CC708CharacterAttribute &format,
                     bool isFirst, bool isLast,
                     int &left, int &right) const;
    MythFontProperties* GetFont(const CC708CharacterAttribute &attr) const;
    void SetFontSize(int pixelSize) { m_fontSize = pixelSize; }

    // Temporary methods until teletextscreen.cpp is refactored into
    // subtitlescreen.cpp
    static QString GetTeletextFontName(void);
    static int GetTeletextBackgroundAlpha(void);

    // MythScreenType methods
    bool Create(void) override; // MythScreenType
    void Pulse(void) override; // MythUIType

private:
    void ResetElementState(void);
    void OptimiseDisplayedArea(void);
    void DisplayAVSubtitles(void);
    int  DisplayScaledAVSubtitles(const AVSubtitleRect *rect, QRect &bbox,
                                  bool top, QRect &display, int forced,
                                  const QString& imagename,
                                  long long displayuntil, long long late);
    void DisplayTextSubtitles(void);
    void DisplayRawTextSubtitles(void);
    void DrawTextSubtitles(const QStringList &subs, uint64_t start,
                           uint64_t duration);
    void DisplayCC608Subtitles(void);
    void DisplayCC708Subtitles(void);
    void AddScaledImage(QImage &img, QRect &pos);
    void InitializeFonts(bool wasResized);

    MythPlayer     *m_player              {nullptr};
    SubtitleReader *m_subreader           {nullptr};
    CC608Reader    *m_608reader           {nullptr};
    CC708Reader    *m_708reader           {nullptr};
    QRect           m_safeArea;
    QRegExp         m_removeHTML          {"</?.+>"};
    int             m_subtitleType        {kDisplayNone};
    int             m_fontSize            {0};
    int             m_textFontZoom        {100}; // valid for 708 & text subs
    int             m_textFontZoomPrev    {100};
    int             m_textFontDelayMs     {0}; // valid for text subs
    int             m_textFontDelayMsPrev {0};
    bool            m_refreshModified     {false};
    bool            m_refreshDeleted      {false};
    int             m_fontStretch;
    QString         m_family; // 608, 708, text, teletext
    // Subtitles initialized but still to be processed and drawn
    QList<FormattedTextSubtitle *> m_qInited;
    class SubtitleFormat *m_format        {nullptr};

#ifdef USING_LIBASS
    bool InitialiseAssLibrary(void);
    void LoadAssFonts(void);
    void CleanupAssLibrary(void);
    void InitialiseAssTrack(int tracknum);
    void CleanupAssTrack(void);
    void AddAssEvent(char *event);
    void ResizeAssRenderer(void);
    void RenderAssTrack(uint64_t timecode);

    ASS_Library    *m_assLibrary          {nullptr};
    ASS_Renderer   *m_assRenderer         {nullptr};
    int             m_assTrackNum         {-1};
    ASS_Track      *m_assTrack            {nullptr};
    uint            m_assFontCount        {0};
#endif // USING_LIBASS
};

#endif // SUBTITLESCREEN_H
