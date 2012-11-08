#base on cronolog-1.6.2    
** http://cronolog.org/
##need iniparser 
** http://ndevilla.free.fr/iniparser/
** http://ndevilla.free.fr/iniparser/iniparser-3.1.tar.gz
** git clone http://github.com/ndevilla/iniparser.git
##need libgearman-devel
##need boost144-devel

#ApacheLog send to Gearman Server

#INSTALL

###./configure
###make
###make install

#USES
##CRONOLOG CONFIG
###vi /etc/cronolog_gm.ini
####[gearman]
####enable=1
####servers="127.0.0.1:4730"
####workercommand="cronolog_svr"
####timeout=1000
####usegzip=1
####debug=1
####debugfile=/var/log/cronolog_gearman.log


##APACHE CONFIG
###CustomLog "|cronolog --period=1minute /home/httpd/logs/access_log" combined env=!do_not_log
###ErrorLog "|cronolog --period=1minute /home/httpd/logs/error_log"

