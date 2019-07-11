#ifndef MYTHUI_WEBBROWSER_H_
#define MYTHUI_WEBBROWSER_H_

#include <QtGlobal>
#include <QUrl>
#include "mythuitype.h"
#include "mythuiexp.h"

#include <QString>
#include <QTime>
#include <QColor>
#include <QIcon>

#include <QWebView>
#include <QWebPage>
#include <QNetworkReply>

class MythUIScrollBar;
class MythUIWebBrowser;
class MythUIBusyDialog;
class MythScreenType;

class BrowserApi : public QObject
{
    Q_OBJECT
  public:
    explicit BrowserApi(QObject *parent);
    ~BrowserApi(void);

    void setWebView(QWebView *view);

  public slots:
    void Play(void);
    void Stop(void);
    void Pause(void);

    void SetVolume(int volumn);
    int GetVolume(void);

    void PlayFile(const QString& filename);
    void PlayTrack(int trackID);
    void PlayURL(const QString& url);

    QString GetMetadata(void);

  private slots:
    void attachObject();

  private:
    void customEvent(QEvent *e) override; // QObject

    QWebFrame *m_frame;

    bool       m_gotAnswer;
    QString    m_answer;
};

class MythNetworkAccessManager : public QNetworkAccessManager
{
  Q_OBJECT
  public:
    MythNetworkAccessManager() = default;

  protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest & req,
                                 QIODevice * outgoingData = nullptr) override; // QNetworkAccessManager
};

class MythWebPage : public QWebPage
{
  Q_OBJECT

  public:
    explicit MythWebPage(QObject *parent = nullptr);
    ~MythWebPage();

    bool extension (Extension extension, const ExtensionOption *option = nullptr,
                    ExtensionReturn *output = nullptr) override; // QWebPage
    bool supportsExtension (Extension extension) const override; // QWebPage

  protected:
    QString userAgentForUrl(const QUrl &url) const override; // QWebPage

  private:
    friend class MythWebView;
};

class MythWebView : public QWebView
{
  Q_OBJECT

  public:
    MythWebView(QWidget *parent, MythUIWebBrowser *parentBrowser);
    ~MythWebView(void);

    void keyPressEvent(QKeyEvent *event) override; // QWidget
    void customEvent(QEvent *e) override; // QWidget

  protected slots:
    void  handleUnsupportedContent(QNetworkReply *reply);
    void  handleDownloadRequested(const QNetworkRequest &request);
    QWebView *createWindow(QWebPage::WebWindowType type) override; // QWebView

  private:
    void showDownloadMenu(void);
    void doDownloadRequested(const QNetworkRequest &request);
    void doDownload(const QString &saveFilename);
    void openBusyPopup(const QString &message);
    void closeBusyPopup(void);

    bool isMusicFile(const QString &extension, const QString &mimetype);
    bool isVideoFile(const QString &extension, const QString &mimetype);

    QString getReplyMimetype(void);
    QString getExtensionForMimetype(const QString &mimetype);

    MythWebPage      *m_webpage         {nullptr};
    MythUIWebBrowser *m_parentBrowser   {nullptr};
    BrowserApi       *m_api             {nullptr};
    QNetworkRequest   m_downloadRequest;
    QNetworkReply    *m_downloadReply   {nullptr};
    MythUIBusyDialog *m_busyPopup       {nullptr};
    bool              m_downloadAndPlay {false};
};

/**
 * \brief Web browsing widget. Can load and render local and remote html.
 *        Supports netscape plugins.
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIWebBrowser : public MythUIType
{
  Q_OBJECT

  public:
    MythUIWebBrowser(MythUIType *parent, const QString &name);
    ~MythUIWebBrowser();

    void Init(void);

    void LoadPage(const QUrl& url);
    void SetHtml(const QString &html, const QUrl &baseUrl = QUrl());

    void LoadUserStyleSheet(const QUrl& url);

    bool keyPressEvent(QKeyEvent *event) override; // MythUIType
    void Pulse(void) override; // MythUIType
    void Scroll(int dx, int dy);

    QIcon GetIcon(void);
    QUrl  GetUrl(void);
    QString GetTitle(void);

    void SetActive(bool active);
    bool IsActive(void) { return m_active; }

    /// returns true if all keypresses are to be passed to the web page
    bool IsInputToggled(void) { return m_inputToggled; }
    void SetInputToggled(bool inputToggled) { m_inputToggled = inputToggled; }

    void  SetZoom(float zoom);
    float GetZoom(void);

    bool CanGoForward(void);
    bool CanGoBack(void);

    QVariant evaluateJavaScript(const QString& scriptSource);

    void SetDefaultSaveDirectory(const QString &saveDir);
    QString GetDefaultSaveDirectory(void) { return m_defaultSaveDir; }

    void SetDefaultSaveFilename(const QString &filename);
    QString GetDefaultSaveFilename(void) { return m_defaultSaveFilename; }

  public slots:
    void Back(void);
    void Forward(void);
    void ZoomIn(void);
    void ZoomOut(void);

  signals:
    void loadStarted(void);                    /// a page is starting to load
    void loadFinished(bool ok);                /// a page has finished loading
    void loadProgress(int progress);           /// % of page loaded
    void titleChanged(const QString &title);   /// a pages title has changed
    void statusBarMessage(const QString &text);/// link hit test messages
    void iconChanged(void);                    /// a pages fav icon has changed
    void fileDownloaded(QString filename);     /// a file has been downloaded

  protected slots:
    void slotLoadStarted(void);
    void slotLoadFinished(bool Ok);
    void slotLoadProgress(int progress);
    void slotTitleChanged(const QString &title);
    void slotStatusBarMessage(const QString &text);
    void slotIconChanged(void);
    void slotLinkClicked(const QUrl &url);
    void slotTopScreenChanged(MythScreenType *screen);
    void slotScrollBarShowing(void);
    void slotScrollBarHiding(void);

  protected:
    void Finalize(void) override; // MythUIType
    void UpdateBuffer(void);
    void HandleMouseAction(const QString &action);
    void SetBackgroundColor(QColor color);
    void ResetScrollBars(void);
    void UpdateScrollBars(void);
    bool IsOnTopScreen(void);

    void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect) override; // MythUIType

    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIType
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType

    MythScreenType *m_parentScreen;

    MythWebView *m_browser;
    MythRect     m_browserArea;
    MythRect     m_actualBrowserArea;

    MythImage   *m_image;

    bool         m_active;
    bool         m_wasActive;
    bool         m_initialized;
    QTime        m_lastUpdateTime;
    int          m_updateInterval;

    float        m_zoom;
    QColor       m_bgColor;
    QUrl         m_widgetUrl;
    QString      m_userCssFile;
    QString      m_defaultSaveDir;
    QString      m_defaultSaveFilename;

    bool         m_inputToggled;
    QString      m_lastMouseAction;
    int          m_mouseKeyCount;
    QTime        m_lastMouseActionTime;

    MythUIScrollBar *m_horizontalScrollbar;
    MythUIScrollBar *m_verticalScrollbar;
    MythUIAnimation  m_scrollAnimation;
    QPoint           m_destinationScrollPos;
};

#endif
