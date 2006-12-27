#!/bin/sh

#
# convert_man_to_html.sh - ts A61214
#
# Generates a HTML version of man page cdrskin.1
#
# To be executed within the libburn toplevel directory (like ./libburn-0.2.7)
#

# set -x

man_dir=$(pwd)"/cdrskin"
export MANPATH="$man_dir"
manpage="cdrskin"
raw_html=$(pwd)/"cdrskin/raw_man_1_cdrskin.html"
htmlpage=$(pwd)/"cdrskin/man_1_cdrskin.html"

if test -r "$manpage"
then
  dummy=dummy
else
  echo "Cannot find readable man page source $1" >&2
  exit 1
fi

if test -e "$man_dir"/man1
then
  dummy=dummy
else
  ln -s . "$man_dir"/man1
fi

if test "$1" = "-work_as_filter"
then

#  set -x

  sed \
  -e 's/<meta name="generator" content="groff -Thtml, see www.gnu.org">/<meta name="generator" content="groff -Thtml, via man -H, via cdrskin\/convert_man_to_html.sh">/' \
  -e 's/<meta name="Content-Style" content="text\/css">/<meta name="Content-Style" content="text\/css"><META NAME="description" CONTENT="man page of cdrskin"><META NAME="keywords" CONTENT="man cdrskin, manual, cdrskin, CD, CD-RW, CD-R, burning, cdrecord, compatible"><META NAME="robots" CONTENT="follow">/' \
  -e 's/<title>CDRSKIN<\/title>/<title>man 1 cdrskin<\/title>/' \
  -e 's/<h1 align=center>CDRSKIN<\/h1>/<h1 align=center>man 1 cdrskin<\/h1>/' \
  -e 's/<body>/<body BGCOLOR="#F5DEB3" TEXT=#000000 LINK=#0000A0 VLINK=#800000>/' \
  -e 's/<b>Overview of features:<\/b>/\&nbsp;<BR><b>Overview of features:<\/b>/' \
  -e 's/<b>Track recording model:<\/b>/\&nbsp;<BR><b>Track recording model:<\/b>/' \
  -e 's/In general there are two types of tracks: data and audio./\&nbsp;<BR>In general there are two types of tracks: data and audio./' \
  -e 's/While audio tracks just contain a given/\&nbsp;<BR>While audio tracks just contain a given/' \
  -e 's/<b>Recordable CD Media:<\/b>/\&nbsp;<BR><b>Recordable CD Media:<\/b>/' \
  -e 's/<b>Recordable DVD Media:<\/b>/\&nbsp;<BR><b>Recordable DVD Media:<\/b>/' \
  -e 's/<b>Drive preparation and addressing:<\/b>/\&nbsp;<BR><b>Drive preparation and addressing:<\/b>/' \
  -e 's/If you only got one CD capable drive/\&nbsp;<BR>If you only got one CD capable drive/' \
  -e 's/^Alphabetical list of options/\&nbsp;<BR>Alphabetical list of options/' \
  -e 's/and for all others\.<\/td><\/table>/and for all others.<\/td><\/table>  <BR><HR><FONT SIZE=-1><CENTER>(HTML generated from '"$manpage"'.1 on '"$(date)"' by '$(basename "$0")' )<\/CENTER><\/FONT>/' \
  -e 's/See section EXAMPLES/See section <A HREF="#EXAMPLES">EXAMPLES<\/A>/' \
  <"$2" >"$htmlpage"

  set +x

  chmod u+rw,go+r,go-w "$htmlpage"
  echo "Emerged file:"
  ls -l "$htmlpage"

else

  export BROWSER='cp "%s" '"$raw_html"
  man -H "$manpage"
  "$0" -work_as_filter "$raw_html"
  rm "$raw_html"
  rm "$man_dir"/man1

fi