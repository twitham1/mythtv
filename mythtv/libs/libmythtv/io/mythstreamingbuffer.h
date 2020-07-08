#ifndef STREAMINGRINGBUFFER_H
#define STREAMINGRINGBUFFER_H

// MythTV
#include "io/mythmediabuffer.h"

// FFmpeg
extern "C" {
#include "libavformat/avformat.h"
#include "libavformat/url.h"
}

class MythStreamingBuffer : public MythMediaBuffer
{
  public:
    explicit MythStreamingBuffer(const QString &Filename);
    ~MythStreamingBuffer() override;

    bool      IsOpen            (void) const override;
    long long GetReadPosition   (void) const override;
    bool      OpenFile          (const QString &Filename, uint Retry = static_cast<uint>(kDefaultOpenTimeout)) override;
    bool      IsStreamed        (void) override { return m_streamed;   }
    bool      IsSeekingAllowed  (void) override { return m_allowSeeks; }
    bool      IsBookmarkAllowed (void) override { return false;        }

  protected:
    int       SafeRead          (void *Buffer, uint Size) override;
    long long GetRealFileSizeInternal(void) const override;
    long long SeekInternal      (long long Position, int Whence) override;

  private:
    URLContext *m_context    { nullptr };
    bool        m_streamed   { true    };
    bool        m_allowSeeks { false   };
};

#endif // STREAMINGRINGBUFFER_H
