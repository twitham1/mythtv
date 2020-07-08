/*
 * PGMConverter
 *
 * Object to convert a MythPlayer frame into a greyscale image.
 */

#ifndef PGMCONVERTER_H
#define PGMCONVERTER_H

extern "C" {
#include "libavcodec/avcodec.h"    /* AVFrame */
}

class MythPlayer;
class MythAVCopy;

/*
 * PGM_CONVERT_GREYSCALE:
 *
 * If #define'd, perform a true greyscale conversion of "frame" in
 * PGMConverter::getImage .
 *
 * If #undef'd, just fake up a greyscale data structure. The "frame" data is
 * YUV data, and the Y channel is close enough for our purposes, and it's
 * faster than a full-blown true greyscale conversion.
 */
#define PGM_CONVERT_GREYSCALE

class PGMConverter
{
public:
    /* Ctor/dtor. */
    PGMConverter(void) = default;
    ~PGMConverter(void);

    int MythPlayerInited(const MythPlayer *player);
    const AVFrame *getImage(const VideoFrame *frame, long long frameno,
            int *pwidth, int *pheight);
    int reportTime(void);

private:
    long long       m_frameNo       {-1}; /* frame number */
    int             m_width         {-1}; /* frame dimensions */
    int             m_height        {-1}; /* frame dimensions */
    AVFrame         m_pgm           {};   /* grayscale frame */
#ifdef PGM_CONVERT_GREYSCALE
    struct timeval  m_convertTime   {0,0};
    bool            m_timeReported  {false};
    MythAVCopy     *m_copy          {nullptr};
#endif /* PGM_CONVERT_GREYSCALE */
};

#endif  /* !PGMCONVERTER_H */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
