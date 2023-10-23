# Zig Language Support plugin

This package is a WIP to provide language support for Zig. It uses zig's
builtin parser.

It was forked from kdev-rust.

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

## Running

Then run KDevelop.

## Dependencies

You must have zig 0.11.0+ installed somewhere where FindZig.cmake
will find it (eg ".local/bin").
