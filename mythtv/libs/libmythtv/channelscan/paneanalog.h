/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef PANE_ANALOG_H
#define PANE_ANALOG_H

#include "standardsettings.h"

class TransFreqTableSelector;

class PaneAnalog : public GroupSetting
{
  public:
    PaneAnalog(const QString &target, StandardSetting *setting);

    void SetSourceID(uint sourceid);

    QString GetFrequencyTable(void) const;

  protected:
    TransFreqTableSelector  *m_freq_table {nullptr};
};

#endif // PANE_ANALOG_H
