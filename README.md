# tuiser

`tuiser` is a TUI SERial monitor for Linux.

File descriptors, baudrates, display modes can be set as program arguments or at any time in the interface.

Baudrates must be one of the baudrates specified in `man 3 termios`, ignoring platform-specific or non-POSIX options. Check the BAUD_MAP values in the source code for an exhaustive list.

## Usage

#### Shortcuts
- ^W, ^A, ^S, ^D -- Move cursor
- ^Z -- Change display mode
- ^X -- Toggle monitoring
- ^C -- Quit

#### Display modes
- `char` (default) -- ASCII text
- `graph` -- 2d graph of signed byte value over time
- `hex` -- Unsigned hexadecimal 
- `uint` -- Unsigned decimal
- `int` -- Signed decimal

`hex`, `uint`, and `int` write bytes in a 16-column grid, left to right, top to bottom.

#### Arguments
Options can be used together in any order. Note that `-r` will fail if `-d` is unspecified or can't be opened.

```sh
$ tuiser -h
$ tuiser -m graph
$ tuiser --device /dev/ttyACM0 --baud 115200
$ tuiser -r -d /dev/ttyUSB0 -b 38400
$ tuiser --no-read --mode uint --device /dev/ttyACM1
```

## License
`tuiser` is free software, licensed under the GNU General Public license version 3.0 (GPL-3.0). See the LICENSE file for details.
