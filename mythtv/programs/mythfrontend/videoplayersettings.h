#ifndef PLAYERSETTINGS_H
#define PLAYERSETTINGS_H

// libmythui
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuicheckbox.h"
#include "mythscreentype.h"
#include "mythdialogbox.h"

class PlayerSettings : public MythScreenType
{
  Q_OBJECT

  public:

    PlayerSettings(MythScreenStack *parent, const char *name = nullptr);
    ~PlayerSettings() = default;

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private:
    MythUITextEdit   *m_defaultPlayerEdit;
    MythUITextEdit   *m_dvdPlayerEdit;
    MythUITextEdit   *m_dvdDriveEdit;
    MythUITextEdit   *m_blurayMountEdit;
    MythUITextEdit   *m_altPlayerEdit;

    MythUIButtonList *m_blurayRegionList;

    MythUICheckBox   *m_altCheck;

    MythUIButton     *m_okButton;
    MythUIButton     *m_cancelButton;

  private slots:
    void slotSave(void);
    void toggleAlt(void);
    void fillRegionList(void);
};

#endif

