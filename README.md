# grim

Grab images from a Wayland compositor. Works well with
[slurp](https://github.com/emersion/slurp).

It currently works on Sway 1.0 alpha.

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
grim $(date +'%Y-%m-%d-%H%M%S_grim.png') # Use a timestamped filename
```

## License

MIT
