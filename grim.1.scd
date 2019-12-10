grim(1)

# NAME

grim - grab images from a Wayland compositor

# DESCRIPTION

*grim* [options...] [output-file]

# SYNOPSIS

grim is a command-line utility to take screenshots of Wayland desktops. For now
it requires support for the screencopy protocol to work. Support for the
xdg-output protocol is optional, but improves fractional scaling support.

grim will write an image to _output-file_, or to a timestamped file name in
*$GRIM_DEFAULT_DIR* if not specified. If *$GRIM_DEFAULT_DIR* is not set, it
defaults to *$XDG_PICTURES_DIR*. If _output-file_ is *-*, grim will output the
image to the standard output instead.

# OPTIONS

*-h*
	Show help message and quit.

*-s* <factor>
	Set the output image's scale factor to _factor_. By default, the scale
	factor is set to the highest of all outputs.

*-g* "<x>,<y> <width>x<height>"
	Set the region to capture, in layout coordinates.

	If set to *-*, read the region from the standard input instead.

*-t* <type>
	Set the output image's file format to _type_. By default, the filetype
	is set to *png*, valid values are *png*, *jpeg* or *ppm*.

*-q* <quality>
	Set the output jpeg's filetype compression rate to _quality_. By default,
	the jpeg quality is *80*, valid values are between 0-100.

*-o* <output>
	Set the output name to capture.

*-c*
	Include cursors in the screenshot.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about grim development, see
https://github.com/emersion/grim.
