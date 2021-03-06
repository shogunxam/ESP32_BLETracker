#! /bin/bash
TOOLCHAIN=~/.platformio/packages/toolchain-xtensa32

# Validate commandline arguments
if [ -z "$1" ]; then
    echo "usage: $0 <elf file> [<backtrace-text>]" 1>&2
    echo "reads from stdin if no backtrace-text is specified" 2>&2
    exit 1
fi
elf="$1"
if ! [ -f $elf ]; then
    echo "ELF file not found ($elf)" 1>&2
    exit 1
fi

if [ -z "$2" ]; then
    echo "reading backtrace from stdin"
    bt=/dev/stdin
elif ! [ -f "$2" ]; then
    echo "Backtrace file not found ($2)" 1>&2
    exit 1
else
    bt=$2
fi

./decoder.py -p ESP32 -t "$TOOLCHAIN" -e $elf $bt