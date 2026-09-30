#!/bin/bash
cd OBJ || exit
$(printf "%s%s%s" "$JREPATH" "$(test -z "$JREPATH" || echo /)" java) -jar -Xms1G dev_game.jar "$@"
