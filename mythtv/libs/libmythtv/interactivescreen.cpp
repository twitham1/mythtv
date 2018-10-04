#include "interactivescreen.h"

InteractiveScreen::InteractiveScreen(MythPlayer *player,
                                     const QString &name) :
    MythScreenType((MythScreenType*)nullptr, name),
    m_player(player),  m_safeArea(QRect())
{
    UpdateArea();
}

void InteractiveScreen::Close(void)
{
    if (m_player)
        m_player->SetVideoResize(QRect());
}

void InteractiveScreen::UpdateArea(void)
{
    if (m_ChildrenList.isEmpty())
    {
        m_safeArea = QRect();
    }
    else if (m_player && m_player->GetVideoOutput())
    {
        float tmp = 0.0;
        QRect dummy;
        m_player->GetVideoOutput()->GetOSDBounds(dummy, m_safeArea, tmp, tmp, tmp);
    }
    SetArea(MythRect(m_safeArea));
}

void InteractiveScreen::OptimiseDisplayedArea(void)
{
    UpdateArea();

    QRegion visible;
    QListIterator<MythUIType *> i(m_ChildrenList);
    while (i.hasNext())
    {
        MythUIType *img = i.next();
        visible = visible.united(img->GetArea());
    }

    if (visible.isEmpty())
        return;

    QRect bounding  = visible.boundingRect();
    bounding = bounding.translated(m_safeArea.topLeft());
    bounding = m_safeArea.intersected(bounding);
    int left = m_safeArea.left() - bounding.left();
    int top  = m_safeArea.top()  - bounding.top();
    SetArea(MythRect(bounding));

    i.toFront();;
    while (i.hasNext())
    {
        MythUIType *img = i.next();
        img->SetArea(img->GetArea().translated(left, top));
    }
}
