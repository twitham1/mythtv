#ifndef CHANNELEDITOR_H
#define CHANNELEDITOR_H

#include "mythscreentype.h"

#include "standardsettings.h"

class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIProgressDialog;

class ChannelEditor : public MythScreenType
{
    Q_OBJECT
  public:
    explicit ChannelEditor(MythScreenStack *parent);

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

  protected slots:
    void menu(void);
    void del(void);
    void edit(MythUIButtonListItem *item = nullptr);
    void scan(void);
    void transportEditor(void);
    void channelIconImport(void);
    void deleteChannels(void);
    void setSortMode(MythUIButtonListItem *item);
    void setSourceID(MythUIButtonListItem *item);
    void setHideMode(bool hide);
    void fillList();

  private slots:
    void itemChanged(MythUIButtonListItem *item);

  private:
    enum sourceFilter {
        FILTER_ALL = -1,
        FILTER_UNASSIGNED = 0
    };

    int     m_sourceFilter           {FILTER_ALL};
    QString m_sourceFilterName;
    QString m_currentSortMode;
    bool    m_currentHideMode        {false};

    MythUIButtonList *m_channelList  {nullptr};
    MythUIButtonList *m_sourceList   {nullptr};

    MythUIImage      *m_preview      {nullptr};
    MythUIText       *m_channame     {nullptr};
    MythUIText       *m_channum      {nullptr};
    MythUIText       *m_callsign     {nullptr};
    MythUIText       *m_chanid       {nullptr};
    MythUIText       *m_sourcename   {nullptr};
    MythUIText       *m_compoundname {nullptr};
};

class ChannelID;

class ChannelWizard : public GroupSetting
{
    Q_OBJECT
  public:
    ChannelWizard(int id, int default_sourceid);

  private:
    ChannelID *m_cid {nullptr};
};

#endif
