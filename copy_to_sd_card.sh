#!/bin/sh
# run with sudo!
set -ex
mount /dev/sdb1 /mnt/sdcard
cp ./build_games/cat_adventure/game.iso /mnt/sdcard
umount /dev/sdb1
