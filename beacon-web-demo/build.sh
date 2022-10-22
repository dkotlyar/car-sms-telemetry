#!/bin/bash

DIR=$(dirname `readlink -f $0`)
cd $DIR
ng build
cp -r dist/ docker/
./docker/build-docker.sh
