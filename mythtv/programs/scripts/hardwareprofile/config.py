# -*- coding: utf-8 -*-

import os, re, sys
from subprocess import Popen, PIPE
import os_detect

SMOON_URL = "http://smolt.mythtv.org/"
SECURE = 0

from MythTV import MythDB
confdir = os.path.join(MythDB().dbconfig.confdir, 'HardwareProfile')
try:
    os.mkdir(confdir, 0o700)
except OSError:
    pass

HW_UUID     = os.path.join(confdir, 'hw-uuid')
PUB_UUID    = os.path.join(confdir, 'pub-uuid')
UUID_DB     = os.path.join(confdir, 'uuiddb.cfg')
ADMIN_TOKEN = os.path.join(confdir, 'smolt_token')

#These are the defaults taken from the source code.
#fs_types = get_config_attr("FS_TYPES", ["ext2", "ext3", "xfs", "reiserfs"])
#fs_mounts = get_config_attr("FS_MOUNTS", ["/", "/home", "/etc", "/var", "/boot"])
#fs_m_filter = get_config_attr("FS_M_FILTER", False)
#fs_t_filter = get_config_attr("FS_T_FILTER", False)

FS_T_FILTER=False
FS_M_FILTER=True

try:
    # This doesn't appear to be used and the old rpm command didn't return mounts.
    p = Popen(['findmnt', '--noheadings', '--output', 'TARGET', '--raw', '--fstab'], stdout=PIPE)
    FS_MOUNTS = p.stdout.read().decode('utf-8')
except OSError:
    FS_MOUNTS = ''

#This will attempt to find the distro.
#Uncomment any of the OS lines below to hardcode.
OS = os_detect.get_os_info()


#For non RH Distros
#HW_UUID = "/etc/smolt/hw-uuid"

