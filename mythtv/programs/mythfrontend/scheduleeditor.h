#ifndef SCHEDULERECORDING_H_
#define SCHEDULERECORDING_H_

#include "mythscreentype.h"
#include "schedulecommon.h"

// libmythbase
#include "mythdb.h"

// libmyth
#include "mythcontext.h"

// libmythtv
#include "recordingrule.h"
#include "recordinginfo.h"

// libmythmetadata
#include "metadatafactory.h"

class ProgramInfo;
class MythUIText;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIStateType;
class MythUISpinBox;
class TV;

class ScheduleEditor;
class SchedEditChild;

class SchedOptMixin
{
  protected:
    SchedOptMixin(MythScreenType &screen, RecordingRule *rule,
                  SchedOptMixin *other = nullptr);
    void SetRule(RecordingRule *rule) { m_rule = rule; };
    void Create(bool *err);
    void Load(void);
    void Save(void);
    void RuleChanged(void);
    void DupMethodChanged(MythUIButtonListItem *item);

    MythUISpinBox    *m_prioritySpin;
    MythUISpinBox    *m_startoffsetSpin;
    MythUISpinBox    *m_endoffsetSpin;
    MythUIButtonList *m_dupmethodList;
    MythUIButtonList *m_dupscopeList;
    MythUIButtonList *m_inputList;
    MythUICheckBox   *m_ruleactiveCheck;
    MythUIButtonList *m_newrepeatList;

  private:
    MythScreenType   *m_screen;
    RecordingRule    *m_rule;
    SchedOptMixin    *m_other;
    bool              m_loaded;
    bool              m_haveRepeats;
};

class StoreOptMixin
{
  protected:
    StoreOptMixin(MythScreenType &screen, RecordingRule *rule,
                  StoreOptMixin *other = nullptr);
    void SetRule(RecordingRule *rule) { m_rule = rule; };
    void Create(bool *err);
    void Load(void);
    void Save(void);
    void RuleChanged(void);
    void MaxEpisodesChanged(MythUIButtonListItem *);
    void PromptForRecGroup(void);
    void SetRecGroup(int recgroupID, QString recgroup);

    int CreateRecordingGroup(const QString &groupName);

    MythUIButtonList *m_recprofileList;
    MythUIButtonList *m_recgroupList;
    MythUIButtonList *m_storagegroupList;
    MythUIButtonList *m_playgroupList;
    MythUISpinBox    *m_maxepSpin;
    MythUIButtonList *m_maxbehaviourList;
    MythUICheckBox   *m_autoexpireCheck;

  private:
    MythScreenType   *m_screen;
    RecordingRule    *m_rule;
    StoreOptMixin    *m_other;
    bool              m_loaded;
};

class PostProcMixin
{
  protected:
    PostProcMixin(MythScreenType &screen, RecordingRule *rule,
                  PostProcMixin *other= nullptr);
    void SetRule(RecordingRule *rule) { m_rule = rule; };
    void Create(bool *err);
    void Load(void);
    void Save(void);
    void RuleChanged(void);
    void TranscodeChanged(bool enable);

    MythUICheckBox   *m_commflagCheck;
    MythUICheckBox   *m_transcodeCheck;
    MythUIButtonList *m_transcodeprofileList;
    MythUICheckBox   *m_userjob1Check;
    MythUICheckBox   *m_userjob2Check;
    MythUICheckBox   *m_userjob3Check;
    MythUICheckBox   *m_userjob4Check;
    MythUICheckBox   *m_metadataLookupCheck;

  private:
    MythScreenType   *m_screen;
    RecordingRule    *m_rule;
    PostProcMixin    *m_other;
    bool              m_loaded;
};

class FilterOptMixin
{
  protected:
    FilterOptMixin(MythScreenType &screen, RecordingRule *rule,
                  FilterOptMixin *other = nullptr);
    void SetRule(RecordingRule *rule) { m_rule = rule; };
    void Create(bool *err);
    void Load(void);
    void Save(void);
    void RuleChanged(void);
    void ToggleSelected(MythUIButtonListItem *item);

    MythUIButtonList *m_filtersList;
    MythUIButtonList *m_activeFiltersList;

  private:
    MythScreenType   *m_screen;
    RecordingRule    *m_rule;
    FilterOptMixin      *m_other;
    bool              m_loaded;

    QStringList       m_descriptions;
};

class ScheduleEditor : public ScheduleCommon,
    public SchedOptMixin, public FilterOptMixin,
    public StoreOptMixin, public PostProcMixin
{
  Q_OBJECT
  public:
    ScheduleEditor(MythScreenStack *parent, RecordingInfo* recinfo,
                   TV *player = nullptr);
    ScheduleEditor(MythScreenStack *parent, RecordingRule* recrule,
                   TV *player = nullptr);
   ~ScheduleEditor();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);
    void customEvent(QEvent *event);

    void showMenu(void);
    void showUpcomingByRule(void);
    void showUpcomingByTitle(void);

    /// Callback
    static void *RunScheduleEditor(ProgramInfo *proginfo, void *player = nullptr);

  signals:
    void ruleSaved(int ruleId);
    void ruleDeleted(int ruleId);
    void templateLoaded(void);

  public slots:
    void ShowSchedOpt(void);
    void ShowFilters(void);
    void ShowStoreOpt(void);
    void ShowPostProc(void);
    void ShowMetadataOptions(void);
    void ShowPreviousView(void);
    void ShowNextView(void);
    void ShowPreview(void);
    void Save(void);

  protected slots:
    void RuleChanged(MythUIButtonListItem *item);
    void DupMethodChanged(MythUIButtonListItem *);
    void FilterChanged(MythUIButtonListItem *);
    void MaxEpisodesChanged(MythUIButtonListItem *);
    void PromptForRecGroup(void);
    void TranscodeChanged(bool enable);
    void ShowSchedInfo(void);
    void ChildClosing(void);
    void Close(void);

  private:
    void Load(void);
    void LoadTemplate(QString name);
    void DeleteRule(void);

    void showTemplateMenu(void);

    virtual ProgramInfo *GetCurrentProgram(void) const
        { return m_recInfo; };

    RecordingInfo *m_recInfo;
    RecordingRule *m_recordingRule;

    bool m_sendSig;

    MythUIButton    *m_saveButton;
    MythUIButton    *m_cancelButton;

    MythUIButtonList *m_rulesList;

    MythUIButton    *m_schedOptButton;
    MythUIButton    *m_storeOptButton;
    MythUIButton    *m_postProcButton;
    MythUIButton    *m_schedInfoButton;
    MythUIButton    *m_previewButton;
    MythUIButton    *m_metadataButton;
    MythUIButton    *m_filtersButton;

    TV *m_player;

    bool             m_loaded;

    enum View
    {
        kMainView,
        kSchedOptView,
        kFilterView,
        kStoreOptView,
        kPostProcView,
        kMetadataView
    };

    int              m_view;
    SchedEditChild  *m_child;
};

class SchedEditChild : public MythScreenType
{
  Q_OBJECT
  protected:
    SchedEditChild(MythScreenStack *parent, const QString &name,
                   ScheduleEditor &editor, RecordingRule &rule,
                   RecordingInfo *recinfo);
   ~SchedEditChild() = default;

    virtual bool keyPressEvent(QKeyEvent *event);
    virtual bool CreateEditChild(
        const QString &xmlfile, const QString &winname, bool isTemplate);

  signals:
    void Closing(void);

  public slots:
    virtual void Close(void);
    virtual void Load(void) = 0;
    virtual void Save(void) = 0;

  protected:
    void SetTextFromMaps(void);

    ScheduleEditor *m_editor;
    RecordingRule  *m_recordingRule;
    RecordingInfo  *m_recInfo;

    MythUIButton   *m_backButton;
    MythUIButton   *m_saveButton;
    MythUIButton   *m_previewButton;
};

class SchedOptEditor : public SchedEditChild, public SchedOptMixin
{
  Q_OBJECT
  public:
    SchedOptEditor(MythScreenStack *parent, ScheduleEditor &editor,
                   RecordingRule &rule, RecordingInfo *recinfo);
   ~SchedOptEditor() = default;

    bool Create(void);

  protected slots:
    void DupMethodChanged(MythUIButtonListItem *);

  private:
    void Load(void);
    void Save(void);

    MythUIButton  *m_filtersButton;
};

class SchedFilterEditor : public SchedEditChild, public FilterOptMixin
{
  Q_OBJECT
  public:
    SchedFilterEditor(MythScreenStack *parent, ScheduleEditor &editor,
                      RecordingRule &rule, RecordingInfo *recinfo);
   ~SchedFilterEditor() = default;

    bool Create(void);

  protected slots:
    void ToggleSelected(MythUIButtonListItem *item);

  private:
    void Load(void);
    void Save(void);
};

class StoreOptEditor : public SchedEditChild, public StoreOptMixin
{
  Q_OBJECT
  public:
    StoreOptEditor(MythScreenStack *parent, ScheduleEditor &editor,
                   RecordingRule &rule, RecordingInfo *recinfo);
   ~StoreOptEditor() = default;

    bool Create(void);
    void customEvent(QEvent *event);

  protected slots:
    void MaxEpisodesChanged(MythUIButtonListItem *);
    void PromptForRecGroup(void);

  private:
    void Load(void);
    void Save(void);
};

class PostProcEditor : public SchedEditChild, public PostProcMixin
{
  Q_OBJECT
  public:
    PostProcEditor(MythScreenStack *parent, ScheduleEditor &editor,
                   RecordingRule &rule, RecordingInfo *recinfo);
   ~PostProcEditor() = default;

    bool Create(void);

  protected slots:
    void TranscodeChanged(bool enable);

  private:
    void Load(void);
    void Save(void);
};

class MetadataOptions : public SchedEditChild
{
  Q_OBJECT
  public:
    MetadataOptions(MythScreenStack *parent, ScheduleEditor &editor,
                    RecordingRule &rule, RecordingInfo *recinfo);
   ~MetadataOptions();

    bool Create(void);

  protected slots:
    void PerformQuery();
    void SelectLocalFanart();
    void SelectLocalCoverart();
    void SelectLocalBanner();
    void SelectOnlineFanart();
    void SelectOnlineCoverart();
    void SelectOnlineBanner();
    void QueryComplete(MetadataLookup *lookup);
    void OnSearchListSelection(RefCountHandler<MetadataLookup> lookup);
    void OnImageSearchListSelection(ArtworkInfo info,
                               VideoArtworkType type);
    void OnArtworkSearchDone(MetadataLookup *lookup);
    void FindNetArt(VideoArtworkType type);

    void ValuesChanged();

  private:
    void Load(void);
    void Save(void);

    void CreateBusyDialog(QString title);
    void FindImagePopup(const QString &prefix,
                        const QString &prefixAlt,
                        QObject &inst,
                        const QString &returnEvent);
    QStringList GetSupportedImageExtensionFilter();

    void HandleDownloadedImages(MetadataLookup *lookup);
    MetadataLookup *CreateLookup(MetadataType mtype);

    bool CanSetArtwork(void);

    void customEvent(QEvent *event);

    // For all metadata downloads
    MetadataFactory *m_metadataFactory;

    // For image picking
    MetadataDownload *m_imageLookup;
    MetadataImageDownload *m_imageDownload;

    MythScreenStack  *m_popupStack;
    MythUIBusyDialog *m_busyPopup;

    MythUIImage     *m_fanart;
    MythUIImage     *m_coverart;
    MythUIImage     *m_banner;

    MythUITextEdit  *m_inetrefEdit;

    MythUISpinBox   *m_seasonSpin;
    MythUISpinBox   *m_episodeSpin;

    MythUIButton    *m_queryButton;
    MythUIButton    *m_localFanartButton;
    MythUIButton    *m_localCoverartButton;
    MythUIButton    *m_localBannerButton;
    MythUIButton    *m_onlineFanartButton;
    MythUIButton    *m_onlineCoverartButton;
    MythUIButton    *m_onlineBannerButton;

    ArtworkMap       m_artworkMap;
};

#endif
