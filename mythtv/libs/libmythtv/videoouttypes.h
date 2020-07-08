#ifndef VIDEOOUT_TYPES_H_
#define VIDEOOUT_TYPES_H_

// Qt
#include <QString>
#include <QObject>

enum PIPState
{
    kPIPOff = 0,
    kPIPonTV,
    kPIPStandAlone,
    kPBPLeft,
    kPBPRight,
};

enum PIPLocation
{
    kPIPTopLeft = 0,
    kPIPBottomLeft,
    kPIPTopRight,
    kPIPBottomRight,
    kPIP_END
};

enum ZoomDirection
{
    kZoomHome = 0,
    kZoomIn,
    kZoomOut,
    kZoomVerticalIn,
    kZoomVerticalOut,
    kZoomHorizontalIn,
    kZoomHorizontalOut,
    kZoomUp,
    kZoomDown,
    kZoomLeft,
    kZoomRight,
    kZoomAspectUp,
    kZoomAspectDown,
    kZoom_END
};

enum AspectOverrideMode
{
    kAspect_Toggle = -1,
    kAspect_Off = 0,
    kAspect_4_3,
    kAspect_16_9,
    kAspect_14_9, // added after 16:9 so as not to upset existing setups.
    kAspect_2_35_1,
    kAspect_END
};

enum AdjustFillMode
{
    kAdjustFill_Toggle = -1,
    kAdjustFill_Off = 0,
    kAdjustFill_Half,
    kAdjustFill_Full,
    kAdjustFill_HorizontalStretch,
    kAdjustFill_VerticalStretch,
    kAdjustFill_HorizontalFill,
    kAdjustFill_VerticalFill,
    kAdjustFill_END,
    kAdjustFill_AutoDetect_DefaultOff,
    kAdjustFill_AutoDetect_DefaultHalf,
};

enum LetterBoxColour
{
    kLetterBoxColour_Toggle = -1,
    kLetterBoxColour_Black = 0,
    kLetterBoxColour_Gray25,
    kLetterBoxColour_END
};

enum FrameScanType
{
    kScan_Ignore       = -1,
    kScan_Detect       =  0,
    kScan_Interlaced   =  1,
    kScan_Intr2ndField =  2,
    kScan_Progressive  =  3,
};

enum PictureAttribute
{
    kPictureAttribute_None = 0,
    kPictureAttribute_MIN = 0,
    kPictureAttribute_Brightness = 1,
    kPictureAttribute_Contrast,
    kPictureAttribute_Colour,
    kPictureAttribute_Hue,
    kPictureAttribute_Range,
    kPictureAttribute_Volume,
    kPictureAttribute_MAX
};

enum PictureAttributeSupported
{
    kPictureAttributeSupported_None       = 0x00,
    kPictureAttributeSupported_Brightness = 0x01,
    kPictureAttributeSupported_Contrast   = 0x02,
    kPictureAttributeSupported_Colour     = 0x04,
    kPictureAttributeSupported_Hue        = 0x08,
    kPictureAttributeSupported_Range      = 0x10,
    kPictureAttributeSupported_Volume     = 0x20,
};

#define ALL_PICTURE_ATTRIBUTES static_cast<PictureAttributeSupported> \
    (kPictureAttributeSupported_Brightness | \
     kPictureAttributeSupported_Contrast | \
     kPictureAttributeSupported_Colour | \
     kPictureAttributeSupported_Hue | \
     kPictureAttributeSupported_Range)

enum StereoscopicMode
{
    kStereoscopicModeNone,
    kStereoscopicModeSideBySide,
    kStereoscopicModeSideBySideDiscard,
    kStereoscopicModeTopAndBottom,
    kStereoscopicModeTopAndBottomDiscard,
};

enum PrimariesMode
{
    PrimariesDisabled = 0,
    PrimariesRelaxed,
    PrimariesExact
};

inline QString toUserString(PrimariesMode Mode)
{
    if (PrimariesMode::PrimariesDisabled == Mode) return QObject::tr("Disabled");
    if (PrimariesMode::PrimariesExact == Mode)    return QObject::tr("Exact");
    return QObject::tr("Auto");
}

inline QString toDBString(PrimariesMode Mode)
{
    if (Mode == PrimariesDisabled) return "disabled";
    if (Mode == PrimariesExact)    return "exact";
    return "auto";
}

inline PrimariesMode toPrimariesMode(const QString& Mode)
{
    if (Mode == "disabled") return PrimariesDisabled;
    if (Mode == "exact")    return PrimariesExact;
    return PrimariesRelaxed;
}

inline QString StereoscopictoString(StereoscopicMode Mode)
{
    switch (Mode)
    {
        case kStereoscopicModeNone:                return QObject::tr("No 3D");
        case kStereoscopicModeSideBySide:          return QObject::tr("3D Side by Side");
        case kStereoscopicModeSideBySideDiscard:   return QObject::tr("Discard 3D Side by Side");
        case kStereoscopicModeTopAndBottom:        return QObject::tr("3D Top and Bottom");
        case kStereoscopicModeTopAndBottomDiscard: return QObject::tr("Discard 3D Top and Bottom");
    }
    return QObject::tr("Unknown");
}

enum VideoErrorState
{
    kError_None            = 0x00,
    kError_Unknown         = 0x01,
};

inline bool is_interlaced(FrameScanType Scan)
{
    return (kScan_Interlaced == Scan) || (kScan_Intr2ndField == Scan);
}

inline bool is_progressive(FrameScanType Scan)
{
    return (kScan_Progressive == Scan);
}

inline QString ScanTypeToString(FrameScanType Scan, bool Forced = false)
{
    switch (Scan)
    {
        case kScan_Ignore:       return QObject::tr("Ignore");
        case kScan_Detect:       return QObject::tr("Detect");
        case kScan_Progressive:  return Forced ? QObject::tr("Progressive (Forced)") : QObject::tr("Progressive");
        case kScan_Interlaced:   return Forced ? QObject::tr("Interlaced (Forced)") : QObject::tr("Interlaced");
        case kScan_Intr2ndField: return Forced ? QObject::tr("Interlaced (Reversed, Forced)") : QObject::tr("Interlaced (Reversed)");
    }
    return QObject::tr("Unknown");
}

inline QString toString(PIPState State)
{
    switch (State)
    {
        case kPIPOff:        return QString("Pip Off");
        case kPIPonTV:       return QString("Pip on TV");
        case kPIPStandAlone: return QString("Pip Standalone");
        case kPBPLeft:       return QString("PBP Left");
        case kPBPRight:      return QString("PBP Right");
    }
    return QString("Unknown");
}

inline QString toString(PIPLocation Location)
{
    switch (Location)
    {
        case kPIPTopLeft:     return QObject::tr("Top Left");
        case kPIPBottomLeft:  return QObject::tr("Bottom Left");
        case kPIPTopRight:    return QObject::tr("Top Right");
        case kPIPBottomRight: return QObject::tr("Bottom Right");
        case kPIP_END:        break;
    }
    return "";
}

inline QString toString(AspectOverrideMode Aspectmode)
{
    switch (Aspectmode)
    {
        case kAspect_4_3:    return QObject::tr("4:3");
        case kAspect_14_9:   return QObject::tr("14:9");
        case kAspect_16_9:   return QObject::tr("16:9");
        case kAspect_2_35_1: return QObject::tr("2.35:1");
        case kAspect_Toggle:
        case kAspect_Off:
        case kAspect_END:    break;
    }
    return QObject::tr("Off");
}

inline QString toString(LetterBoxColour LetterboxColour)
{
    switch (LetterboxColour)
    {
        case kLetterBoxColour_Gray25: return QObject::tr("Gray");
        case kLetterBoxColour_Black:
        case kLetterBoxColour_Toggle:
        case kLetterBoxColour_END:    break;
    }
    return QObject::tr("Black");
}

inline float get_aspect_override(AspectOverrideMode Aspectmode, float Original)
{
    switch (Aspectmode)
    {
        case kAspect_4_3:    return 4.0F  / 3.0F;
        case kAspect_14_9:   return 14.0F / 9.0F;
        case kAspect_16_9:   return 16.0F / 9.0F;
        case kAspect_2_35_1: return 2.35F;
        case kAspect_Toggle:
        case kAspect_Off:
        case kAspect_END:    break;
    }
    return Original;
}

inline QString toString(AdjustFillMode Aspectmode)
{
    switch (Aspectmode)
    {
        case kAdjustFill_Half: return QObject::tr("Half");
        case kAdjustFill_Full: return QObject::tr("Full");
        case kAdjustFill_HorizontalStretch: return QObject::tr("H.Stretch");
        case kAdjustFill_VerticalStretch:   return QObject::tr("V.Stretch");
        case kAdjustFill_VerticalFill:      return QObject::tr("V.Fill");
        case kAdjustFill_HorizontalFill:    return QObject::tr("H.Fill");
        case kAdjustFill_AutoDetect_DefaultOff: return QObject::tr("Auto Detect (Default Off)");
        case kAdjustFill_AutoDetect_DefaultHalf: return QObject::tr("Auto Detect (Default Half)");
        case kAdjustFill_Toggle:
        case kAdjustFill_Off:
        case kAdjustFill_END: break;
    }
    return QObject::tr("Off");
}

inline QString toString(PictureAttribute PictureAttribute)
{
    switch (PictureAttribute)
    {
        case kPictureAttribute_Brightness: return QObject::tr("Brightness");
        case kPictureAttribute_Contrast:   return QObject::tr("Contrast");
        case kPictureAttribute_Colour:     return QObject::tr("Color");
        case kPictureAttribute_Hue:        return QObject::tr("Hue");
        case kPictureAttribute_Range:      return QObject::tr("Range");
        case kPictureAttribute_Volume:     return QObject::tr("Volume");
        case kPictureAttribute_MAX:        return "MAX";
        case kPictureAttribute_None:       break;
    }
    return QObject::tr("None");
}

inline QString toDBString(PictureAttribute PictureAttribute)
{
    switch (PictureAttribute)
    {
        case kPictureAttribute_Brightness: return "brightness";
        case kPictureAttribute_Contrast:   return "contrast";
        case kPictureAttribute_Colour:     return "colour";
        case kPictureAttribute_Hue:        return "hue";
        case kPictureAttribute_None:
        case kPictureAttribute_Range:
        case kPictureAttribute_Volume:
        case kPictureAttribute_MAX:        break;
    }
    return "";
}

inline QString toString(PictureAttributeSupported Supported)
{
    QStringList list;
    if (kPictureAttributeSupported_Brightness & Supported)
        list += "Brightness";
    if (kPictureAttributeSupported_Contrast & Supported)
        list += "Contrast";
    if (kPictureAttributeSupported_Colour & Supported)
        list += "Colour";
    if (kPictureAttributeSupported_Hue & Supported)
        list += "Hue";
    if (kPictureAttributeSupported_Range & Supported)
        list += "Range";
    if (kPictureAttributeSupported_Volume & Supported)
        list += "Volume";
    return list.join(",");
}

inline PictureAttributeSupported toMask(PictureAttribute PictureAttribute)
{
    switch (PictureAttribute)
    {
        case kPictureAttribute_Brightness: return kPictureAttributeSupported_Brightness;
        case kPictureAttribute_Contrast:   return kPictureAttributeSupported_Contrast;
        case kPictureAttribute_Colour:     return kPictureAttributeSupported_Colour;
        case kPictureAttribute_Hue:        return kPictureAttributeSupported_Hue;
        case kPictureAttribute_Range:      return kPictureAttributeSupported_Range;
        case kPictureAttribute_Volume:     return kPictureAttributeSupported_Volume;
        case kPictureAttribute_None:
        case kPictureAttribute_MAX:        break;
    }
    return kPictureAttributeSupported_None;
}

inline PictureAttribute next(PictureAttributeSupported Supported,
                             PictureAttribute          Attribute)
{
    int i = static_cast<int>(Attribute + 1) % static_cast<int>(kPictureAttribute_MAX);
    for (int j = 0; j < kPictureAttribute_MAX; (i = (i +1 ) % kPictureAttribute_MAX), j++)
        if (toMask(static_cast<PictureAttribute>(i)) & Supported)
            return static_cast<PictureAttribute>(i);
    return kPictureAttribute_None;
}

#endif // VIDEOOUT_TYPES_H_
