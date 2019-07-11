#ifndef FILETRANSFER_H_
#define FILETRANSFER_H_

// C++ headers
#include <cstdint>
#include <vector>
using namespace std;

// Qt headers
#include <QMutex>
#include <QWaitCondition>

// MythTV headers
#include "referencecounter.h"

class ProgramInfo;
class RingBuffer;
class MythSocket;
class QString;

class FileTransfer : public ReferenceCounter
{
    friend class QObject; // quiet OSX gcc warning

  public:
    FileTransfer(QString &filename, MythSocket *remote,
                 bool usereadahead, int timeout_ms);
    FileTransfer(QString &filename, MythSocket *remote, bool write);

    MythSocket *getSocket() { return m_sock; }

    bool isOpen(void);
    bool ReOpen(QString newFilename = "");

    void Stop(void);

    void Pause(void);
    void Unpause(void);
    int RequestBlock(int size);
    int WriteBlock(int size);

    long long Seek(long long curpos, long long pos, int whence);

    uint64_t GetFileSize(void);
    QString GetFileName(void);

    void SetTimeout(bool fast);

  private:
   ~FileTransfer();

    volatile bool   m_readthreadlive    {true};
    bool            m_readsLocked       {false};
    QWaitCondition  m_readsUnlockedCond;

    ProgramInfo    *m_pginfo            {nullptr};
    RingBuffer     *m_rbuffer           {nullptr};
    MythSocket     *m_sock              {nullptr};
    bool            m_ateof             {false};

    vector<char>    m_requestBuffer;

    QMutex          m_lock              {QMutex::NonRecursive};

    bool            m_writemode         {false};
};

#endif
