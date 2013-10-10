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

echo -e  "=========================="
echo -e  "  Contiki installation"
echo -e  "=========================="
cp -R $THIS/dep/* $DEST/
echo -e ""

# Patch Contiki OS
echo -e  "==================="
echo -e  "  Contiki patching"
echo -e  "==================="
patch -p2 --directory=$DEST --input=$THIS/patch/patch-slip-raven.diff
patch -p2 --directory=$DEST --input=$THIS/patch/patch_erbium.diff
patch -p2 --directory=$DEST --input=$THIS/patch/raven-main_patch.diff
patch -p2 --directory=$DEST --input=$THIS/patch/border-router_patch.diff
patch -p2 --directory=$DEST --input=$THIS/patch/patch-slip-activate-control-overrun.diff

# Temporary CFS patch
patch -p2 --directory=$DEST --input=$THIS/patch/cfs_patch.diff
echo -e ""

# Copy EMMA application and example
echo -e  "=========================="
echo -e  "  emma-node installation"
echo -e  "=========================="
cp -r $THIS/src/apps/emma-node $DEST/apps/
cp -r $THIS/src/examples/emma-node-example $DEST/examples/
cp $THIS/network.sh $DEST/../
echo -e ""

# Copy EMMA documentation
echo -e  "=========================="
echo -e  "     doc installation"
echo -e  "=========================="
cp -r $THIS/doc $DEST/../
echo -e ""

# Install compilator toolchain
echo -e  "=========================="
echo -e  "   Package installation"
echo -e  "=========================="
sudo apt-get install gcc-avr avr-libc avrdude
echo -e ""

echo "Done."
