#!/bin/sh
# run with sudo!
set -ex
mount /dev/sda1 /mnt/sdcard
cp ./build_games/cat_adventure/game.iso /mnt/sdcard
umount /dev/sda1
