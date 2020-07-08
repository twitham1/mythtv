#ifndef MYTHDRMPRIMEINTEROP_H
#define MYTHDRMPRIMEINTEROP_H

// MythTV
#include "mythegldmabuf.h"
#include "mythopenglinterop.h"

struct AVDRMFrameDescriptor;

class MythDRMPRIMEInterop : public MythOpenGLInterop, public MythEGLDMABUF
{
    friend class MythOpenGLInterop;

  public:
    static MythDRMPRIMEInterop* Create(MythRenderOpenGL *Context, Type InteropType);
    void DeleteTextures(void) override;
    vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context,
                                      VideoColourSpace *ColourSpace,
                                      VideoFrame *Frame, FrameScanType Scan) override;

  protected:
    explicit MythDRMPRIMEInterop(MythRenderOpenGL *Context);
    ~MythDRMPRIMEInterop() override;
    static Type GetInteropType(VideoFrameType Format);

  private:
    AVDRMFrameDescriptor* VerifyBuffer(MythRenderOpenGL *Context, VideoFrame *Frame);
    bool m_deinterlacing { false };
    bool m_composable    { true  };
};

#endif // MYTHDRMPRIMEINTEROP_H
