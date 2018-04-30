#!/bin/sh
########################################################################
# insserv LSB header                                                   #
########################################################################

### BEGIN INIT INFO
# Provides:          llmnrd
# Required-Start:    $network $syslog dbus hostname
# Required-Stop:     $network $syslog dbus hostname
# Should-Start:      $time
# Should-Start:      $time
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Link local multicast name resolution
# Description:       Zeroconf daemon for configuring your network
#                    RFC 4795
### END INIT INFO

########################################################################
# Variables                                                            #
########################################################################

PATH=/sbin:/bin:/usr/sbin:/usr/bin
DIR=/etc/llmnr
DAEMONFILE=llmnrd
PIDFILE=llmnr.pid
NSSFILE=/etc/nsswitch.conf

########################################################################
# Functions                                                            #
########################################################################

check_file()
{
    test -f $1

    if [ "$?" -eq 0 ]; then
        return 0
    fi
    return 1
}

__start()
{
    echo -n "[Starting $DAEMONFILE]..."
    check_file "$DIR/$DAEMONFILE"
    
    if [ "$?" -eq "1" ]; then
        echo "Fail ($DIR/$DAEMONFILE does not exists)"
        return 1
    fi
    
    start-stop-daemon -S -x $DIR/$DAEMONFILE
    
    if [ "$?" -eq "0" ]; then
	sed -i '/^hosts:/ s/rnmll/llmnr/' $NSSFILE
        echo "OK"
        return 0
    fi
    return 0
}

__stop()
{
    echo -n "[Stopping $DAEMONFILE]..."
    check_file "$DIR/$PIDFILE"
    
    if [ "$?" -eq "0" ]; then
        start-stop-daemon -K -p $DIR/$PIDFILE --remove-pidfile
    else
        start-stop-daemon -K -n $DAEMONFILE
    fi
    
    if [ "$?" -eq "0" ]; then
	sed -i '/^hosts:/ s/llmnr/rnmll/' $NSSFILE
        echo "OK"
        return 0
    fi
    return 1
}

__status()
{
    echo -n "[Checking $DAEMONFILE]..."
    check_file $DIR/$PIDFILE

    if [ "$?" -eq "0" ]; then
        start-stop-daemon -T -p $DIR/$PIDFILE
    else
        start-stop-daemon -T -n $DAEMONFILE
    fi

    case $? in
        0)
            PID=$(pidof $DAEMONFILE)
            echo "Running (PID $PID)"
            ;;
        1)
            echo "Not running (pid file exists)"
            ;;
        3)
            echo "Not running"
            ;;
        4)
            echo "Unknow status"
            ;;
    esac
    return 0
}

########################################################################
# Main                                                                 #
########################################################################

case "$1" in
    start)
        __start
        ;;

    stop)
        __stop
        ;;
		
    status)
        __status
        ;;

    reload)
        __stop
        if [ "$?" -eq "0" ]; then
            __start
        fi
        ;;    
    *)
        echo "Use: /etc/init.d/$NAME {start|stop|reload|status}"
        ;;
esac
exit 0
