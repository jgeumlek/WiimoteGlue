#Build Utilities

This directory contains the script used to generate some code
used in commandline.c to translate key names to constants.

This script is not needed once WiimoteGlue is successfully compiled.

It is not a fully automated solution; it currently requires two instances of elbow/grease in copying and pasting.

1. Open the \<linux/input.h\> header file, and find the section with the KEY_\* constants.

2. Copy and paste the whole section of KEY defines to a file named 'keycodes'

3. Run generate_key_codes and it'll print the needed C code to its standard output.

4. Copy and paste the generated code into 'get_output_key()' in commandline.c (Be sure to replace only the keyboard key section of that file; you need to leave the gamepad button section alone. KEY_ESC should be on the first line you replace.)

5. Forgive the author for making you go through all this hassle.
