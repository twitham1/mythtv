// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson

#include <cstdlib>

#include "mythlogging.h"
#include "decoderbase.h"
#include "mythplayer.h"
#include "cc708reader.h"

#define LOC QString("CC708Reader: ")
#define CHECKENABLED if (!m_enabled) return

CC708Reader::CC708Reader(MythPlayer *owner)
  : m_parent(owner)
{
    for (uint i=0; i < k708MaxServices; i++)
    {
        m_buf_alloc[i] = 512;
        m_buf[i]       = (unsigned char*) malloc(m_buf_alloc[i]);
        m_buf_size[i]  = 0;
        m_delayed[i]   = false;

        m_temp_str_alloc[i] = 512;
        m_temp_str_size[i]  = 0;
        m_temp_str[i]       = (short*) malloc(m_temp_str_alloc[i] * sizeof(short));
    }
    memset(&CC708DelayedDeletes, 0, sizeof(CC708DelayedDeletes));
}

CC708Reader::~CC708Reader()
{
    for (uint i=0; i < k708MaxServices; i++)
    {
        free(m_buf[i]);
        free(m_temp_str[i]);
    }
}

void CC708Reader::ClearBuffers(void)
{
    for (uint i = 1; i < k708MaxServices; i++)
        DeleteWindows(i, 0xff);
}

void CC708Reader::SetCurrentWindow(uint service_num, int window_id)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("SetCurrentWindow(%1, %2)")
            .arg(service_num).arg(window_id));
    CC708services[service_num].m_current_window = window_id;
}

void CC708Reader::DefineWindow(
    uint service_num,     int window_id,
    int priority,         bool visible,
    int anchor_point,     int relative_pos,
    int anchor_vertical,  int anchor_horizontal,
    int row_count,        int column_count,
    int row_lock,         int column_lock,
    int pen_style,        int window_style)
{
    if (m_parent && m_parent->GetDecoder())
    {
        StreamInfo si(-1, 0, 0, service_num, 0, false, false);
        m_parent->GetDecoder()->InsertTrack(kTrackTypeCC708, si);
    }

    CHECKENABLED;

    CC708DelayedDeletes[service_num & 63] &= ~(1 << window_id);

    LOG(VB_VBI, LOG_DEBUG, LOC +
        QString("DefineWindow(%1, %2,\n\t\t\t\t\t")
            .arg(service_num).arg(window_id) +
        QString("  prio %1, vis %2, ap %3, rp %4, av %5, ah %6")
            .arg(priority).arg(visible).arg(anchor_point).arg(relative_pos)
            .arg(anchor_vertical).arg(anchor_horizontal) +
        QString("\n\t\t\t\t\t  row_cnt %1, row_lck %2, "
                    "col_cnt %3, col_lck %4 ")
            .arg(row_count).arg(row_lock)
            .arg(column_count).arg(column_lock) +
        QString("\n\t\t\t\t\t  pen style %1, win style %2)")
            .arg(pen_style).arg(window_style));

    GetCCWin(service_num, window_id)
        .DefineWindow(priority,         visible,
                      anchor_point,     relative_pos,
                      anchor_vertical,  anchor_horizontal,
                      row_count,        column_count,
                      row_lock,         column_lock,
                      pen_style,        window_style);

    CC708services[service_num].m_current_window = window_id;
}

void CC708Reader::DeleteWindows(uint service_num, int window_map)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("DeleteWindows(%1, %2)")
            .arg(service_num).arg(window_map, 8, 2, QChar(48)));

    for (uint i = 0; i < 8; i++)
        if ((1 << i) & window_map)
            GetCCWin(service_num, i).Clear();
    CC708DelayedDeletes[service_num&63] |= window_map;
}

void CC708Reader::DisplayWindows(uint service_num, int window_map)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("DisplayWindows(%1, %2)")
            .arg(service_num).arg(window_map, 8, 2, QChar(48)));

    for (uint i = 0; i < 8; i++)
    {
        if ((1 << i) & CC708DelayedDeletes[service_num & 63])
        {
            CC708Window &win = GetCCWin(service_num, i);
            QMutexLocker locker(&win.m_lock);

            win.SetExists(false);
            if (win.m_text)
            {
                delete [] win.m_text;
                win.m_text = nullptr;
            }
        }
        CC708DelayedDeletes[service_num & 63] = 0;
    }

    for (uint i = 0; i < 8; i++)
    {
        if ((1 << i ) & window_map)
        {
            CC708Window &win = GetCCWin(service_num, i);
            win.SetVisible(true);
            LOG(VB_VBI, LOG_DEBUG, LOC +
                QString("DisplayedWindow(%1, %2)").arg(service_num).arg(i));
        }
    }
}

void CC708Reader::HideWindows(uint service_num, int window_map)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("HideWindows(%1, %2)")
            .arg(service_num).arg(window_map, 8, 2, QChar(48)));

    for (uint i = 0; i < 8; i++)
    {
        if ((1 << i) & window_map)
        {
            CC708Window &win = GetCCWin(service_num, i);
            win.SetVisible(false);
        }
    }
}

void CC708Reader::ClearWindows(uint service_num, int window_map)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("ClearWindows(%1, %2)")
            .arg(service_num).arg(window_map, 8, 2, QChar(48)));

    for (uint i = 0; i < 8; i++)
        if ((1 << i) & window_map)
            GetCCWin(service_num, i).Clear();
}

void CC708Reader::ToggleWindows(uint service_num, int window_map)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("ToggleWindows(%1, %2)")
            .arg(service_num).arg(window_map, 8, 2, QChar(48)));

    for (uint i = 0; i < 8; i++)
    {
        if ((1 << i) & window_map)
        {
            CC708Window &win = GetCCWin(service_num, i);
            win.SetVisible(!win.GetVisible());
        }
    }
}

void CC708Reader::SetWindowAttributes(
    uint service_num,
    int fill_color,     int fill_opacity,
    int border_color,   int border_type,
    int scroll_dir,     int print_dir,
    int effect_dir,
    int display_effect, int effect_speed,
    int justify,        int word_wrap)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("SetWindowAttributes(%1...)")
            .arg(service_num));

    CC708Window &win = GetCCWin(service_num);

    win.m_fill_color     = fill_color   & 0x3f;
    win.m_fill_opacity   = fill_opacity;
    win.m_border_color   = border_color & 0x3f;
    win.m_border_type    = border_type;
    win.m_scroll_dir     = scroll_dir;
    win.m_print_dir      = print_dir;
    win.m_effect_dir     = effect_dir;
    win.m_display_effect = display_effect;
    win.m_effect_speed   = effect_speed;
    win.m_justify        = justify;
    win.m_word_wrap      = word_wrap;
}

void CC708Reader::SetPenAttributes(
    uint service_num, int pen_size,
    int offset,       int text_tag,  int font_tag,
    int edge_type,    int underline, int italics)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("SetPenAttributes(%1, %2,")
            .arg(service_num).arg(CC708services[service_num].m_current_window) +
            QString("\n\t\t\t\t\t      pen_size %1, offset %2, text_tag %3, "
                    "font_tag %4,"
                    "\n\t\t\t\t\t      edge_type %5, underline %6, italics %7")
            .arg(pen_size).arg(offset).arg(text_tag).arg(font_tag)
            .arg(edge_type).arg(underline).arg(italics));

    GetCCWin(service_num).m_pen.SetAttributes(
        pen_size, offset, text_tag, font_tag, edge_type, underline, italics);
}

void CC708Reader::SetPenColor(
    uint service_num,
    int fg_color, int fg_opacity,
    int bg_color, int bg_opacity,
    int edge_color)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG,
        LOC + QString("SetPenColor(service=%1, fg%2.%3, bg=%4.%5, edge=%6)")
        .arg(service_num).arg(fg_color).arg(fg_opacity)
        .arg(bg_color).arg(bg_opacity).arg(edge_color));

    CC708CharacterAttribute &attr = GetCCWin(service_num).m_pen.attr;

    attr.m_fg_color   = fg_color;
    attr.m_fg_opacity = fg_opacity;
    attr.m_bg_color   = bg_color;
    attr.m_bg_opacity = bg_opacity;
    attr.m_edge_color = edge_color;
}

void CC708Reader::SetPenLocation(uint service_num, int row, int column)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("SetPenLocation(%1, (c %2, r %3))")
            .arg(service_num).arg(column).arg(row));
    GetCCWin(service_num).SetPenLocation(row, column);
}

void CC708Reader::Delay(uint service_num, int tenths_of_seconds)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("Delay(%1, %2 seconds)")
            .arg(service_num).arg(tenths_of_seconds * 0.1F));
}

void CC708Reader::DelayCancel(uint service_num)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("DelayCancel(%1)").arg(service_num));
}

void CC708Reader::Reset(uint service_num)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("Reset(%1)").arg(service_num));
    DeleteWindows(service_num, 0xff);
    DelayCancel(service_num);
}

void CC708Reader::TextWrite(uint service_num,
                                  short* unicode_string, short len)
{
    CHECKENABLED;
    QString debug = QString();
    for (uint i = 0; i < (uint)len; i++)
    {
        GetCCWin(service_num).AddChar(QChar(unicode_string[i]));
        debug += QChar(unicode_string[i]);
    }
    LOG(VB_VBI, LOG_DEBUG, LOC + QString("AddText to %1->%2 |%3|")
        .arg(service_num).arg(CC708services[service_num].m_current_window).arg(debug));
}
