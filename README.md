# Zig Language Support plugin

This package is a WIP to provide language support for Zig. It uses zig's
builtin parser via a library.

![Example](https://user-images.githubusercontent.com/380158/279830656-9fc7aa74-175c-4826-a698-401751a07464.png)

It was forked from kdev-rust and borrows code from kdev-python so it is GPL.

## Compiling

First, build and install kdevelop from source so the test libraries are availble.

### Build kdev-zig

```
mkdir build
cd build
cmake -G ninja ..
ninja install
```

Use `zig test duchain/kdevzigastparser.zig` to run zig tests.
Run the `duchaintest` for kdev-zig tests.

In case you run into problems with it use `ninja uninstall`.

To enable debug logging set the following env var.

```bash
export QT_LOGGING_RULES="kdevelop.languages.zig.duchain.debug=true;"
```

## Running

Then run KDevelop.

## Dependencies

You must have zig 0.11.0+ installed somewhere where FindZig.cmake
will find it (eg ".local/bin").
