#ifndef CHANNELGROUP_H
#define CHANNELGROUP_H

// c/c++
#include <utility>
#include <vector>
using namespace std;

// qt
#include <QString>
#include <QCoreApplication>

// mythtv
#include "mythtvexp.h"

class MTV_PUBLIC ChannelGroupItem
{
  public:
    ChannelGroupItem(const ChannelGroupItem&)  = default;
    ChannelGroupItem(const uint grpid, QString name) :
        m_grpId(grpid), m_name(std::move(name)) {}

    bool operator == (uint grpid) const
        { return m_grpId == grpid; }

    ChannelGroupItem& operator=(const ChannelGroupItem&) = default;

  public:
    uint    m_grpId;
    QString m_name;
};
using ChannelGroupList = vector<ChannelGroupItem>;

/** \class ChannelGroup
*/
class MTV_PUBLIC ChannelGroup
{
    Q_DECLARE_TR_FUNCTIONS(ChannelGroup);

  public:
    // ChannelGroup 
    static ChannelGroupList  GetChannelGroups(bool includeEmpty = true);
    static bool              ToggleChannel(uint chanid, int changrpid, bool delete_chan);
    static bool              AddChannel(uint chanid, int changrpid);
    static bool              DeleteChannel(uint chanid, int changrpid);
    static int               GetNextChannelGroup(const ChannelGroupList &sorted, int grpid);
    static QString           GetChannelGroupName(int grpid);
    static int               GetChannelGroupId(const QString& changroupname);

  private:

};

#endif
