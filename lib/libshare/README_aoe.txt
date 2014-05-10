This is support for AoE (ATA-over-Ethernet), using the vblade driver
from http://aoetools.sourceforge.net/.

It will call 'vblade' to share a ZVOL via AoE.

   zfs create -s -V10G mypool/test
   zfs set shareaoe=on mypool/test

There is no need to issue 'zfs share tank/test', because ZFS will
automatically issue the corresponding share command(s) when setting
(or modifying) the shareaoe property.


The driver will execute the following commands (example!):

    /usr/sbin/vblade 9 0 eth0  /dev/zvol/rpool/test
