#!/bin/sh
set -ex
rm -rf docker_out
mkdir docker_out
docker build . -t ps1dev_eliasdaler -o docker_out
echo "ISO was built into docker_out/game.iso"
