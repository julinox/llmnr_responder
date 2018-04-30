#!/bin/bash

########################################################################
# Variables                                                            #
########################################################################

PATH=/sbin:/bin:/usr/sbin:/usr/bin
EXEC=llmnrd
DIR=/etc/llmnr
INIT_DIR=/etc/init.d
INIT_SCRIPT=llmnr
__INIT_SCRIPT=.llmnr.sh

########################################################################
# Functions                                                            #
########################################################################

check_root()
{
    ID=`id -u`
    if [ "$ID" -ne "0" ]; then
        echo "Need root privileges to run this script"
        exit 1
    fi
    return 0
}

check_status()
{
    if [ "$?" -ne 0 ]; then
        echo "$1"
        exit 1
    fi
}

print_action()
{
    echo -n "$1..."
}

print_ok()
{
    echo -n "OK"
    echo ""
}

########################################################################
# Main                                                                 #
########################################################################

print_action "[Checking root]"
check_root
print_ok

print_action "[Checking executable ('$EXEC') file]"
test -f $EXEC
check_status "Not found. Try recompile it"
chmod 755 $EXEC
print_ok

print_action "[Checking init script ('$__INIT_SCRIPT') file]"
test -f $__INIT_SCRIPT
check_status "Not found"
chmod 755 $__INIT_SCRIPT
print_ok

print_action "[Making dir '$DIR']"
test -d $DIR
if [ "$?" -eq "1" ]; then
    mkdir $DIR
    check_status
fi
print_ok

print_action "[Coping '$EXEC' into '$DIR']"
cp $EXEC $DIR
check_status
print_ok

print_action "[Coping '$__INIT_SCRIPT' into '$INIT_DIR/$INIT_SCRIPT']"
cp $__INIT_SCRIPT $INIT_DIR/$INIT_SCRIPT
check_status
print_ok

print_action "[Setting daemon start]"
insserv $INIT_SCRIPT

if [ "$?" -eq "1" ]; then
    echo "failed insserv"
    exit 1
fi

make cleanall
echo "All done"
exit 0

