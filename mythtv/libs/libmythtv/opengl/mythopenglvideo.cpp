// MythTV
#include "mythcontext.h"
#include "tv.h"
#include "opengl/mythrenderopengl.h"
#include "mythavutil.h"
#include "mythopenglvideoshaders.h"
#include "mythopengltonemap.h"
#include "mythopenglvideo.h"

// std
#include <utility>

#define LOC QString("GLVid: ")
#define MAX_VIDEO_TEXTURES 10 // YV12 Kernel deinterlacer + 1

/**
 * \class MythOpenGLVideo
 *
 * MythOpenGLVideo is responsible for displaying video frames on screen using OpenGL.
 * Frames may be sourced from main or graphics memory and MythOpenGLVideo must
 * handle colourspace conversion, deinterlacing and scaling as required.
 *
 * \note MythOpenGLVideo has no knowledge of buffering, timing and other presentation
 * state. Its role is to render video frames on screen.
*/
MythOpenGLVideo::MythOpenGLVideo(MythRenderOpenGL *Render, VideoColourSpace *ColourSpace,
                                 QSize VideoDim, QSize VideoDispDim,
                                 QRect DisplayVisibleRect, QRect DisplayVideoRect, QRect VideoRect,
                                 bool  ViewportControl, QString Profile)
  : m_profile(std::move(Profile)),
    m_render(Render),
    m_videoDispDim(VideoDispDim),
    m_videoDim(VideoDim),
    m_masterViewportSize(DisplayVisibleRect.size()),
    m_displayVideoRect(DisplayVideoRect),
    m_videoRect(VideoRect),
    m_videoColourSpace(ColourSpace),
    m_viewportControl(ViewportControl),
    m_inputTextureSize(m_videoDim)
{
    if (!m_render || !m_videoColourSpace)
        return;

    OpenGLLocker ctx_lock(m_render);
    m_render->IncrRef();
    if (m_render->isOpenGLES())
        m_gles = m_render->format().majorVersion();

    m_videoColourSpace->IncrRef();
    connect(m_videoColourSpace, &VideoColourSpace::Updated, this, &MythOpenGLVideo::UpdateColourSpace);

    // Set OpenGL feature support
    m_features      = m_render->GetFeatures();
    m_extraFeatures = m_render->GetExtraFeatures();
    m_valid = true;

    m_chromaUpsamplingFilter = gCoreContext->GetBoolSetting("ChromaUpsamplingFilter", true);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Chroma upsampling filter %1")
        .arg(m_chromaUpsamplingFilter ? "enabled" : "disabled"));
}

MythOpenGLVideo::~MythOpenGLVideo()
{
    if (m_videoColourSpace)
        m_videoColourSpace->DecrRef();

    if (!m_render)
        return;

    m_render->makeCurrent();
    ResetFrameFormat();
    delete m_toneMap;
    m_render->doneCurrent();
    m_render->DecrRef();
}

bool MythOpenGLVideo::IsValid(void) const
{
    return m_valid;
}

void MythOpenGLVideo::UpdateColourSpace(bool PrimariesChanged)
{
    OpenGLLocker locker(m_render);

    // if input/output type are unset - we haven't created the shaders yet
    if (PrimariesChanged && (m_outputType != FMT_NONE))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Primaries conversion changed - recreating shaders");
        SetupFrameFormat(m_inputType, m_outputType, m_videoDim, m_textureTarget);
    }

    float colourgamma  = m_videoColourSpace->GetColourGamma();
    float displaygamma = 1.0F / m_videoColourSpace->GetDisplayGamma();
    QMatrix4x4 primary = m_videoColourSpace->GetPrimaryMatrix();
    for (int i = Progressive; i < ShaderCount; ++i)
    {
        m_render->SetShaderProgramParams(m_shaders[i], *m_videoColourSpace, "m_colourMatrix");
        m_render->SetShaderProgramParams(m_shaders[i], primary, "m_primaryMatrix");
        if (m_shaders[i])
        {
            m_shaders[i]->setUniformValue("m_colourGamma", colourgamma);
            m_shaders[i]->setUniformValue("m_displayGamma", displaygamma);
        }
    }
}

void MythOpenGLVideo::UpdateShaderParameters(void)
{
    if (m_inputTextureSize.isEmpty())
        return;

    OpenGLLocker locker(m_render);
    bool rect = m_textureTarget == QOpenGLTexture::TargetRectangle;
    GLfloat lineheight = rect ? 1.0F : 1.0F / m_inputTextureSize.height();
    GLfloat maxheight  = rect ? m_videoDispDim.height() : m_videoDispDim.height() / static_cast<GLfloat>(m_inputTextureSize.height());
    GLfloat fieldsize  = rect ? 0.5F : m_inputTextureSize.height() / 2.0F;
    QVector4D parameters(lineheight,                                       /* lineheight */
                         static_cast<GLfloat>(m_inputTextureSize.width()), /* 'Y' select */
                         maxheight - lineheight,                           /* maxheight  */
                         fieldsize                                         /* fieldsize  */);

    for (int i = Progressive; i < ShaderCount; ++i)
    {
        if (m_shaders[i])
        {
            m_render->EnableShaderProgram(m_shaders[i]);
            m_shaders[i]->setUniformValue("m_frameData", parameters);
        }
    }
}

void MythOpenGLVideo::SetMasterViewport(QSize Size)
{
    m_masterViewportSize = Size;
}

void MythOpenGLVideo::SetVideoDimensions(const QSize &VideoDim, const QSize &VideoDispDim)
{
    m_videoDim = VideoDim;
    m_videoDispDim = VideoDispDim;
}

void MythOpenGLVideo::SetVideoRects(const QRect &DisplayVideoRect, const QRect &VideoRect)
{
    m_displayVideoRect = DisplayVideoRect;
    m_videoRect = VideoRect;
}

void MythOpenGLVideo::SetViewportRect(const QRect &DisplayVisibleRect)
{
    SetMasterViewport(DisplayVisibleRect.size());
}

QString MythOpenGLVideo::GetProfile(void) const
{
    if (format_is_hw(m_inputType))
        return TypeToProfile(m_inputType);
    return TypeToProfile(m_outputType);
}

void MythOpenGLVideo::SetProfile(const QString &Profile)
{
    m_profile = Profile;
}

QSize MythOpenGLVideo::GetVideoSize(void) const
{
    return m_videoDim;
}

void MythOpenGLVideo::CleanupDeinterlacers(void)
{
    // If switching off/from basic deinterlacing, then we need to delete and
    // recreate the input textures and sometimes the shaders as well - so start
    // from scratch
    if (m_deinterlacer == DEINT_BASIC && format_is_yuv(m_inputType))
    {
        // Note. Textures will be created with linear filtering - which matches
        // no resizing - which should be the case for the basic deinterlacer - and
        // the call to SetupFrameFormat will reset resizing anyway
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Removing single field textures");
        // revert to YUY2 if preferred
        if ((m_inputType == FMT_YV12) && (m_profile == "opengl"))
            m_outputType = FMT_YUY2;
        SetupFrameFormat(m_inputType, m_outputType, m_videoDim, m_textureTarget);
        emit OutputChanged(m_videoDim, m_videoDim, -1.0F);
    }
    m_fallbackDeinterlacer = DEINT_NONE;
    m_deinterlacer = DEINT_NONE;
    m_deinterlacer2x = false;
}

bool MythOpenGLVideo::AddDeinterlacer(const VideoFrame *Frame, FrameScanType Scan,
                                      MythDeintType Filter  /* = DEINT_SHADER */,
                                      bool CreateReferences /* = true */)
{
    if (!Frame)
        return false;

    // do we want an opengl shader?
    // shaders trump CPU deinterlacers if selected and driver deinterlacers will only
    // be available under restricted circumstances
    // N.B. there should in theory be no situation in which shader deinterlacing is not
    // available for software formats, hence there should be no need to fallback to cpu

    if (!is_interlaced(Scan) || Frame->already_deinterlaced)
    {
        CleanupDeinterlacers();
        return false;
    }

    m_deinterlacer2x = true;
    MythDeintType deinterlacer = GetDoubleRateOption(Frame, Filter);
    MythDeintType other        = GetDoubleRateOption(Frame, DEINT_DRIVER);
    if (other) // another double rate deinterlacer is enabled
    {
        CleanupDeinterlacers();
        return false;
    }

    if (!deinterlacer)
    {
        m_deinterlacer2x = false;
        deinterlacer = GetSingleRateOption(Frame, Filter);
        other        = GetSingleRateOption(Frame, DEINT_DRIVER);
        if (!deinterlacer || other) // no shader deinterlacer needed
        {
            CleanupDeinterlacers();
            return false;
        }
    }

    // if we get this far, we cannot use driver deinterlacers, shader deints
    // are preferred over cpu, we have a deinterlacer but don't actually care whether
    // it is single or double rate
    if (m_deinterlacer == deinterlacer || (m_fallbackDeinterlacer && (m_fallbackDeinterlacer == deinterlacer)))
        return true;

    // Lock
    OpenGLLocker ctx_lock(m_render);

    // delete old reference textures
    MythVideoTexture::DeleteTextures(m_render, m_prevTextures);
    MythVideoTexture::DeleteTextures(m_render, m_nextTextures);

    // For basic deinterlacing of software frames, we now create 2 sets of field
    // based textures - which is the same approach taken by the CPU based onefield/bob
    // deinterlacer and the EGL basic deinterlacer. The advantages of this
    // approach are:-
    //  - no dependent texturing in the samplers (it is just a basic YUV to RGB conversion
    //    in the shader)
    //  - better quality (the onefield shader line doubles but does not have the
    //    implicit interpolation/smoothing of using separate textures directly,
    //    which leads to 'blockiness').
    //  - as we are not sampling other fields, there is no need to use an intermediate
    //    framebuffer to ensure accurate sampling - so we can skip the resize stage.
    //
    // YUYV formats are currently not supported as it does not work correctly - force YV12 instead.

    if (deinterlacer == DEINT_BASIC && format_is_yuv(m_inputType))
    {
        if (m_outputType == FMT_YUY2)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Forcing OpenGL YV12 for basic deinterlacer");
            m_outputType = FMT_YV12;
        }
        MythVideoTexture::DeleteTextures(m_render, m_inputTextures);
        QSize size(m_videoDim.width(), m_videoDim.height() >> 1);
        vector<QSize> sizes;
        sizes.emplace_back(size);
        // N.B. If we are currently resizing, it will be turned off for this
        // deinterlacer, so the default linear texture filtering is OK.
        m_inputTextures = MythVideoTexture::CreateTextures(m_render, m_inputType, m_outputType, sizes);
        // nextTextures will hold the other field
        m_nextTextures = MythVideoTexture::CreateTextures(m_render, m_inputType, m_outputType, sizes);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created %1 single field textures")
            .arg(m_inputTextures.size() * 2));
        // Con VideoOutWindow into display the field only
        emit OutputChanged(m_videoDim, size, -1.0F);
    }

    // sanity check max texture units. Should only be an issue on old hardware (e.g. Pi)
    int max = m_render->GetMaxTextureUnits();
    uint refstocreate = ((deinterlacer == DEINT_HIGH) && CreateReferences) ? 2 : 0;
    int totaltextures = static_cast<int>(planes(m_outputType)) * static_cast<int>(refstocreate + 1);
    if (totaltextures > max)
    {
        m_fallbackDeinterlacer = deinterlacer;
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Insufficent texture units for deinterlacer '%1' (%2 < %3)")
            .arg(DeinterlacerName(deinterlacer | DEINT_SHADER, m_deinterlacer2x)).arg(max).arg(totaltextures));
        deinterlacer = DEINT_BASIC;
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Falling back to '%1'")
            .arg(DeinterlacerName(deinterlacer | DEINT_SHADER, m_deinterlacer2x)));
    }

    // create new deinterlacers - the old ones will be deleted
    if (!(CreateVideoShader(InterlacedBot, deinterlacer) && CreateVideoShader(InterlacedTop, deinterlacer)))
        return false;

    // create the correct number of reference textures
    if (refstocreate)
    {
        vector<QSize> sizes;
        sizes.emplace_back(QSize(m_videoDim));
        m_prevTextures = MythVideoTexture::CreateTextures(m_render, m_inputType, m_outputType, sizes);
        m_nextTextures = MythVideoTexture::CreateTextures(m_render, m_inputType, m_outputType, sizes);
        // ensure we use GL_NEAREST if resizing is already active and needed
        if ((m_resizing & Sampling) == Sampling)
        {
            MythVideoTexture::SetTextureFilters(m_render, m_prevTextures, QOpenGLTexture::Nearest);
            MythVideoTexture::SetTextureFilters(m_render, m_nextTextures, QOpenGLTexture::Nearest);
        }
    }

    // ensure they work correctly
    UpdateColourSpace(false);
    UpdateShaderParameters();
    m_deinterlacer = deinterlacer;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created deinterlacer '%1' (%2->%3)")
        .arg(DeinterlacerName(m_deinterlacer | DEINT_SHADER, m_deinterlacer2x))
        .arg(format_description(m_inputType)).arg(format_description(m_outputType)));
    return true;
}

/*! \brief Create the appropriate shader for the operation Type
 *
 * \note Shader cost is calculated as 1 per texture read and 2 per dependent read.
 * If there are alternative shader conditions, the worst case is used.
 * (A dependent read is defined as any texture read that does not use the exact
 * texture coordinates passed into the fragment shader)
*/
bool MythOpenGLVideo::CreateVideoShader(VideoShaderType Type, MythDeintType Deint)
{
    if (!m_render || !(m_features & QOpenGLFunctions::Shaders))
        return false;

    // delete the old
    if (m_shaders[Type])
        m_render->DeleteShaderProgram(m_shaders[Type]);
    m_shaders[Type] = nullptr;

    QStringList defines;
    QString vertex = DefaultVertexShader;
    QString fragment;
    int cost = 1;

    if (m_textureTarget == GL_TEXTURE_EXTERNAL_OES)
        defines << "EXTOES";

    if ((Default == Type) || (!format_is_yuv(m_outputType)))
    {
        QString glsldefines;
        for (const QString& define : qAsConst(defines))
            glsldefines += QString("#define MYTHTV_%1\n").arg(define);
        fragment = glsldefines + YUVFragmentExtensions + RGBFragmentShader;

#ifdef USING_MEDIACODEC
        if (FMT_MEDIACODEC == m_inputType)
            vertex = MediaCodecVertexShader;
#endif
    }
    // no interlaced shaders yet (i.e. interlaced chroma upsampling - not deinterlacers)
    else
    {
        fragment = YUVFragmentShader;
        QString extensions = YUVFragmentExtensions;
        QString glsldefines;

        // Any software frames that are not 8bit need to use unsigned integer
        // samplers with GLES3.x - which need more modern shaders
        if ((m_gles > 2) && (ColorDepth(m_inputType) > 8))
        {
            static const QString glsl300("#version 300 es\n");
            fragment   = GLSL300YUVFragmentShader;
            extensions = GLSL300YUVFragmentExtensions;
            vertex     = glsl300 + GLSL300VertexShader;
            glsldefines.append(glsl300);
        }

        bool kernel = false;
        bool topfield = InterlacedTop == Type;
        bool progressive = (Progressive == Type) || (Deint == DEINT_NONE);
        if (format_is_420(m_outputType) || format_is_422(m_outputType) || format_is_444(m_outputType))
        {
            defines << "YV12";
            cost = 3;
        }
        else if (format_is_nv12(m_outputType))
        {
            defines << "NV12";
            cost = 2;
        }
        else if (FMT_YUY2 == m_outputType)
        {
            defines << "YUY2";
        }

#ifdef USING_VTB
        // N.B. Rectangular texture support is only currently used for VideoToolBox
        // video frames which are NV12. Do not use rectangular textures for the 'default'
        // shaders as it breaks video resizing and would require changes to our
        // FramebufferObject code.
        if ((m_textureTarget == QOpenGLTexture::TargetRectangle) && (Default != Type))
            defines << "RECTS";
#endif
        if (!progressive)
        {
            bool basic = Deint == DEINT_BASIC && format_is_yuv(m_inputType);
            // Chroma upsampling filter
            if ((format_is_420(m_outputType) || format_is_nv12(m_outputType)) &&
                m_chromaUpsamplingFilter && !basic)
            {
                defines << "CUE";
            }

            // field
            if (topfield && !basic)
                defines << "TOPFIELD";

            switch (Deint)
            {
                case DEINT_BASIC:
                if (!basic)
                {
                    cost *= 2;
                    defines << "ONEFIELD";
                }
                break;
                case DEINT_MEDIUM: cost *= 5;  defines << "LINEARBLEND"; break;
                case DEINT_HIGH:   cost *= 15; defines << "KERNEL"; kernel = true; break;
                default: break;
            }
        }

        // Start building the new fragment shader
        // We do this in code otherwise the shader string becomes monolithic

        // 'expand' calls to sampleYUV for multiple planes
        // do this before we add the samplers
        int count = static_cast<int>(planes(m_outputType));
        for (int i = (kernel ? 2 : 0); (i >= 0) && count; i--)
        {
            QString find = QString("s_texture%1").arg(i);
            QStringList replacelist;
            for (int j = (i * count); j < ((i + 1) * count); ++j)
                replacelist << QString("s_texture%1").arg(j);
            fragment.replace(find, replacelist.join(", "));
        }

        // 'expand' calls to the kernel function
        if (kernel && count)
        {
            for (int i = 1 ; i >= 0; i--)
            {
                QString find1 = QString("sampler2D kernelTex%1").arg(i);
                QString find2 = QString("kernelTex%1").arg(i);
                QStringList replacelist1;
                QStringList replacelist2;
                for (int j = 0; j < count; ++j)
                {
                    replacelist1 << QString("sampler2D kernelTexture%1%2").arg(i).arg(j);
                    replacelist2 << QString("kernelTexture%1%2").arg(i).arg(j);
                }
                fragment.replace(find1, replacelist1.join(", "));
                fragment.replace(find2, replacelist2.join(", "));
            }
        }

        // Retrieve colour mappping defines
        defines << m_videoColourSpace->GetColourMappingDefines();

        // Add defines
        for (const QString& define : qAsConst(defines))
            glsldefines += QString("#define MYTHTV_%1\n").arg(define);

        // Add the required samplers
        int start = 0;
        int end   = count;
        if (kernel)
        {
            end *= 3;
            if (topfield)
                start += count;
            else
                end -= count;
        }
        QString glslsamplers;
        for (int i = start; i < end; ++i)
            glslsamplers += QString("uniform sampler2D s_texture%1;\n").arg(i);

        // construct the final shader string
        fragment = glsldefines + extensions + glslsamplers + fragment;
    }

    m_shaderCost[Type] = cost;
    QOpenGLShaderProgram *program = m_render->CreateShaderProgram(vertex, fragment);
    if (!program)
        return false;

    m_shaders[Type] = program;
    return true;
}

bool MythOpenGLVideo::SetupFrameFormat(VideoFrameType InputType, VideoFrameType OutputType,
                                       QSize Size, GLenum TextureTarget)
{
    QString texnew = (TextureTarget == QOpenGLTexture::TargetRectangle) ? "Rect" :
                     (TextureTarget == GL_TEXTURE_EXTERNAL_OES) ? "OES" : "2D";
    QString texold = (m_textureTarget == QOpenGLTexture::TargetRectangle) ? "Rect" :
                     (m_textureTarget == GL_TEXTURE_EXTERNAL_OES) ? "OES" : "2D";
    LOG(VB_GENERAL, LOG_WARNING, LOC +
        QString("New frame format: %1:%2 %3x%4 (Tex: %5) -> %6:%7 %8x%9 (Tex: %10)")
        .arg(format_description(m_inputType)).arg(format_description(m_outputType))
        .arg(m_videoDim.width()).arg(m_videoDim.height()).arg(texold)
        .arg(format_description(InputType)).arg(format_description(OutputType))
        .arg(Size.width()).arg(Size.height()).arg(texnew));

    ResetFrameFormat();

    m_inputType = InputType;
    m_outputType = OutputType;
    m_textureTarget = TextureTarget;
    m_videoDim = Size;

    // This is only currently needed for RGBA32 frames from composed DRM_PRIME
    // textures that may be half height for simple bob deinterlacing
    if (m_inputType == FMT_DRMPRIME && m_outputType == FMT_RGBA32)
        emit OutputChanged(m_videoDim, m_videoDim, -1.0F);

    if (!format_is_hw(InputType))
    {
        vector<QSize> sizes;
        sizes.push_back(Size);
        m_inputTextures = MythVideoTexture::CreateTextures(m_render, m_inputType, m_outputType, sizes);
        if (m_inputTextures.empty())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create input textures");
            return false;
        }

        m_inputTextureSize = m_inputTextures[0]->m_totalSize;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created %1 input textures for '%2'")
            .arg(m_inputTextures.size()).arg(GetProfile()));
    }
    else
    {
        m_inputTextureSize = Size;
    }

    // Create shaders
    if (!CreateVideoShader(Default) || !CreateVideoShader(Progressive))
        return false;

    UpdateColourSpace(false);
    UpdateShaderParameters();
    return true;
}

void MythOpenGLVideo::ResetFrameFormat(void)
{
    for (auto & shader : m_shaders)
        if (shader)
            m_render->DeleteShaderProgram(shader);
    memset(m_shaders, 0, sizeof(m_shaders));
    memset(m_shaderCost, 1, sizeof(m_shaderCost));
    MythVideoTexture::DeleteTextures(m_render, m_inputTextures);
    MythVideoTexture::DeleteTextures(m_render, m_prevTextures);
    MythVideoTexture::DeleteTextures(m_render, m_nextTextures);
    m_inputType = FMT_NONE;
    m_outputType = FMT_NONE;
    m_textureTarget = QOpenGLTexture::Target2D;
    m_inputTextureSize = QSize();
    m_deinterlacer = DEINT_NONE;
    m_fallbackDeinterlacer = DEINT_NONE;
    m_render->DeleteFramebuffer(m_frameBuffer);
    m_render->DeleteTexture(m_frameBufferTexture);
    m_frameBuffer = nullptr;
    m_frameBufferTexture = nullptr;
    // textures are created with Linear filtering - which matches no resize
    m_resizing = None;
}

/// \brief Update the current input texture using the data from the given video frame.
void MythOpenGLVideo::ProcessFrame(VideoFrame *Frame, FrameScanType Scan)
{
    if (Frame->codec == FMT_NONE)
        return;

    // Hardware frames are retrieved/updated in PrepareFrame but we need to
    // reset software frames now if necessary
    if (format_is_hw(Frame->codec))
    {
        if ((!format_is_hw(m_inputType)) && (m_inputType != FMT_NONE))
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Resetting input format");
            ResetFrameFormat();
        }
        return;
    }

    // Sanitise frame
    if ((Frame->width < 1) || (Frame->height < 1) || !Frame->buf)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid software frame");
        return;
    }

    // Can we render this frame format
    if (!format_is_yuv(Frame->codec))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Frame format is not supported");
        return;
    }

    // lock
    OpenGLLocker ctx_lock(m_render);

    // check for input changes
    if ((Frame->width  != m_videoDim.width()) ||
        (Frame->height != m_videoDim.height()) ||
        (Frame->codec  != m_inputType))
    {
        VideoFrameType frametype = Frame->codec;
        if ((frametype == FMT_YV12) && (m_profile == "opengl"))
            frametype = FMT_YUY2;
        QSize size(Frame->width, Frame->height);
        if (!SetupFrameFormat(Frame->codec, frametype, size, QOpenGLTexture::Target2D))
            return;
    }

    // Setup deinterlacing if required
    AddDeinterlacer(Frame, Scan);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "UPDATE_FRAME_START");

    m_videoColourSpace->UpdateColourSpace(Frame);

    // Rotate textures if necessary
    bool current = true;
    if (m_deinterlacer == DEINT_MEDIUM || m_deinterlacer == DEINT_HIGH)
    {
        if (!m_nextTextures.empty() && !m_prevTextures.empty())
        {
            if (abs(Frame->frameCounter - m_discontinuityCounter) > 1)
                ResetTextures();
            vector<MythVideoTexture*> temp = m_prevTextures;
            m_prevTextures = m_inputTextures;
            m_inputTextures = m_nextTextures;
            m_nextTextures = temp;
            current = false;
        }
    }

    m_discontinuityCounter = Frame->frameCounter;

    if (m_deinterlacer == DEINT_BASIC)
    {
        // first field. Fake the pitches
        int pitches[3];
        memcpy(pitches, Frame->pitches, sizeof(int) * 3);
        Frame->pitches[0] = Frame->pitches[0] << 1;
        Frame->pitches[1] = Frame->pitches[1] << 1;
        Frame->pitches[2] = Frame->pitches[2] << 1;
        MythVideoTexture::UpdateTextures(m_render, Frame, m_inputTextures);
        // second field. Fake the offsets as well.
        int offsets[3];
        memcpy(offsets, Frame->offsets, sizeof(int) * 3);
        Frame->offsets[0] = Frame->offsets[0] + pitches[0];
        Frame->offsets[1] = Frame->offsets[1] + pitches[1];
        Frame->offsets[2] = Frame->offsets[2] + pitches[2];
        MythVideoTexture::UpdateTextures(m_render, Frame, m_nextTextures);
        memcpy(Frame->pitches, pitches, sizeof(int) * 3);
        memcpy(Frame->offsets, offsets, sizeof(int) * 3);
    }
    else
    {
        MythVideoTexture::UpdateTextures(m_render, Frame, current ? m_inputTextures : m_nextTextures);
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "UPDATE_FRAME_END");
}

void MythOpenGLVideo::PrepareFrame(VideoFrame *Frame, bool TopFieldFirst, FrameScanType Scan,
                                   StereoscopicMode Stereo, bool DrawBorder)
{
    if (!m_render)
        return;

    OpenGLLocker locker(m_render);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PREP_FRAME_START");

    // Set required input textures for the last stage
    // ProcessFrame is always called first, which will create/destroy software
    // textures as needed
    bool hwframes = false;
    bool useframebufferimage = false;

    // for tiled renderers (e.g. Pi), enable scissoring to try and improve performance
    // when not full screen and avoid the resize stage unless absolutely necessary
    bool tiled = m_extraFeatures & kGLTiled;

    // We lose the pause frame when seeking and using VDPAU/VAAPI/NVDEC direct rendering.
    // As a workaround, we always use the resize stage so the last displayed frame
    // should be retained in the Framebuffer used for resizing. If there is
    // nothing to display, then fallback to this framebuffer.
    // N.B. this is now strictly necessary with v4l2 and DRM PRIME direct rendering
    // but ignore now for performance reasons
    VideoResizing resize = Frame ? (format_is_hwframes(Frame->codec) ? Framebuffer : None) :
                                   (format_is_hwframes(m_inputType)  ? Framebuffer : None);

    vector<MythVideoTexture*> inputtextures = m_inputTextures;
    if (inputtextures.empty())
    {
        // Pull in any hardware frames
        inputtextures = MythOpenGLInterop::Retrieve(m_render, m_videoColourSpace, Frame, Scan);
        if (!inputtextures.empty())
        {
            hwframes = true;
            QSize newsize = inputtextures[0]->m_size;
            VideoFrameType newsourcetype = inputtextures[0]->m_frameType;
            VideoFrameType newtargettype = inputtextures[0]->m_frameFormat;
            GLenum newtargettexture = inputtextures[0]->m_target;
            if ((m_outputType != newtargettype) || (m_textureTarget != newtargettexture) ||
                (m_inputType != newsourcetype) || (newsize != m_inputTextureSize))
            {
                SetupFrameFormat(newsourcetype, newtargettype, newsize, newtargettexture);
            }

#ifdef USING_MEDIACODEC
            // Set the texture transform for mediacodec
            if (FMT_MEDIACODEC == m_inputType)
            {
                if (inputtextures[0]->m_transform && m_shaders[Default])
                {
                    m_render->EnableShaderProgram(m_shaders[Default]);
                    m_shaders[Default]->setUniformValue("u_transform", *inputtextures[0]->m_transform);
                }
            }
#endif
            // Enable deinterlacing for NVDEC, VTB and VAAPI DRM if VPP is not available
            if (inputtextures[0]->m_allowGLSLDeint)
                AddDeinterlacer(Frame, Scan, DEINT_SHADER | DEINT_CPU, false); // pickup shader or cpu prefs
        }
        else
        {
            if ((resize == Framebuffer) && m_frameBuffer && m_frameBufferTexture)
            {
                LOG(VB_PLAYBACK, LOG_DEBUG, "Using existing framebuffer");
                useframebufferimage = true;
            }
            else
            {
                LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "Nothing to display");
                // if this is live tv startup and the window rect has changed we
                // must set the viewport
                m_render->SetViewPort(QRect(QPoint(), m_masterViewportSize));
                return;
            }
        }
    }

    // Determine which shader to use. This helps optimise the resize check.
    bool deinterlacing = false;
    bool basicdeinterlacing = false;
    VideoShaderType program = format_is_yuv(m_outputType) ? Progressive :  Default;
    if (m_deinterlacer != DEINT_NONE)
    {
        if (Scan == kScan_Interlaced)
        {
            program = TopFieldFirst ? InterlacedTop : InterlacedBot;
            deinterlacing = true;
        }
        else if (Scan == kScan_Intr2ndField)
        {
            program = TopFieldFirst ? InterlacedBot : InterlacedTop;
            deinterlacing = true;
        }

        // select the correct field for the basic deinterlacer
        if (deinterlacing && m_deinterlacer == DEINT_BASIC && format_is_yuv(m_inputType))
        {
            basicdeinterlacing = true;
            if (program == InterlacedBot)
                inputtextures = m_nextTextures;
        }
    }

    // Set deinterlacer type for debug OSD
    if (deinterlacing && Frame)
    {
        Frame->deinterlace_inuse = m_deinterlacer | DEINT_SHADER;
        Frame->deinterlace_inuse2x = m_deinterlacer2x;
    }

    // Tonemapping can only render to a texture
    if (m_toneMap)
        resize |= ToneMap;

    // Decide whether to use render to texture - for performance or quality
    if (format_is_yuv(m_outputType) && !resize)
    {
        // ensure deinterlacing works correctly when down scaling in height
        // N.B. not needed for the basic deinterlacer
        if (deinterlacing && !basicdeinterlacing && (m_videoDispDim.height() > m_displayVideoRect.height()))
            resize |= Deinterlacer;

        // NB GL_NEAREST introduces some 'minor' chroma sampling errors
        // for the following 2 cases. For YUY2 this may be better handled in the
        // shader. For GLES3.0 10bit textures - Vulkan is probably the better solution.

        // UYVY packed pixels must be sampled exactly with GL_NEAREST
        if (FMT_YUY2 == m_outputType)
            resize |= Sampling;
        // unsigned integer texture formats need GL_NEAREST sampling
        if ((m_gles > 2) && (ColorDepth(m_inputType) > 8))
            resize |= Sampling;

        // don't enable resizing if the cost of a framebuffer switch may be
        // prohibitive (e.g. Raspberry Pi/tiled renderers) or for basic deinterlacing,
        // where we are trying to simplifiy/optimise rendering (and the framebuffer
        // sizing gets confused by the change to m_videoDispDim)
        if (!resize && !tiled && !basicdeinterlacing)
        {
            // improve performance. This is an educated guess on the relative cost
            // of render to texture versus straight rendering.
            int totexture    = m_videoDispDim.width() * m_videoDispDim.height() * m_shaderCost[program];
            int blitcost     = m_displayVideoRect.width() * m_displayVideoRect.height() * m_shaderCost[Default];
            int noresizecost = m_displayVideoRect.width() * m_displayVideoRect.height() * m_shaderCost[program];
            if ((totexture + blitcost) < noresizecost)
                resize |= Performance;
        }
    }

    // set software frame filtering if resizing has changed
    if (!resize && m_resizing)
    {
        // remove framebuffer
        if (m_frameBufferTexture)
        {
            m_render->DeleteTexture(m_frameBufferTexture);
            m_frameBufferTexture = nullptr;
        }
        if (m_frameBuffer)
        {
            m_render->DeleteFramebuffer(m_frameBuffer);
            m_frameBuffer = nullptr;
        }
        // set filtering
        MythVideoTexture::SetTextureFilters(m_render, m_inputTextures, QOpenGLTexture::Linear);
        MythVideoTexture::SetTextureFilters(m_render, m_prevTextures, QOpenGLTexture::Linear);
        MythVideoTexture::SetTextureFilters(m_render, m_nextTextures, QOpenGLTexture::Linear);
        m_resizing = None;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabled resizing");
    }
    else if (!m_resizing && resize)
    {
        // framebuffer will be created as needed below
        QOpenGLTexture::Filter filter = ((resize & Sampling) == Sampling) ? QOpenGLTexture::Nearest : QOpenGLTexture::Linear;
        MythVideoTexture::SetTextureFilters(m_render, m_inputTextures, filter);
        MythVideoTexture::SetTextureFilters(m_render, m_prevTextures, filter);
        MythVideoTexture::SetTextureFilters(m_render, m_nextTextures, filter);
        m_resizing = resize;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Resizing from %1x%2 to %3x%4 for %5")
            .arg(m_videoDispDim.width()).arg(m_videoDispDim.height())
            .arg(m_displayVideoRect.width()).arg(m_displayVideoRect.height())
            .arg(VideoResizeToString(resize)));
    }

    // check hardware frames have the correct filtering
    if (hwframes)
    {
        QOpenGLTexture::Filter filter = ((resize & Sampling) == Sampling) ? QOpenGLTexture::Nearest : QOpenGLTexture::Linear;
        if (inputtextures[0]->m_filter != filter)
            MythVideoTexture::SetTextureFilters(m_render, inputtextures, filter);
    }

    // texture coordinates
    QRect trect(m_videoRect);

    if (resize)
    {
        MythVideoTexture* nexttexture = nullptr;

        // only render to the framebuffer if there is something to update
        if (useframebufferimage)
        {
            if (m_toneMap)
            {
                nexttexture = m_toneMap->GetTexture();
                trect = QRect(QPoint(0, 0), m_displayVideoRect.size());
            }
            else
            {
                nexttexture = m_frameBufferTexture;
            }
        }
        else if (m_toneMap)
        {
            if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
                m_render->logDebugMarker(LOC + "RENDER_TO_TEXTURE");
            nexttexture = m_toneMap->Map(inputtextures, m_displayVideoRect.size());
            trect = QRect(QPoint(0, 0), m_displayVideoRect.size());
        }
        else
        {
            // render to texture stage
            if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
                m_render->logDebugMarker(LOC + "RENDER_TO_TEXTURE");

            // we need a framebuffer
            if (!m_frameBuffer)
            {
                m_frameBuffer = CreateVideoFrameBuffer(m_outputType, m_videoDispDim);
                if (!m_frameBuffer)
                    return;
            }

            // and its associated texture
            if (!m_frameBufferTexture)
            {
                m_frameBufferTexture = reinterpret_cast<MythVideoTexture*>(m_render->CreateFramebufferTexture(m_frameBuffer));
                if (!m_frameBufferTexture)
                    return;
                m_render->SetTextureFilters(m_frameBufferTexture, QOpenGLTexture::Linear);
            }

            // coordinates
            QRect vrect(QPoint(0, 0), m_videoDispDim);
            QRect trect2 = vrect;
            if (FMT_YUY2 == m_outputType)
                trect2.setWidth(m_videoDispDim.width() >> 1);

            // framebuffer
            m_render->BindFramebuffer(m_frameBuffer);
            m_render->SetViewPort(vrect);

            // bind correct textures
            MythGLTexture* textures[MAX_VIDEO_TEXTURES];
            uint numtextures = 0;
            BindTextures(deinterlacing, inputtextures, &textures[0], numtextures);

            // render
            m_render->DrawBitmap(textures, numtextures, m_frameBuffer,
                                 trect2, vrect, m_shaders[program], 0);
            nexttexture = m_frameBufferTexture;
        }

        // reset for next stage
        inputtextures.clear();
        inputtextures.push_back(nexttexture);
        program = Default;
        deinterlacing = false;
    }

    // render to default framebuffer/screen
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "RENDER_TO_SCREEN");

    // discard stereoscopic fields
    if (kStereoscopicModeSideBySideDiscard == Stereo)
        trect = QRect(trect.left() >> 1, trect.top(), trect.width() >> 1, trect.height());
    else if (kStereoscopicModeTopAndBottomDiscard == Stereo)
        trect = QRect(trect.left(), trect.top() >> 1, trect.width(), trect.height() >> 1);

    // bind default framebuffer
    m_render->BindFramebuffer(nullptr);
    m_render->SetViewPort(QRect(QPoint(), m_masterViewportSize));

    // PiP border
    if (DrawBorder)
    {
        QRect piprect = m_displayVideoRect.adjusted(-10, -10, +10, +10);
        static const QPen kNopen(Qt::NoPen);
        static const QBrush kRedBrush(QBrush(QColor(127, 0, 0, 255)));
        m_render->DrawRect(nullptr, piprect, kRedBrush, kNopen, 255);
    }

    // bind correct textures
    MythGLTexture* textures[MAX_VIDEO_TEXTURES];
    uint numtextures = 0;
    BindTextures(deinterlacing, inputtextures, &textures[0], numtextures);

    // rotation
    if (Frame)
        m_lastRotation = Frame->rotation;

    // apply scissoring
    if (tiled)
    {
        // N.B. It's not obvious whether this helps
        m_render->glEnable(GL_SCISSOR_TEST);
        m_render->glScissor(m_displayVideoRect.left() - 1, m_displayVideoRect.top() - 1,
                            m_displayVideoRect.width() + 2, m_displayVideoRect.height() + 2);
    }

    // draw
    m_render->DrawBitmap(textures, numtextures, nullptr, trect,
                         m_displayVideoRect, m_shaders[program], m_lastRotation);

    // disable scissoring
    if (tiled)
        m_render->glDisable(GL_SCISSOR_TEST);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PREP_FRAME_END");
}

/// \brief Clear reference frames after a seek as they will contain old images.
void MythOpenGLVideo::ResetTextures(void)
{
    for (auto & texture : m_inputTextures)
        texture->m_valid = false;
    for (auto & texture : m_prevTextures)
        texture->m_valid = false;
    for (auto & texture : m_nextTextures)
        texture->m_valid = false;
}

void MythOpenGLVideo::BindTextures(bool Deinterlacing, vector<MythVideoTexture*> &Current,
                                   MythGLTexture **Textures, uint &TextureCount)
{
    bool usecurrent = true;
    if (Deinterlacing)
    {
        if (format_is_hw(m_inputType))
        {
            usecurrent = true;
        }
        else if ((m_nextTextures.size() == Current.size()) && (m_prevTextures.size() == Current.size()))
        {
            // if we are using reference frames, we want the current frame in the middle
            // but next will be the first valid, followed by current...
            usecurrent = false;
            size_t count = Current.size();
            vector<MythVideoTexture*> &current = Current[0]->m_valid ? Current : m_nextTextures;
            vector<MythVideoTexture*> &prev    = m_prevTextures[0]->m_valid ? m_prevTextures : current;

            for (uint i = 0; i < count; ++i)
                Textures[TextureCount++] = reinterpret_cast<MythGLTexture*>(prev[i]);
            for (uint i = 0; i < count; ++i)
                Textures[TextureCount++] = reinterpret_cast<MythGLTexture*>(current[i]);
            for (uint i = 0; i < count; ++i)
                Textures[TextureCount++] = reinterpret_cast<MythGLTexture*>(m_nextTextures[i]);
        }
    }

    if (usecurrent)
        for (auto & texture : Current)
            Textures[TextureCount++] = reinterpret_cast<MythGLTexture*>(texture);
}

QString MythOpenGLVideo::TypeToProfile(VideoFrameType Type)
{
    if (format_is_hw(Type))
        return "opengl-hw";

    switch (Type)
    {
        case FMT_YUY2:    return "opengl"; // compatibility with old profiles
        case FMT_YV12:    return "opengl-yv12";
        case FMT_NV12:    return "opengl-nv12";
        default: break;
    }
    return "opengl";
}

QString MythOpenGLVideo::VideoResizeToString(VideoResizing Resize)
{
    QStringList reasons;
    if ((Resize & Deinterlacer) == Deinterlacer) reasons << "Deinterlacer";
    if ((Resize & Sampling)     == Sampling)     reasons << "Sampling";
    if ((Resize & Performance)  == Performance)  reasons << "Performance";
    if ((Resize & Framebuffer)  == Framebuffer)  reasons << "Framebuffer";
    return reasons.join(",");
}

QOpenGLFramebufferObject* MythOpenGLVideo::CreateVideoFrameBuffer(VideoFrameType OutputType, QSize Size)
{
    // Use a 16bit float framebuffer if necessary and available (not GLES2) to maintain precision.
    // The depth check will pick up all software formats as well as NVDEC, VideoToolBox and VAAPI DRM.
    // VAAPI GLXPixmap and GLXCopy are currently not 10/12bit aware and VDPAU has no 10bit support -
    // and all return RGB formats anyway. The MediaCoded texture format is an unknown but resizing will
    // never be enabled as it returns an RGB frame - so if MediaCodec uses a 16bit texture, precision
    // will be preserved.
    bool sixteenbitfb  = m_extraFeatures & kGL16BitFBO;
    bool sixteenbitvid = ColorDepth(OutputType) > 8;
    if (sixteenbitfb && sixteenbitvid)
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Requesting 16bit framebuffer texture");
    return m_render->CreateFramebuffer(Size, sixteenbitfb && sixteenbitvid);
}
