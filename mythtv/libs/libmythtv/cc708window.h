// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson

#ifndef _CC708_WINDOW_
#define _CC708_WINDOW_

#include <vector>
using namespace std;

#include <QString>
#include <QMutex>
#include <QColor>

#include "mythtvexp.h"

extern const uint k708JustifyLeft;
extern const uint k708JustifyRight;
extern const uint k708JustifyCenter;
extern const uint k708JustifyFull;

extern const uint k708EffectSnap;
extern const uint k708EffectFade;
extern const uint k708EffectWipe;

extern const uint k708BorderNone;
extern const uint k708BorderRaised;
extern const uint k708BorderDepressed;
extern const uint k708BorderUniform;
extern const uint k708BorderShadowLeft;
extern const uint k708BorderShadowRight;

extern const uint k708DirLeftToRight;
extern const uint k708DirRightToLeft;
extern const uint k708DirTopToBottom;
extern const uint k708DirBottomToTop;

extern const uint k708AttrSizeSmall;
extern const uint k708AttrSizeStandard;
extern const uint k708AttrSizeLarge;

extern const uint k708AttrOffsetSubscript;
extern const uint k708AttrOffsetNormal;
extern const uint k708AttrOffsetSuperscript;

extern const uint k708AttrFontDefault;
extern const uint k708AttrFontMonospacedSerif;
extern const uint k708AttrFontProportionalSerif;
extern const uint k708AttrFontMonospacedSansSerif;
extern const uint k708AttrFontProportionalSansSerif;
extern const uint k708AttrFontCasual;
extern const uint k708AttrFontCursive;
extern const uint k708AttrFontSmallCaps;

extern const uint k708AttrEdgeNone;
extern const uint k708AttrEdgeRaised;
extern const uint k708AttrEdgeDepressed;
extern const uint k708AttrEdgeUniform;
extern const uint k708AttrEdgeLeftDropShadow;
extern const uint k708AttrEdgeRightDropShadow;

extern const uint k708AttrColorBlack;
extern const uint k708AttrColorWhite;

extern const uint k708AttrOpacitySolid;
extern const uint k708AttrOpacityFlash;
extern const uint k708AttrOpacityTranslucent;
extern const uint k708AttrOpacityTransparent;

const int k708MaxWindows = 8;
const int k708MaxRows    = 16; // 4-bit field in DefineWindow
const int k708MaxColumns = 64; // 6-bit field in DefineWindow

class CC708CharacterAttribute
{
  public:
    uint   m_pen_size        {k708AttrSizeStandard};
    uint   m_offset          {k708AttrOffsetNormal};
    uint   m_text_tag        {0};
    uint   m_font_tag        {0};                    // system font
    uint   m_edge_type       {k708AttrEdgeNone};
    bool   m_underline       {false};
    bool   m_italics         {false};
    bool   m_boldface        {false};

    uint   m_fg_color        {k708AttrColorWhite};   // will be overridden
    uint   m_fg_opacity      {k708AttrOpacitySolid}; // solid
    uint   m_bg_color        {k708AttrColorBlack};
    uint   m_bg_opacity      {k708AttrOpacitySolid};
    uint   m_edge_color      {k708AttrColorBlack};

    QColor m_actual_fg_color; // if !isValid(), then convert m_fg_color

    CC708CharacterAttribute(bool isItalic = false,
                            bool isBold = false,
                            bool isUnderline = false,
                            QColor fgColor = QColor()) :
        m_underline(isUnderline),
        m_italics(isItalic),
        m_boldface(isBold),
        m_actual_fg_color(fgColor)
    {
    }

    static QColor ConvertToQColor(uint eia708color);
    QColor GetFGColor(void) const
    {
        QColor fg = (m_actual_fg_color.isValid() ?
                     m_actual_fg_color : ConvertToQColor(m_fg_color));
        fg.setAlpha(GetFGAlpha());
        return fg;
    }
    QColor GetBGColor(void) const
    {
        QColor bg = ConvertToQColor(m_bg_color);
        bg.setAlpha(GetBGAlpha());
        return bg;
    }
    QColor GetEdgeColor(void) const { return ConvertToQColor(m_edge_color); }

    uint GetFGAlpha(void) const
    {
        //SOLID=0, FLASH=1, TRANSLUCENT=2, and TRANSPARENT=3.
        static uint alpha[4] = { 0xff, 0xff, 0x7f, 0x00, };
        return alpha[m_fg_opacity & 0x3];
    }

    uint GetBGAlpha(void) const
    {
        //SOLID=0, FLASH=1, TRANSLUCENT=2, and TRANSPARENT=3.
        static uint alpha[4] = { 0xff, 0xff, 0x7f, 0x00, };
        return alpha[m_bg_opacity & 0x3];
    }

    bool operator==(const CC708CharacterAttribute &other) const;
    bool operator!=(const CC708CharacterAttribute &other) const
        { return !(*this == other); }
};

class CC708Pen
{
  public:
    CC708Pen() = default;
    void SetPenStyle(uint style);
    void SetAttributes(int pen_size,
                       int offset,       int text_tag,  int font_tag,
                       int edge_type,    int underline, int italics)
    {
        attr.m_pen_size  = pen_size;
        attr.m_offset    = offset;
        attr.m_text_tag  = text_tag;
        attr.m_font_tag  = font_tag;
        attr.m_edge_type = edge_type;
        attr.m_underline = underline;
        attr.m_italics   = italics;
        attr.m_boldface  = 0;
    }
  public:
    CC708CharacterAttribute attr;

    uint m_row    {0};
    uint m_column {0};
};

class CC708Window;
class CC708Character
{
  public:
    CC708Character() = default;
    explicit CC708Character(const CC708Window &win);
    CC708CharacterAttribute m_attr;
    QChar                   m_character {' '};
};

class CC708String
{
  public:
    uint x;
    uint y;
    QString str;
    CC708CharacterAttribute attr;
};

class MTV_PUBLIC CC708Window
{
 public:
    CC708Window() = default;
    ~CC708Window();

    void DefineWindow(int priority,         bool visible,
                      int anchor_point,     int relative_pos,
                      int anchor_vertical,  int anchor_horizontal,
                      int row_count,        int column_count,
                      int row_lock,         int column_lock,
                      int pen_style,        int window_style);
    void Resize(uint new_rows, uint new_columns);
    void Clear(void);
    void SetWindowStyle(uint);

    void AddChar(QChar);
    void IncrPenLocation(void);
    void DecrPenLocation(void);
    void SetPenLocation(uint, uint);
    void LimitPenLocation(void);

    bool IsPenValid(void) const
    {
        return ((m_pen.m_row < m_true_row_count) &&
                (m_pen.m_column < m_true_column_count));
    }
    CC708Character &GetCCChar(void) const;
    vector<CC708String*> GetStrings(void) const;
    void DisposeStrings(vector<CC708String*> &strings) const;
    QColor GetFillColor(void) const
    {
        QColor fill = CC708CharacterAttribute::ConvertToQColor(m_fill_color);
        fill.setAlpha(GetFillAlpha());
        return fill;
    }
    uint GetFillAlpha(void) const
    {
        //SOLID=0, FLASH=1, TRANSLUCENT=2, and TRANSPARENT=3.
        static uint alpha[4] = { 0xff, 0xff, 0x7f, 0x00, };
        return alpha[m_fill_opacity & 0x3];
    }

 private:
    void Scroll(int row, int col);

 public:
    uint            m_priority          {0};
 private:
    bool            m_visible           {false};
 public:
    enum {
        kAnchorUpperLeft  = 0, kAnchorUpperCenter, kAnchorUpperRight,
        kAnchorCenterLeft = 3, kAnchorCenter,      kAnchorCenterRight,
        kAnchorLowerLeft  = 6, kAnchorLowerCenter, kAnchorLowerRight,
    };
    uint            m_anchor_point      {0};
    uint            m_relative_pos      {0};
    uint            m_anchor_vertical   {0};
    uint            m_anchor_horizontal {0};
    uint            m_row_count         {0};
    uint            m_column_count      {0};
    uint            m_row_lock          {0};
    uint            m_column_lock       {0};
//  uint            m_pen_style         {0};
//  uint            m_window_style      {0};

    uint            m_fill_color        {0};
    uint            m_fill_opacity      {0};
    uint            m_border_color      {0};
    uint            m_border_type       {0};
    uint            m_scroll_dir        {0};
    uint            m_print_dir         {0};
    uint            m_effect_dir        {0};
    uint            m_display_effect    {0};
    uint            m_effect_speed      {0};
    uint            m_justify           {0};
    uint            m_word_wrap         {0};

    // These are akin to the capacity of a vector, which is always >=
    // the current size.
    uint            m_true_row_count    {0};
    uint            m_true_column_count {0};

    CC708Character *m_text              {nullptr};
    CC708Pen        m_pen;

 private:
    /// set to false when DeleteWindow is called on the window.
    bool            m_exists            {false};
    bool            m_changed           {true};

 public:
    bool GetExists(void) const { return m_exists; }
    bool GetVisible(void) const { return m_visible; }
    bool GetChanged(void) const { return m_changed; }
    void SetExists(bool value)
    {
        if (m_exists != value)
            SetChanged();
        m_exists = value;
    }
    void SetVisible(bool value)
    {
        if (m_visible != value)
            SetChanged();
        m_visible = value;
    }
    void SetChanged(void)
    {
        m_changed = true;
    }
    void ResetChanged(void)
    {
        m_changed = false;
    }
    mutable QMutex  m_lock {QMutex::Recursive};
};

class CC708Service
{
  public:
    CC708Service() = default;

  public:
    uint        m_current_window {0};
    CC708Window m_windows[k708MaxWindows];
};

#endif // _CC708_WINDOW_
