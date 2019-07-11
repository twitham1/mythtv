/*
 * TemplateFinder
 *
 * Attempt to infer the existence of a static template across a series of
 * images by looking for stable edges. Some ideas are taken from
 * http://thomashargrove.com/logo-detection/
 *
 * By operating on the edges of images rather than the image data itself, both
 * opaque and transparent templates are robustly located (if they exist).
 * Hopefully the "core" portion of animated templates is sufficiently large and
 * stable enough for animated templates to also be discovered.
 *
 * This TemplateFinder only expects to successfully discover non- or
 * minimally-moving templates. Templates that change position during the
 * sequence of images will cause this algorithm to fail.
 */

#ifndef __TEMPLATEFINDER_H__
#define __TEMPLATEFINDER_H__

extern "C" {
#include "libavcodec/avcodec.h"    /* AVFrame */
}
#include "FrameAnalyzer.h"

class PGMConverter;
class BorderDetector;
class EdgeDetector;

class TemplateFinder : public FrameAnalyzer
{
public:
    /* Ctor/dtor. */
    TemplateFinder(PGMConverter *pgmc, BorderDetector *bd, EdgeDetector *ed,
            MythPlayer *player, int proglen, const QString& debugdir);
    ~TemplateFinder(void);

    /* FrameAnalyzer interface. */
    const char *name(void) const override // FrameAnalyzer
        { return "TemplateFinder"; }
    enum analyzeFrameResult MythPlayerInited(MythPlayer *player,
            long long nframes) override; // FrameAnalyzer
    enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame) override; // FrameAnalyzer
    int finished(long long nframes, bool final) override; // FrameAnalyzer
    int reportTime(void) const override; // FrameAnalyzer
    FrameMap GetMap(unsigned int) const override // FrameAnalyzer
        { FrameMap map; return map; }

    /* TemplateFinder implementation. */
    const struct AVFrame *getTemplate(int *prow, int *pcol,
            int *pwidth, int *pheight) const;

private:
    int resetBuffers(int newwidth, int newheight);

    PGMConverter   *m_pgmConverter;
    BorderDetector *m_borderDetector;
    EdgeDetector   *m_edgeDetector;

    unsigned int    m_sampleTime       {20 * 60}; /* amount of time to analyze */
    int             m_frameInterval;              /* analyze every <Interval> frames */
    long long       m_endFrame         {0};       /* end of logo detection */
    long long       m_nextFrame        {0};       /* next desired frame */

    int             m_width            {-1};      /* dimensions of frames */
    int             m_height           {-1};      /* dimensions of frames */
    unsigned int   *scores             {nullptr}; /* pixel "edge" scores */

    int             mincontentrow      {INT_MAX}; /* limits of content area of images */
    int             mincontentcol      {INT_MAX};
    int             maxcontentrow1     {INT_MAX}; /* minrow + height ("maxrow + 1") */
    int             maxcontentcol1     {INT_MAX}; /* mincol + width ("maxcol + 1") */

    AVFrame         m_tmpl;                       /* logo-matching template */
    int             m_tmplrow          {-1};
    int             m_tmplcol          {-1};
    int             m_tmplwidth        {-1};
    int             m_tmplheight       {-1};

    AVFrame         m_cropped;                    /* cropped version of frame */
    int             m_cwidth           {-1};      /* cropped width */
    int             m_cheight          {-1};      /* cropped height */

    /* Debugging. */
    int             m_debugLevel       {0};
    QString         m_debugdir;
    QString         m_debugdata;                  /* filename: template location */
    QString         m_debugtmpl;                  /* filename: logo template */
    bool            m_debug_template   {false};
    bool            m_debug_edgecounts {false};
    bool            m_debug_frames     {false};
    bool            m_tmpl_valid       {false};
    bool            m_tmpl_done        {false};
    struct timeval  m_analyze_time;
};

#endif  /* !__TEMPLATEFINDER_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
