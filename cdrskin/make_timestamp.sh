#!/bin/sh

# Create version timestamp cdrskin/cdrskin_timestamp.h
# to be executed within  ./libburn-*  or ./cdrskin-*

timestamp="$(date -u '+%Y.%m.%d.%H%M%S')"
echo "Version timestamp :  $timestamp"
echo '#define Cdrskin_timestamP "'"$timestamp"'"' >cdrskin/cdrskin_timestamp.h

