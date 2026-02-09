#!/bin/sh -eu

make ${MAKE_OPTS:-} -C kernel kernel.elf

for MK in $(ls apps/*/Makefile)
do
    APP_DIR=$(dirname $MK)
    APP=$(basename $APP_DIR)
    make ${MAKE_OPTS:-} -C $APP_DIR $APP
done

if [ "${1:-}" = "run" ]
then
    MIKANOS_DIR=$PWD $(ghq root)/github.com/sasurau4/mikan-os/devenv/run_mikanos.sh
fi