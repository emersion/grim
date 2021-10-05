grim(1)

# NAME

grim - grab images from a Wayland compositor

# SYNOPSIS

*grim* [options...] [output-file]

# DESCRIPTION

grim is a command-line utility to take screenshots of Wayland desktops. For now
it requires support for the screencopy protocol to work. Support for the
xdg-output protocol is optional, but improves fractional scaling support.

grim will write an image to _output-file_, or to a timestamped file name in
*$GRIM_DEFAULT_DIR* if not specified. If *$GRIM_DEFAULT_DIR* is not set, it
falls back first to *$XDG_PICTURES_DIR* and then to the current working
directory. If _output-file_ is *-*, grim will write the image to the standard
output instead.

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

*-l* <level>
	Set the output PNG's filetype compression level to _level_. By default,
	the PNG compression level is 6 on a scale from 0 to 9. Level 9 gives
	the highest compression ratio, but may be slow; level 1 gives a lower
	compression ratio, but is faster. Level 0 does no compression at all,
	and produces very large files; it can be useful when grim is used
	in a pipeline with other commands.

*-o* <output>
	Set the output name to capture.

*-c*
	Include cursors in the screenshot.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about grim development, see
https://github.com/emersion/grim.
