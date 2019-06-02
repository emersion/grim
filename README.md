# grim

Grab images from a Wayland compositor. Works great with [slurp] and with [sway] >= 1.0.

## Example usage

Screenshoot all outputs:

```sh
grim screenshot.png
```

Screenshoot a specific output:

```sh
grim -o DP-1 screenshot.png
```

Screenshoot a region:

```sh
grim -g "10,20 300x400" screenshot.png
```

Select a region and screenshoot it:

```sh
grim -g "$(slurp)" screenshot.png
```

Use a timestamped filename:

```sh
grim $(xdg-user-dir PICTURES)/$(date +'%Y-%m-%d-%H%M%S_grim.png')
```

Screenshoot and copy to clipboard:

```sh
grim - | wl-copy
```

Grab a screenshot from the focused monitor under Sway, using `swaymsg` and `jq`:

```sh
grim -o $(swaymsg -t get_outputs | jq -r '.[] | select(.focused) | .name') screenshot.png
```

## Package manager installation

* Arch Linux: `pacman -S grim`

## Building from source

Install dependencies:

* meson
* wayland
* cairo
* libjpeg (optional)

Then run:

```sh
meson build
ninja -C build
```

To run directly, use `build/grim`, or if you would like to do a system installation (in `/usr/local` by default), run `ninja -C build install`. 

## Contributing

Either [send GitHub pull requests][github] or [send patches on the mailing list][ml].

## License

MIT

[slurp]: https://github.com/emersion/slurp
[sway]: https://github.com/swaywm/sway
[github]: https://github.com/emersion/grim
[ml]: https://lists.sr.ht/%7Eemersion/public-inbox
