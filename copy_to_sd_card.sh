#!/bin/sh
# run with sudo!
set -ex
mount /dev/sdc1 /mnt/sdcard
cp ./build_games/cat_adventure/game.iso /mnt/sdcard
umount /dev/sdc1
