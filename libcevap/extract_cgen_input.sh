#!/bin/sh

copy_mode=0

while true
do
  read line
  if test "$copy_mode" = "0"
  then
    if echo " $line" | grep '^ Cgen=' >/dev/null 2>&1
    then
      copy_mode=1
      if echo " $line" | grep '^ Cgen=..' >/dev/null 2>&1
      then
        echo " $line" | sed -e 's/^ Cgen=//'
      fi
    elif echo " $line" | grep '^ =end Model=' >/dev/null 2>&1
    then
break
    fi
  else
    if test " $line" = " @"
    then
      copy_mode=0
      echo "@"
    else
      echo " $line" | sed -e 's/^ //'
    fi
  fi
done
