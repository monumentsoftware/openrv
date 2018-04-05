#!/bin/bash
#
# This script is automatically called by the xcode OpenRV framework 
# project to generate orv_config.h and orv_version.h

IFS=" "
configfilename="../libopenrv/public/libopenrv/orv_config.h"
versionfilename="../libopenrv/public/libopenrv/orv_version.h"
verparts=()
copyright=""
rm -f $versionfilename
rm -f $configfilename
echo "#ifndef OPENRV_ORV_VERSION_H" >> $versionfilename
echo "#define OPENRV_ORV_VERSION_H" >> $versionfilename
while read -r line
do
    # get version parts
    var=`echo $line | grep -o "set(ORV_[a-zA-Z0-9 \t_]*)"`
    if [ -n "$var" ]; then
        var=${var/set(ORV_/"#define LIBOPENRV_"}
        var=${var/)/}
        echo "${var}" >> $versionfilename 
        x=$(echo $var|tr -s ' ')
        y=($var)
        verparts+=(${y[2]})
    fi
    
    # get copyright string
    var=`echo $line | grep "set(ORV_COPYRIGHT_STRING"`
    if [ -n "$var" ]; then
        var=`echo $var | grep -o "\"[a-zA-Z0-9() -_]*\""`
        copyright=$var
    fi
done < ../version.cmake 
printf "#define LIBOPENRV_VERSION 0x%02x%02x%02x\n" ${verparts[0]} ${verparts[1]} ${verparts[2]} >> $versionfilename
printf "#define LIBOPENRV_VERSION_STRING \"%d.%d.%d\"\n" ${verparts[0]} ${verparts[1]} ${verparts[2]} >> $versionfilename
printf "#define LIBOPENRV_COPYRIGHT_STRING $copyright\n" >> $versionfilename
echo "#endif" >> $versionfilename

# config file is empty for now
touch $configfilename

