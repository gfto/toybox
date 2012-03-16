#!/bin/bash

# Grab default values for $CFLAGS and such.

source ./configure

scripts/genconfig.sh

TOYFILES=$(cat generated/build_files 2>/dev/null)

echo "Compile toybox..."

do_loudly()
{
  [ ! -z "$V" ] && echo "$@"
  "$@"
}

do_loudly ${CROSS_COMPILE}${CC} $CFLAGS -I . -o toybox_unstripped $OPTIMIZE \
  main.c lib/*.c $TOYFILES -Wl,--as-needed,-lutil,--no-as-needed || exit 1
do_loudly ${CROSS_COMPILE}${STRIP} toybox_unstripped -o toybox || exit 1
# gcc 4.4's strip command is buggy, and doesn't set the executable bit on
# its output the way SUSv4 suggests it do so.
do_loudly chmod +x toybox || exit 1
