#!/bin/sh

# Enable loading the virtualbox kernel modules
grep '^vboxdrv_load="YES"' /boot/loader.conf >/dev/null 2>/dev/null
if [ $? -ne 0 ] ; then
        echo 'vboxdrv_load="YES"' >>/boot/loader.conf
fi

# Enable loading the vboxnet drivers
grep '^vboxnet_enable="YES"' /etc/rc.conf >/dev/null 2>/dev/null
if [ $? -ne 0 ] ; then
        echo 'vboxnet_enable="YES"' >>/etc/rc.conf
fi

exit 0
