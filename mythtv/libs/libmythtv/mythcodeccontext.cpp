//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 MythTV Developers <mythtv-dev@mythtv.org>
//
// This is part of MythTV (https://www.mythtv.org)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythcodeccontext.h"
#include "videooutbase.h"
#include "mythplayer.h"
#ifdef USING_VAAPI2
#include "vaapi2context.h"
#endif

extern "C" {
    #include "libavutil/pixfmt.h"
    #include "libavutil/hwcontext.h"
    #include "libavcodec/avcodec.h"
    #include "libavfilter/avfilter.h"
    #include "libavfilter/buffersink.h"
    #include "libavfilter/buffersrc.h"
    #include "libavformat/avformat.h"
    #include "libavutil/opt.h"
    #include "libavutil/buffer.h"
}

#define LOC QString("MythCodecContext: ")

MythCodecContext::MythCodecContext() :
    stream(nullptr),
    buffersink_ctx(nullptr),
    buffersrc_ctx(nullptr),
    filter_graph(nullptr),
    filtersInitialized(false),
    hw_frames_ctx(nullptr),
    player(nullptr),
    ptsUsed(0),
    doublerate(false)
{
    priorPts[0] = 0;
    priorPts[1] = 0;
}

MythCodecContext::~MythCodecContext()
{
    CloseFilters();
}

// static
MythCodecContext *MythCodecContext::createMythCodecContext(MythCodecID codec)
{
    MythCodecContext *mctx = nullptr;
#ifdef USING_VAAPI2
    if (codec_is_vaapi2(codec))
        mctx = new Vaapi2Context();
#else
    Q_UNUSED(codec);
#endif
    if (!mctx)
        mctx = new MythCodecContext();
    return mctx;
}

// static
QStringList MythCodecContext::GetDeinterlacers(QString decodername)
{
    QStringList ret;
#ifdef USING_VAAPI2
    if (decodername == "vaapi2")
    {
        ret.append("vaapi2default");
        ret.append("vaapi2bob");
        ret.append("vaapi2weave");
        ret.append("vaapi2motion_adaptive");
        ret.append("vaapi2motion_compensated");
        ret.append("vaapi2doubleratedefault");
        ret.append("vaapi2doubleratebob");
        ret.append("vaapi2doublerateweave");
        ret.append("vaapi2doubleratemotion_adaptive");
        ret.append("vaapi2doubleratemotion_compensated");

/*
    Explanation of vaapi2 deinterlacing modes.
    "mode", "Deinterlacing mode",
        "default", "Use the highest-numbered (and therefore possibly most advanced) deinterlacing algorithm",
        "bob", "Use the bob deinterlacing algorithm",
        "weave", "Use the weave deinterlacing algorithm",
        "motion_adaptive", "Use the motion adaptive deinterlacing algorithm",
        "motion_compensated", "Use the motion compensated deinterlacing algorithm",

    "rate", "Generate output at frame rate or field rate",
        "frame", "Output at frame rate (one frame of output for each field-pair)",
        "field", "Output at field rate (one frame of output for each field)",

    "auto", "Only deinterlace fields, passing frames through unchanged",
        1 = enabled
        0 = disabled
*/

    }
#else
    Q_UNUSED(decodername);
#endif
    return ret;
}
// static - Find if a deinterlacer is codec-provided
bool MythCodecContext::isCodecDeinterlacer(QString decodername)
{
    return (decodername.startsWith("vaapi2"));
}


// Currently this will only set up the filter after an interlaced frame.
// If we need other filters apart from deinterlace filters we will
// need to make a change here.

int MythCodecContext::FilteredReceiveFrame(AVCodecContext *ctx, AVFrame *frame)
{
    int ret = 0;

    while (1)
    {
        if (filter_graph)
        {
            ret = av_buffersink_get_frame(buffersink_ctx, frame);
            if  (ret >= 0)
            {
                if (priorPts[0] && ptsUsed == priorPts[1])
                {
                    frame->pts = priorPts[1] + (priorPts[1] - priorPts[0])/2;
                    frame->scte_cc_len = 0;
                    frame->atsc_cc_len = 0;
                    av_frame_remove_side_data(frame, AV_FRAME_DATA_A53_CC);
                }
                else
                {
                    frame->pts = priorPts[1];
                    ptsUsed = priorPts[1];
                }
            }
            if  (ret != AVERROR(EAGAIN))
                break;
        }

        // EAGAIN or no filter graph
        ret = avcodec_receive_frame(ctx, frame);
        if (ret < 0)
            break;
        priorPts[0]=priorPts[1];
        priorPts[1]=frame->pts;
        if (frame->interlaced_frame || filter_graph)
        {
            if (!filtersInitialized
              || width != frame->width
              || height != frame->height)
            {
                // bypass any frame of unknown format
                if (frame->format < 0)
                    break;
                ret = InitDeinterlaceFilter(ctx, frame);
                if (ret < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC + "InitDeinterlaceFilter failed - continue without filters");
                    break;
                }
            }
            if (filter_graph)
            {
                ret = av_buffersrc_add_frame(buffersrc_ctx, frame);
                if (ret < 0)
                    break;
            }
            else
                break;
        }
        else
            break;
    }

    return ret;
}

// Setup or change deinterlacer.
// Same usage as VideoOutBase::SetupDeinterlace
// enable - true to enable, false to disable
// name - empty to use video profile deinterlacers, otherwise
//        use the supplied name.
// return true if the deinterlacer was found as a hardware deinterlacer.
// return false if the deinterlacer is nnt a hardware deinterlacer,
//        and a videououtput deinterlacer should be tried instead.

bool MythCodecContext::setDeinterlacer(bool enable, QString name)
{
    QMutexLocker lock(&contextLock);
    // Code to disable interlace
    if (!enable)
    {
        if (deinterlacername.isEmpty())
            return true;
        else
        {
            deinterlacername.clear();
            doublerate = false;
            filtersInitialized = false;
            return true;
        }
    }

    // Code to enable or change interlace
    if (name.isEmpty())
    {
        if (deinterlacername.isEmpty())
        {
            VideoOutput *vo = nullptr;
            VideoDisplayProfile *vdisp_profile = nullptr;
            if (player)
                vo = player->GetVideoOutput();
            if (vo)
                vdisp_profile = vo->GetProfile();
            if (vdisp_profile)
                name = vdisp_profile->GetFilteredDeint(QString());
        }
        else
            name = deinterlacername;
    }
    bool ret = true;
    if (!isCodecDeinterlacer(name))
        name.clear();

    if (name.isEmpty())
        ret = false;

    if (deinterlacername == name)
        return ret;

    deinterlacername = name;
    doublerate = deinterlacername.contains("doublerate");
    filtersInitialized = false;
    return ret;
}

bool MythCodecContext::BestDeint(void)
{
    deinterlacername.clear();
    doublerate = false;
    return setDeinterlacer(true);
}

bool MythCodecContext::FallbackDeint(void)
{
    return setDeinterlacer(true,GetFallbackDeint());
}

QString MythCodecContext::GetFallbackDeint(void)
{

    VideoOutput *vo = nullptr;
    VideoDisplayProfile *vdisp_profile = nullptr;
    if (player)
        vo = player->GetVideoOutput();
    if (vo)
        vdisp_profile = vo->GetProfile();
    if (vdisp_profile)
        return vdisp_profile->GetFallbackDeinterlacer();
    return QString();
}

int MythCodecContext::InitDeinterlaceFilter(AVCodecContext *ctx, AVFrame *frame)
{
    QMutexLocker lock(&contextLock);
    char args[512];
    int ret = 0;
    CloseFilters();
    width = frame->width;
    height = frame->height;
    filtersInitialized = true;
    if (!player || !stream)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Player or stream is not set up in MythCodecContext");
        return -1;
    }
    if (doublerate && !player->CanSupportDoubleRate())
    {
        QString request = deinterlacername;
        deinterlacername = GetFallbackDeint();
        LOG(VB_PLAYBACK, LOG_INFO, LOC
          + QString("Deinterlacer %1 requires double rate, switching to %2 instead.")
          .arg(request).arg(deinterlacername));
        if (!isCodecDeinterlacer(deinterlacername))
            deinterlacername.clear();
        doublerate = deinterlacername.contains("doublerate");

        // if the fallback is a non-vaapi - deinterlace will be turned off
        // and the videoout methods can take over.
    }
    QString filters;
    if (isValidDeinterlacer(deinterlacername))
        filters = GetDeinterlaceFilter();

    if (filters.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "Disabled hardware decoder based deinterlacer.");
        return ret;
    }
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVRational time_base = stream->time_base;
    AVBufferSrcParameters* params = nullptr;

    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph)
    {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            frame->width, frame->height, frame->format, // ctx->pix_fmt,
            time_base.num, time_base.den,
            ctx->sample_aspect_ratio.num, ctx->sample_aspect_ratio.den);

    // isInterlaced = frame->interlaced_frame;

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, nullptr, filter_graph);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "avfilter_graph_create_filter failed for buffer source");
        goto end;
    }

    params = av_buffersrc_parameters_alloc();
    if (hw_frames_ctx)
        av_buffer_unref(&hw_frames_ctx);
    hw_frames_ctx = av_buffer_ref(frame->hw_frames_ctx);
    params->hw_frames_ctx = hw_frames_ctx;

    ret = av_buffersrc_parameters_set(buffersrc_ctx, params);

    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "av_buffersrc_parameters_set failed");
        goto end;
    }

    av_freep(&params);

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       nullptr, nullptr, filter_graph);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "avfilter_graph_create_filter failed for buffer sink");
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = nullptr;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = nullptr;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters.toLocal8Bit(),
                                    &inputs, &outputs,nullptr)) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC
            + QString("avfilter_graph_parse_ptr failed for %1").arg(filters));
        goto end;
    }

    if ((ret = avfilter_graph_config(filter_graph, nullptr)) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC
            + QString("avfilter_graph_config failed"));
        goto end;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Enabled hardware decoder based deinterlace filter '%1': <%2>.")
            .arg(deinterlacername).arg(filters));
end:
    if (ret < 0)
    {
        avfilter_graph_free(&filter_graph);
        filter_graph = nullptr;
        doublerate = false;
    }
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

void MythCodecContext::CloseFilters()
{
    avfilter_graph_free(&filter_graph);
    filter_graph = nullptr;
    buffersink_ctx = nullptr;
    buffersrc_ctx = nullptr;
    filtersInitialized = false;
    ptsUsed = 0;
    priorPts[0] = 0;
    priorPts[1] = 0;
    // isInterlaced = 0;
    width = 0;
    height = 0;

    if (hw_frames_ctx)
        av_buffer_unref(&hw_frames_ctx);
}
