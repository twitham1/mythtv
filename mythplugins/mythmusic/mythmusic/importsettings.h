#ifndef IMPORTSETTINGS_H
#define IMPORTSETTINGS_H

#include <mythscreentype.h>
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythuicheckbox.h>
#include <mythuitext.h>
#include <mythuitextedit.h>

class ImportSettings : public MythScreenType
{
    Q_OBJECT
  public:
    ImportSettings(MythScreenStack *parent, const char *name = nullptr);
    ~ImportSettings() = default;

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private:
    MythUIButtonList   *m_paranoiaLevel;
    MythUITextEdit     *m_filenameTemplate;
    MythUICheckBox     *m_noWhitespace;
    MythUITextEdit     *m_postCDRipScript;
    MythUICheckBox     *m_ejectCD;
    MythUIButtonList   *m_encoderType;
    MythUIButtonList   *m_defaultRipQuality;
    MythUICheckBox     *m_mp3UseVBR;
    MythUIButton       *m_saveButton;
    MythUIButton       *m_cancelButton;

  private slots:
    void slotSave(void);

};

#endif // IMPORTSETTINGS_H
