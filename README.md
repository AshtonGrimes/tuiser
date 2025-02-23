# tuiser

`tuiser` is a TUI SERial monitor for Linux.

## Usage

The device file descriptor, baudrate, monitoring state, and display mode can be set in the program or as program arguments.

Baudrates must be one of the baudrates specified in `man 3 termios`, ignoring platform-specific or non-POSIX options.

#### Shortcuts
- ^W, ^A, ^S, ^D -- Move cursor
- ^Z -- Change display mode
- ^X -- Toggle monitoring
- ^C -- Quit

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
