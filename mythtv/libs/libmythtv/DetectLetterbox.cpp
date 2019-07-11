// -*- Mode: c++ -*-

// MythTV headers
#include "DetectLetterbox.h"
#include "mythplayer.h"
#include "videoouttypes.h"
#include "mythcorecontext.h"

DetectLetterbox::DetectLetterbox(MythPlayer* const player)
{
    int dbAdjustFill = gCoreContext->GetNumSetting("AdjustFill", 0);
    m_isDetectLetterbox = dbAdjustFill >= kAdjustFill_AutoDetect_DefaultOff;
    m_detectLetterboxDefaultMode = (AdjustFillMode) max((int) kAdjustFill_Off,
                                 dbAdjustFill - kAdjustFill_AutoDetect_DefaultOff);
    m_detectLetterboxDetectedMode = player->GetAdjustFill();
    m_detectLetterboxLimit = gCoreContext->GetNumSetting("DetectLeterboxLimit", 75);
    m_player = player;
}

/** \fn DetectLetterbox::Detect(VideoFrame*)
 *  \brief Detects if this frame is or is not letterboxed
 *
 *  If a change is detected detectLetterboxSwitchFrame and
 *  detectLetterboxDetectedMode are set.
 */
void DetectLetterbox::Detect(VideoFrame *frame)
{
    unsigned char *buf = frame->buf;
    int *pitches = frame->pitches;
    int *offsets = frame->offsets;
    const int width = frame->width;
    const int height = frame->height;
    const long long frameNumber = frame->frameNumber;
    const int NUMBER_OF_DETECTION_LINES = 3; // How many lines are we looking at
    const int THRESHOLD = 5; // Y component has to not vary more than this in the bars
    const int HORIZONTAL_THRESHOLD = 4; // How tolerant are we that the image has horizontal edges

    // If the black bars is larger than this limit we switch to Half or Full Mode
    //    const int fullLimit = (int) (((height - width * 9 / 16) / 2) * m_detectLetterboxLimit / 100);
    //    const int halfLimit = (int) (((height - width * 9 / 14) / 2) * m_detectLetterboxLimit / 100);
    // If the black bars is larger than this limit we switch to Half or Full Mode
    const int fullLimit = (int) ((height * (1 - m_player->GetVideoAspect() * 9 / 16) / 2) * m_detectLetterboxLimit / 100);
    const int halfLimit = (int) ((height * (1 - m_player->GetVideoAspect() * 9 / 14) / 2) * m_detectLetterboxLimit / 100);

    const int xPos[] = {width / 4, width / 2, width * 3 / 4};    // Lines to scan for black letterbox edge
    int topHits = 0, bottomHits = 0, minTop = 0, minBottom = 0, maxTop = 0, maxBottom = 0;
    int topHit[] = {0, 0, 0}, bottomHit[] = {0, 0, 0};

    if (!GetDetectLetterbox())
        return;

    if (!m_player->GetVideoOutput())
        return;

    switch (frame->codec) {
        case FMT_YV12:
            if (!m_firstFrameChecked)
            {
                m_firstFrameChecked = frameNumber;
                LOG(VB_PLAYBACK, LOG_INFO,
                    QString("Detect Letterbox: YV12 frame format detected"));
            }
            break;
        default:
            LOG(VB_PLAYBACK, LOG_INFO,
                QString("Detect Letterbox: The source is not "
                        "a supported frame format (was %1)").arg(frame->codec));
            m_isDetectLetterbox = false;
            return;
    }

    if (frameNumber < 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO,
             QString("Detect Letterbox: Strange frame number %1")
                 .arg(frameNumber));
        return;
    }

    if (m_player->GetVideoAspect() > 1.5F)
    {
        if (m_detectLetterboxDetectedMode != m_detectLetterboxDefaultMode)
        {
            LOG(VB_PLAYBACK, LOG_INFO,
                QString("Detect Letterbox: The source is "
                        "already in widescreen (aspect: %1)")
                    .arg(m_player->GetVideoAspect()));
            m_detectLetterboxLock.lock();
            m_detectLetterboxConsecutiveCounter = 0;
            m_detectLetterboxDetectedMode = m_detectLetterboxDefaultMode;
            m_detectLetterboxSwitchFrame = frameNumber;
            m_detectLetterboxLock.unlock();
        }
        else
        {
            m_detectLetterboxConsecutiveCounter++;
        }
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("Detect Letterbox: The source is already "
                    "in widescreen (aspect: %1)")
                .arg(m_player->GetVideoAspect()));
        m_isDetectLetterbox = false;
        return;
    }

    // Establish the level of light in the edge
    int averageY = 0;
    for (int detectionLine = 0;
         detectionLine < NUMBER_OF_DETECTION_LINES;
         detectionLine++)
    {
        averageY += buf[offsets[0] + 5 * pitches[0]            + xPos[detectionLine]];
        averageY += buf[offsets[0] + (height - 6) * pitches[0] + xPos[detectionLine]];
    }
    averageY /= NUMBER_OF_DETECTION_LINES * 2;
    if (averageY > 64) // To bright to be a letterbox border
        averageY = 0;

    // Scan the detection lines
    for (int y = 5; y < height / 4; y++) // skip first pixels incase of noise in the edge
    {
        for (int detectionLine = 0;
             detectionLine < NUMBER_OF_DETECTION_LINES;
             detectionLine++)
        {
            int Y = buf[offsets[0] +  y     * pitches[0] +  xPos[detectionLine]];
            int U = buf[offsets[1] + (y>>1) * pitches[1] + (xPos[detectionLine]>>1)];
            int V = buf[offsets[2] + (y>>1) * pitches[2] + (xPos[detectionLine]>>1)];
            if ((!topHit[detectionLine]) &&
                ( Y > averageY + THRESHOLD || Y < averageY - THRESHOLD ||
                  U < 128 - 32 || U > 128 + 32 ||
                  V < 128 - 32 || V > 128 + 32 ))
            {
                topHit[detectionLine] = y;
                topHits++;
                if (!minTop)
                    minTop = y;
                maxTop = y;
            }

            Y = buf[offsets[0] + (height-y-1     ) * pitches[0] + xPos[detectionLine]];
            U = buf[offsets[1] + ((height-y-1) >> 1) * pitches[1] + (xPos[detectionLine]>>1)];
            V = buf[offsets[2] + ((height-y-1) >> 1) * pitches[2] + (xPos[detectionLine]>>1)];
            if ((!bottomHit[detectionLine]) &&
                ( Y > averageY + THRESHOLD || Y < averageY - THRESHOLD ||
                  U < 128 - 32 || U > 128 + 32 ||
                  V < 128 - 32 || V > 128 + 32 ))
            {
                bottomHit[detectionLine] = y;
                bottomHits++;
                if (!minBottom)
                    minBottom = y;
                maxBottom = y;
            }
        }

        if (topHits == NUMBER_OF_DETECTION_LINES &&
            bottomHits == NUMBER_OF_DETECTION_LINES)
        {
            break;
        }
    }
    if (topHits != NUMBER_OF_DETECTION_LINES) maxTop = height / 4;
    if (!minTop) minTop = height / 4;
    if (bottomHits != NUMBER_OF_DETECTION_LINES) maxBottom = height / 4;
    if (!minBottom) minBottom = height / 4;

    bool horizontal = ((minTop && maxTop - minTop < HORIZONTAL_THRESHOLD) &&
                       (minBottom && maxBottom - minBottom < HORIZONTAL_THRESHOLD));

    if (m_detectLetterboxSwitchFrame > frameNumber) // user is reversing
    {
        m_detectLetterboxLock.lock();
        m_detectLetterboxDetectedMode = m_player->GetAdjustFill();
        m_detectLetterboxSwitchFrame = -1;
        m_detectLetterboxPossibleHalfFrame = -1;
        m_detectLetterboxPossibleFullFrame = -1;
        m_detectLetterboxLock.unlock();
    }

    if (minTop < halfLimit || minBottom < halfLimit)
        m_detectLetterboxPossibleHalfFrame = -1;
    if (minTop < fullLimit || minBottom < fullLimit)
        m_detectLetterboxPossibleFullFrame = -1;

    if (m_detectLetterboxDetectedMode != kAdjustFill_Full)
    {
        if (m_detectLetterboxPossibleHalfFrame == -1 &&
            minTop > halfLimit && minBottom > halfLimit) {
            m_detectLetterboxPossibleHalfFrame = frameNumber;
        }
    }
    else
    {
        if (m_detectLetterboxPossibleHalfFrame == -1 &&
            minTop < fullLimit && minBottom < fullLimit) {
            m_detectLetterboxPossibleHalfFrame = frameNumber;
        }
    }
    if (m_detectLetterboxPossibleFullFrame == -1 &&
        minTop > fullLimit && minBottom > fullLimit)
        m_detectLetterboxPossibleFullFrame = frameNumber;

    if ( maxTop < halfLimit || maxBottom < halfLimit) // Not to restrictive when switching to off
    {
        // No Letterbox
        if (m_detectLetterboxDetectedMode != m_detectLetterboxDefaultMode)
        {
            LOG(VB_PLAYBACK, LOG_INFO,
                QString("Detect Letterbox: Non Letterbox "
                        "detected on line: %1 (limit: %2)")
                    .arg(min(maxTop, maxBottom)).arg(halfLimit));
            m_detectLetterboxLock.lock();
            m_detectLetterboxConsecutiveCounter = 0;
            m_detectLetterboxDetectedMode = m_detectLetterboxDefaultMode;
            m_detectLetterboxSwitchFrame = frameNumber;
            m_detectLetterboxLock.unlock();
        }
        else
        {
            m_detectLetterboxConsecutiveCounter++;
        }
    }
    else if (horizontal && minTop > halfLimit && minBottom > halfLimit &&
             maxTop < fullLimit && maxBottom < fullLimit)
    {
        // Letterbox (with narrow bars)
        if (m_detectLetterboxDetectedMode != kAdjustFill_Half)
        {
            LOG(VB_PLAYBACK, LOG_INFO,
                QString("Detect Letterbox: Narrow Letterbox "
                        "detected on line: %1 (limit: %2) frame: %3")
                    .arg(minTop).arg(halfLimit)
                    .arg(m_detectLetterboxPossibleHalfFrame));
            m_detectLetterboxLock.lock();
            m_detectLetterboxConsecutiveCounter = 0;
            if (m_detectLetterboxDetectedMode == kAdjustFill_Full &&
                m_detectLetterboxSwitchFrame != -1) {
                // Do not change switch frame if switch to Full mode has not been executed yet
            }
            else
                m_detectLetterboxSwitchFrame = m_detectLetterboxPossibleHalfFrame;
            m_detectLetterboxDetectedMode = kAdjustFill_Half;
            m_detectLetterboxLock.unlock();
        }
        else
        {
            m_detectLetterboxConsecutiveCounter++;
        }
    }
    else if (horizontal && minTop > fullLimit && minBottom > fullLimit)
    {
        // Letterbox
        m_detectLetterboxPossibleHalfFrame = -1;
        if (m_detectLetterboxDetectedMode != kAdjustFill_Full)
        {
            LOG(VB_PLAYBACK, LOG_INFO,
                QString("Detect Letterbox: Detected Letterbox "
                        "on line: %1 (limit: %2) frame: %3").arg(minTop)
                    .arg(fullLimit).arg(m_detectLetterboxPossibleFullFrame));
            m_detectLetterboxLock.lock();
            m_detectLetterboxConsecutiveCounter = 0;
            m_detectLetterboxDetectedMode = kAdjustFill_Full;
            m_detectLetterboxSwitchFrame = m_detectLetterboxPossibleFullFrame;
            m_detectLetterboxLock.unlock();
        }
        else
        {
            m_detectLetterboxConsecutiveCounter++;
        }
    }
    else
    {
        if (m_detectLetterboxConsecutiveCounter <= 3)
            m_detectLetterboxConsecutiveCounter = 0;
    }
}

/** \fn DetectLetterbox::SwitchTo(VideoFrame*)
 *  \brief Switch to the mode detected by DetectLetterbox
 *
 *  Switch fill mode if a switch was detected for this frame.
 */
void DetectLetterbox::SwitchTo(VideoFrame *frame)
{
    if (!GetDetectLetterbox())
        return;

    if (m_detectLetterboxSwitchFrame == -1)
        return;

    m_detectLetterboxLock.lock();
    if (m_detectLetterboxSwitchFrame <= frame->frameNumber &&
        m_detectLetterboxConsecutiveCounter > 3)
    {
        if (m_player->GetAdjustFill() != m_detectLetterboxDetectedMode)
        {
            LOG(VB_PLAYBACK, LOG_INFO,
                QString("Detect Letterbox: Switched to %1 "
                        "on frame %2 (%3)").arg(m_detectLetterboxDetectedMode)
                    .arg(frame->frameNumber)
                    .arg(m_detectLetterboxSwitchFrame));
            m_player->GetVideoOutput()->
                      ToggleAdjustFill(m_detectLetterboxDetectedMode);
            m_player->ReinitOSD();
        }
        m_detectLetterboxSwitchFrame = -1;
    }
    else if (m_detectLetterboxSwitchFrame <= frame->frameNumber)
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("Detect Letterbox: Not Switched to %1 on frame %2 "
                    "(%3) Not enough consecutive detections (%4)")
                .arg(m_detectLetterboxDetectedMode)
                .arg(frame->frameNumber).arg(m_detectLetterboxSwitchFrame)
                .arg(m_detectLetterboxConsecutiveCounter));

    m_detectLetterboxLock.unlock();
}

void DetectLetterbox::SetDetectLetterbox(bool detect)
{
    m_isDetectLetterbox = detect;
    m_detectLetterboxSwitchFrame = -1;
    m_detectLetterboxDetectedMode = m_player->GetAdjustFill();
    m_firstFrameChecked = 0;
}

bool DetectLetterbox::GetDetectLetterbox() const
{
    return m_isDetectLetterbox;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
