#Build Utilities

This directory contains the script used to generate some code
used in commandline.c to translate key names to constants.

This script is not needed once WiimoteGlue is successfully compiled.

Run the script, optionally giving it a particular path to the linux/input.h header file. It will generate key_codes.c
Replace key_codes.c with the new one, and you should be good to go.