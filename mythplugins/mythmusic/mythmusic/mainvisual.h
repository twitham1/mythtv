// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef MAINVISUAL_H
#define MAINVISUAL_H

#include <vector>
using namespace std;

#include "constants.h"

#include <QResizeEvent>
#include <QPaintEvent>
#include <QStringList>
#include <QHideEvent>
#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QList>

// MythTV headers
#include <visual.h>

// MythMusic
#include "visualize.h"

class MythUIVideo;

// base class to handle things like frame rate...
class MainVisual :  public QObject, public MythTV::Visual
{
    Q_OBJECT

  public:
    explicit MainVisual(MythUIVideo *visualizer);
    ~MainVisual() override;

    VisualBase *visual(void) const { return m_vis; }
    void setVisual(const QString &name);

    void stop(void);

    void resize(const QSize &size);

    void add(const void *buffer, unsigned long b_len, unsigned long timecode,
             int source_channel, int bits_per_sample) override; // Visual
    void prepare(void) override; // Visual

    void customEvent(QEvent *event) override; // QObject

    void setFrameRate(int newfps);
    int frameRate(void) const { return m_fps; }

    QStringList getVisualizations(void) { return m_visualizers; }

    int getCurrentVisual(void) const { return m_currentVisualizer; }

  public slots:
    void timeout();

  private:
    MythUIVideo *m_visualizerVideo {nullptr};
    QStringList m_visualizers;
    int m_currentVisualizer        {0};
    VisualBase *m_vis              {nullptr};
    QPixmap m_pixmap;
    QList<VisualNode*> m_nodes;
    bool m_playing                 {false};
    int m_fps                      {20};
    unsigned long m_samples        {SAMPLES_DEFAULT_SIZE};
    QTimer *m_updateTimer          {nullptr};
};

#endif // MAINVISUAL_H
