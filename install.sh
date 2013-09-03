#!/bin/bash

THIS=$( cd "$( dirname "$0" )" && pwd )

usage()
{
        echo "emma-node install"
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
cp -R $THIS/dep/contiki/* $DEST/

# Patch Contiki OS
patch -p2 --directory=$DEST --input=$THIS/patch/patch-slip-raven.diff
patch -p2 --directory=$DEST --input=$THIS/patch/patch_erbium.diff
patch -p2 --directory=$DEST --input=$THIS/patch/raven-main_patch.diff
patch -p2 --directory=$DEST --input=$THIS/patch/border-router_patch.diff
patch -p2 --directory=$DEST --input=$THIS/patch/patch-slip-activate-control-overrun.diff

# Temporary CFS patch
patch -p2 --directory=$DEST --input=$THIS/patch/cfs_patch.diff

# Copy EMMA application and example
cp -r $THIS/src/contiki/apps/emma-node $DEST/apps/
cp -r $THIS/src/contiki/examples/emma-node-example $DEST/examples/

# Copy EMMA documentation
cp -r $THIS/doc $DEST/../

# Copy 'Network' script
cp $THIS/network.sh $DEST/../

# Install compilator toolchain
sudo apt-get install gcc-avr avr-libc avrdude

echo "Done."
