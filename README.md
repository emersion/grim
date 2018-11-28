# grim

Grab images from a Wayland compositor. Works great with [slurp](https://github.com/emersion/slurp) and also with [sway](https://github.com/swaywm/sway/) >= 1.0.

## Building

Install dependencies:
* meson
* wayland
* cairo

Then run:

```shell
meson build
ninja -C build
build/grim
```

## Usage

```shell
grim screenshot.png # Screenshoot all outputs
grim -o DP-1 screenshot.png # Screenshoot a specific output
grim -g "10,20 300x400" screenshot.png # Screenshoot a region
slurp | grim -g - screenshot.png # Select a region and screenshoot it
grim $(xdg-user-dir PICTURES)/$(date +'%Y-%m-%d-%H%M%S_grim.png') # Use a timestamped filename
grim -o $(swaymsg -t get_outputs | jq -r '.[] | select(.focused==true) | .name') /tmp/screenshot.png # Grab a screenshot from the focused monitor
```

## Installation

### Arch Linux

    pacman -S grim

## Contributing

Either [send GitHub pull requests][1] or [send patches on the mailing list][2].

## License

MIT

[1]: https://github.com/emersion/grim
[2]: https://lists.sr.ht/%7Eemersion/public-inbox
