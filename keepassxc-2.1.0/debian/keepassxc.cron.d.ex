#
# Regular cron jobs for the keepassxc package
#
0 4	* * *	root	[ -x /usr/bin/keepassxc_maintenance ] && /usr/bin/keepassxc_maintenance
