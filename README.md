# qlprint
Command-line utility for printing to Brother QL series label printers.

Tested with:
  * QL-570
  
Hopefully also works with:
  * QL-500/550
  * QL-560
  * QL-580N
  * QL-650TD
  * QL-700
  * QL-710W
  * QL-720W
  * QL-1050
  * QL-1060N

Inspired, though not derived from, the [ql570](https://github.com/sudomesh/ql570.git) tool. Licensed under the GPLv3 nevertheless.

## Building
Requires `GNU make` and `libpng` (with development headers), with `pkg-config` to locate libpng headers & libs.

Simply run `make` in this directory, e.g.:
```
$ make
cc -std=c11 -Wall -Wextra -g -Iinclude -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809 -I/usr/include/libpng12 -c src/main.c -o build/main.o
cc -std=c11 -Wall -Wextra -g -Iinclude -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809 -I/usr/include/libpng12 -c src/ql.c -o build/ql.o
cc -std=c11 -Wall -Wextra -g -Iinclude -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809 -I/usr/include/libpng12 -c src/loadpng.c -o build/loadpng.o
cc -lpng12 build/main.o build/ql.o build/loadpng.o -o build/qlprint
$
```

## Running
```
Syntax:
  qlprint [-p lp] -i
          [-p lp] [-m margin] [-a] [-C|-D] [-W width] [-L length] [-Q] [-n num] [-t threshold] png...
Where:
  -p lp         Printer port (default /dev/usb/lp0)
  -i            Print status information only, then exit
  -m margin     Margin (dots)
  -a            Enable auto-cut
  -C            Request continuous-length-tape when printing (error if not)
  -D            Request die-cut-labels when printing (error if not)
  -W width      Request particular width media when printing (error if not)
  -L length     Request particular length media when printing (error if not)
  -Q            Prioritise quality of speed
  -n num        Print num copies
  -t threshold  Threshold for black-vs-white (default 128, i.e. 0-127=black)
  png...        One or more png files to print

```

The PNG files are converted to monochrome internally. The black-vs-white
threshold for this conversion may be tuned with the `-t threshold` argument.

Image height is limited to the capability of the printer (720 for most, 1296
for 1050/1060N models). Attempting to print larger images will fail.

On successful printing, the exit code is zero; in case of any error, the exit
code is non-zero and an error message is printed to stderr.

## Examples

### Show printer status information
Here with a narrow continuous-length-tape cartridge loaded.
```
$ ./build/qlprint -i
          Printer: QL-570
             Mode: no-auto-cut
           Errors: none
       Media type: continuous-length-tape
 Media width (mm): 29
$
```

### Printing with auto-cutter enabled:
```
$ ./build/qlprint -a example.png
example.png (135x135) OK
$
```

### Printing two images, cutting only once
```
$ ./build/qlprint -a example.png example.png
example.png (135x135) OK
example.png (135x135) OK
$
```

### Printing two copies of the one image, cutting after each
```
$ ./build/qlprint -a -n 2 example.png
example.png (135x135) OK
example.png (135x135) OK
$
```

### Print only on the correct media type
Assuming a continuous-length-tape cartridge is installed:
```
$ ./build/qlprint -C example.png
example.png (135x135) OK
$
```
...otherwise:
```
$ ./build/qlprint -C example.png
Printer reported error(s): replace-media
$
```

