#!/bin/bash

THIS=$( cd "$( dirname "$0" )" && pwd )

usage()
{
	echo "install.sh DESTINATION"
	echo "		DESTINATION: The location of a Contiki directory (David Kopf branch)"
}

if [ ! -d "$1" ]; then
	usage
	exit
else
	DEST="$1"
fi

echo "Installing 'emma-node' ..."

# Patch Contiki OS
patch -p2 --directory=$DEST --input=$THIS/patch/patch-slip-raven.diff
patch -p2 --directory=$DEST --input=$THIS/patch/patch_erbium.diff
patch -p2 --directory=$DEST --input=$THIS/patch/raven-main_patch.diff
patch -p2 --directory=$DEST --input=$THIS/patch/border-router_patch.diff

# Copy EMMA application and example
cp -r $THIS/src/contiki/apps/emma-node $DEST/apps/
cp -r $THIS/src/contiki/examples/emma-node-example $DEST/examples/

# Install compilator toolchain
sudo apt-get install gcc-avr avr-libc avrdude

echo "Done."
