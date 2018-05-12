# grim

Grab images from a Wayland compositor.

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

## License

MIT
