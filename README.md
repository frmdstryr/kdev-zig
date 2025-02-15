# Zig Language Support plugin

This a plugin for [KDevelop](https://kdevelop.org/) that adds language support for Zig. It uses zig's
builtin parser via a library. 

### Features
- Syntax error messages
- Syntax highlighting of structs, fns, enums. Rainbow colored local vars
- Goto decl, view doc comments on hover.
- Highlight usages on hover, lookup uses of fns, vars, etc..
- Resolve imports from zig std lib and user defined packages
- Evalutate some comptime expressions (enum switches, if exprs, basic math exprs)
- Basic typed fn resolution for fns that return structs or typed params (still WIP)
- Hints for type mismatches, missing args, invalid enum/struct fields, etc..

## Screenshots

Resolving basic math expressions
![image](https://github.com/frmdstryr/kdev-zig/assets/380158/581f1ec6-f92a-408c-97d3-58f8b85a4800)


The following shows it is able to resolve methods of a USBCDC driver using zig's builtin buffered writer to print.
![](https://user-images.githubusercontent.com/380158/284731452-bca7eb00-0f1c-44aa-8096-f2f0e4042f29.png)

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

> Note: Make sure you have a file assoication setup for zig in "File associations" or it will not parse files. In KDE Plasma add a new entry`text/x-zig` for all `*.zig` files.

Then run KDevelop.

## Dependencies

You must have zig 0.12 or 0.13 installed somewhere where FindZig.cmake will find it (eg ".local/bin").

# License

It was forked from kdev-rust and borrows code from kdev-python so licensed under the GPL.
