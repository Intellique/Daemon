# Daemon
StoriqOne daemon

## For Debian users (or derivative)
It's recommended to use the precompiled Debian packages. You can find an assistant in order to configure the StoriqOne repository 
[here](http://update.intellique.com/storiqone/).

## Building StoriqOne Daemon
In order to build storiqone, you need these following packages
### Requirements (debian package):
 - cscope
 - debhelper (>= 9.0.0)
 - dh-systemd (>= 1.5)
 - exuberant-ctags
 - gettext
 - git
 - libexpat1-dev
 - libipc-run3-perl
 - libmagic-dev
 - libpq-dev
 - libssl-dev
 - nettle-dev
 - pkg-config
 - uuid-dev
 - make (>= 4.0)
 
For Debian users, we can also check out one of these following branches (beryl/jessie or beryl/stretch). Then we can use
`dpkg-buildpackage -us -uc -rfakeroot` in order to generate Debian packages.
Using these following branches (agathe/* and beryl/wheezy) are discouraged.

## Installing StoriqOne Daemon
 - First, you need PostgreSQL (v9.4 or upper). For Debian users, you also need `postgresql-contrib` package before creating storiqone database. You can install PostgreSQL on another host.
 - Then, install theses followings packages `storiqone-daemon` and `storiqone-scripts`. `storiqone-daemon` has a dependancy to `storiqone-plugin-db`
   which is provided by `storiqone-plugin-db-postgresql`. `storiqone-plugin-db-postgresql` contains an assistant which will create the database.
 - After, you have to check the file `/etc/storiq/storiqone.conf`. This file contains login and password.
 - Run this following command `storiqonectl config`
 - And finally, starts the storiqone service. `service storiqoned start`
