#base on cronolog-1.6.2    
** http://cronolog.org/
##need iniparser (included) 
** http://ndevilla.free.fr/iniparser/
##need libgearman-devel
##nead boost144-devel

#ApacheLog send to Gearman Server

#INSTALL
###export CFLAGS="-I/usr/include/boost141 -I../iniparser/src"
###export LDFLAGS="-lgearman -L../iniparser -liniparser"

###./configure
###make
###make install

#USES
##CRONOLOG CONFIG
###vi /etc/cronolog_gm.ini
####[gearman]
####servers="10.128.5.11:4730"
####workercommand="cronolog_svr"
####exceptext=""
####timeout=1000

##APACHE CONFIG
###CustomLog "|cronolog --period=1minute /home/httpd/logs/access_log" combined env=!do_not_log
###ErrorLog "|cronolog --period=1minute /home/httpd/logs/error_log"

