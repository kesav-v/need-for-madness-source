#!/bin/bash
# Compiles the default-package game sources plus ibxm/ds modules. The com/nfm tree is a
# partial duplicate of those classes (and includes a truncated xtGraphics); mixing both
# breaks javac with duplicate-class errors. OpenJDK 8 through 23 are supported this way.
echo "$($(printf "%s%s%s" "$JDKPATH" "$(test -z "$JDKPATH" || echo /)" java) -version 2>&1 | head -n1)"
echo "JDKPATH=$JDKPATH"
echo "JREPATH=$JREPATH"
echo
mkdir -p BUILD
$(printf "%s%s%s" "$JDKPATH" "$(test -z "$JDKPATH" || echo /)" javac) "$@" -encoding UTF-8 -d BUILD \
  ./*.java \
  ibxm/*.java \
  ds/nfm/*.java \
  ds/nfm/mod/*.java \
  ds/nfm/xm/*.java
cd BUILD || exit
$(printf "%s%s%s" "$JDKPATH" "$(test -z "$JDKPATH" || echo /)" jar) cmvf ../MANIFEST.MF ../OBJ/dev_game.jar $(find . -iname "*.class") >/dev/null
