#ifndef MYTHUIANIMATION_H
#define MYTHUIANIMATION_H

#include "xmlparsebase.h"
#include <QVariantAnimation>

class MythUIType;

class UIEffects
{
  public:
    enum Centre { TopLeft, Top, TopRight,
                  Left, Middle, Right,
                  BottomLeft, Bottom, BottomRight };

    UIEffects() = default;

    QPointF GetCentre(const QRect &rect, int xoff, int yoff)
    {
        float x = xoff + rect.left();
        float y = yoff + rect.top();
        if (Middle == m_centre || Top == m_centre || Bottom == m_centre)
            x += rect.width() / 2.0F;
        if (Middle == m_centre || Left == m_centre || Right == m_centre)
            y += rect.height() / 2.0F;
        if (Right == m_centre || TopRight == m_centre || BottomRight == m_centre)
            x += rect.width();
        if (Bottom == m_centre || BottomLeft == m_centre || BottomRight == m_centre)
            y += rect.height();
        return QPointF(x, y);
    }

    QRect GetExtent(const QSize &size);

    int    m_alpha  {255};
    float  m_hzoom  {1.0F};
    float  m_vzoom  {1.0F};
    float  m_angle  {0.0F};
    Centre m_centre {Middle};
};

class MythUIAnimation : public QVariantAnimation, XMLParseBase
{
  public:
    enum Type    { Alpha, Position, Zoom, HorizontalZoom, VerticalZoom, Angle };
    enum Trigger { AboutToHide, AboutToShow };

    MythUIAnimation(MythUIType* parent = nullptr,
                    Trigger trigger = AboutToShow, Type type = Alpha)
        : m_parent(parent), m_type(type), m_trigger(trigger) {}
    void Activate(void);
    void CopyFrom(const MythUIAnimation* animation);
    Trigger GetTrigger(void) const { return m_trigger; }
    QVariant Value() const { return m_value; }
    bool IsActive() const { return m_active; }

    void updateCurrentValue(const QVariant& value) override; // QVariantAnimation

    void IncrementCurrentTime(void);
    void SetEasingCurve(const QString &curve);
    void SetCentre(const QString &centre);
    void SetLooped(bool looped)  { m_looped = looped;  }
    void SetReversible(bool rev) { m_reversible = rev; }

    static void ParseElement(const QDomElement& element, MythUIType* parent);

  private:
    static void ParseSection(const QDomElement &element,
                             MythUIType* parent, Trigger trigger);
    static void parseAlpha(const QDomElement& element, QVariant& startValue,
                           QVariant& endValue);
    static void parsePosition(const QDomElement& element, QVariant& startValue,
                              QVariant& endValue, MythUIType *parent);
    static void parseZoom(const QDomElement& element, QVariant& startValue,
                          QVariant& endValue);
    static void parseAngle(const QDomElement& element, QVariant& startValue,
                           QVariant& endValue);

    MythUIType* m_parent       {nullptr};
    Type        m_type         {Alpha};
    Trigger     m_trigger      {AboutToShow};
    UIEffects::Centre m_centre {UIEffects::Middle};
    QVariant    m_value;
    bool        m_active       {false};
    bool        m_looped       {false};
    bool        m_reversible   {false};
};

#endif // MYTHUIANIMATION_H
