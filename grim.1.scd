grim(1)

# NAME

grim - grab images from a Wayland compositor

# DESCRIPTION

*grim* [options...] <output-file>

# SYNOPSIS

grim is a command-line utility to take screenshots of Wayland desktops. For now
it requires support for Orbital's screenshooter to work. Support for the
xdg-output protocol is optional, but improves fractional scaling support.

# OPTIONS

*-h*
	Show help message and quit.

*-s* <factor>
	Set the output image's scale factor to _factor_. By default, the scale
	factor is set to the highest of all outputs.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about grim development, see
https://github.com/emersion/grim.
