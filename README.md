Copyright (C) 2020 Western Digital Corporation or its affiliates.

# libzbd

*libzbd* is a library providing functions simplifying access to zoned block
device information and the execution of zone management operations.

An example command line application using *libzbd* is available under the
tools directory. Additionally, graphical user interface applications are also
implemented to visually represent the state and usage of zones of a zoned
block device.

## License

*libzbd* is distributed under the terms of the of the GNU Lesser General Public
License Version 3 or any later verison ((SPDX: *LGPL-3.0-or-later*). A copy of
this license with *libzbd* copyright can be found in the file
[LICENSES/LGPL-3.0-or-later.txt](LICENSES/LGPL-3.0-or-later.txt).

All example applications under the tools directory are distributed under
the terms of the GNU General Public License version 3, or any later version.
A copy of this license can be found in the file
[LICENSES/GPL-3.0-or-later.txt](LICENSES/GPL-3.0-or-later.txt).

If *libzbd* license files are missing, please see the LGPL version 3 text
[here](https://www.gnu.org/licenses/lgpl-3.0.html) and the GPL version 3 text
[here](https://www.gnu.org/licenses/gpl-3.0.html).

All source files in *libzbd* contain the LGPL v3 or GPL v3 license SPDX short
identifiers in place of the full license text.

```
SPDX-License-Identifier: LGPL-3.0-or-later
SPDX-License-Identifier: GPL-3.0-or-later
```

Some files such as the `.gitignore` file are public domain specified by the
CC0 1.0 Universal (CC0 1.0) Public Domain Dedication. These files are
identified with the following SPDX header.

```
SPDX-License-Identifier: CC0-1.0
```

See the file [LICENSES/CC0-1.0.txt] for the full text of this license.

*libzbd* and all its example applications are distributed "as is," without
technical support, and WITHOUT ANY WARRANTY, without even the implied warranty
of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

## Contributions and Bug Reports

Contributions are accepted as github pull requests or via email (`git
send-email` patches). Any problem may also be reported through github issues
page or by contacting:

* Damien Le Moal (damien.lemoal@wdc.com)
* Niklas Cassel (niklas.cassel@wdc.com)

PLEASE DO NOT SUBMIT CONFIDENTIAL INFORMATION OR INFORMATION SPECIFIC TO DRIVES
THAT ARE VENDOR SAMPLES OR NOT PUBLICLY AVAILABLE.

## Compilation and installation

### Requirements

*libzbd* requires the following packages for compilation:

* m4
* autoconf
* automake
* libtool
* m4
* GTK3 and GTK3 development headers (when building the *gzbd* and *gzbd-viewer*
  graphical applications)

Since *libzbd* uses Linux(tm) kernel zoned block device interface, compilation
must be done on a system where the kernel header file *blkzoned.h* for zoned
block devices interface is installed under /usr/include/linux/. This implies
that the kernel version must be higher than version 4.10.0.

If the GTK3 and GTK3 development packages are not installed, compilation of the
*gzbd* and *gzbd-viewer* graphical applications is automatically disabled.

### Compilation

To compile the library and all example applications under the tools directory,
execute the following commands.

```
$ sh ./autogen.sh
$ ./configure
$ make
```

### Installation

To install the library and all example applications compiled under the tools
directory, execute the following command.

```
$ sudo make install
```

The library files are by default installed under `/usr/lib` (or `/usr/lib64`).
The library header file is installed under `/usr/include/libzbd`.

The executable files for the example applications are installed under
`/usr/bin`.

These default installation paths can be changed. Executing the following
command displays the options used to control the installation paths.

```
$ ./configure --help
```

### Building RPM Packages

The *rpm* and *rpmbuild* utilities are necessary to build *libzbd* RPM
packages. Once these utilities are installed, the RPM packages can be built
using the following command.

```
$ sh ./autogen.sh
$ ./configure
$ make rpm
```

Five RPM packages are built:
* A binary package providing *libzbd* library file, the tools executables,
  the tools man pages, documentation and license files.
* A source RPM package
* A *debuginfo* RPM package and a *debugsource* RPM package
* A development package providing the *libzbd* header files

The source RPM package can be used to build the binary and debug RPM packages
outside of *libzbd* source tree using the following command.

```
$ rpmbuild --rebuild libzbd-<version>.src.rpm
```

## Library

*libzbd* defines a set of functions and data structures simplifying the
management and use of zoned block devices.

### Overview

*libzbd* functions and data structures are defined in the header file
[include/libzbd/zbd.h]. Applications using *libzbd* must include this header
file and compile against *libzbd* using dynamic linking (`libzbd.so` library
file) or statically linking using the `libzbd.a` archive file.

*libzbd* internal implementation is simple. No internal library state is
maintained at run-time, with the exception of a list of open zoned block device.
Data types and units used by regular file access system calls are reused.

* Open zoned block device files are identified using a file descriptor similar
  to one that the *open()* system call returns.
* Zone information such as zone start position, zone size and zone write pointer
  position use Byte unit, the same unit as used by system calls such as
  *fseek()*, *read()* or *write()* for file offset position and I/O buffer
  sizes.

*libzbd* provides data structures for describing zones of a zoned block device
(*struct zbd_zone*) and providing information about the device itself
(*struct zbd_info*).

### Library Functions

*libzbd* implements the following functions.

Function                | Description
----------------------- | -----------------------------------------------
*zbd_device_is_zoned()* | Test if a block device is a zoned block device
*zbd_open()*            | Open a zoned block device
*zbd_close()*           | Close a zoned block device
*zbd_get_info()*        | Get an open zoned block device information
*zbd_report_zones()*<br>*zbd_list_zones()* | Get zone information of an open device
*zbd_report_nr_zones()* | Get the number of zones of an open device
*zbd_zone_operation()*  | Execute a zone management operation
*zbd_reset_zones()*     | Reset the write pointer position of a range of zones
*zbd_open_zones()*      | Explicitly open a range of zones
*zbd_close_zones()*     | Explicitly close a range of open zones
*zbd_finish_zones()*    | Finish a range of zones

The following macro definitions are defined to facilitate manipulation of a
zone descriptor information (*struct zbd_zone*).

Function                | Description
----------------------- | --------------------------------------------------
*zbd_zone_type()*	| Get a zone type (*enum zbd_zone_type*)
*zbd_zone_cnv()*	| Test if a zone type is conventional
*zbd_zone_swr()*	| Test if a zone type is sequential write required
*zbd_zone_swp()*	| Test if a zone type is sequential write preferred
*zbd_zone_seq()*	| Test if zone type is sequential (not conventional)
*zbd_zone_cond()*	| Get a zone condition (*enum zbd_zone_cond*)
*zbd_zone_not_wp()*	| Test if a zone is "not write pointer" (conventional zones)
*zbd_zone_empty()*	| Test if a zone is empty
*zbd_zone_imp_open()*	| Test if a zone is implicitly open
*zbd_zone_exp_open()*	| Test if a zone is explicitly open
*zbd_zone_is_open()*	| Test if a zone is open (implicitly or explicitly)
*zbd_zone_closed()*	| Test if a zone is closed
*zbd_zone_full()*	| Test if a zone is full
*zbd_zone_rdonly()*	| Test if a zone is read-only
*zbd_zone_offline()*	| Test if a zone is offline
*zbd_zone_start()*	| Get a zone start position (bytes)
*zbd_zone_len()*	| Get a zone size (bytes)
*zbd_zone_capacity()*	| Get a zone capacity (bytes)
*zbd_zone_wp()*		| Get a zone write pointer position (bytes)
*zbd_zone_flags()*	| Get a zone state flags
*zbd_zone_rwp_recommended()* | Test if a zone indicates reset write pointer recommended
*zbd_zone_non_seq_resources()* | Test if a zone indicates using non-sequential write resources

The followimg utility functions are also defined.

Function                 | Description
------------------------ | --------------------------------------------------
*zbd_set_log_level()*    | Set the logging level of the library functions     
*zbd_device_model_str()* | Get a string describing a device zoned model
*zbd_zone_type_str()*    | Get a string describing a zone type
*zbd_zone_cond_str()*	 | Get a string describing a zone condition
                                                                                
### Thread Safety

Since *libzbd* does not maintain any internal state for open zoned block
devices, that is, it does not dynamically maintain the zone state of open zoned
block devices, no synchronization mechanism for multiple threads applications is
implemented. It is the responsibility of the application to ensure that zones of
a device are manipulated correctly with mutual exclusion when needed. This is
in particular necessary for operations like concurrent write to the same zone
by multiple threads or the execution of zone management operations while reading
or writing the target zones.

### Functions Documentation

*libzbd* functions and data types are documented in more details using code
comments in the file [include/libzbd/zbd.h].

## Tools

Under the tools directory, several simple applications are available as
examples. These appliations are as follows.

* **zbd** This application execute zone commands on a device.

* **gzbd** provides a graphical user interface showing zone information of a
  zoned block device. It also displays the write status (write pointer
  position) of zones graphically using color coding (red for written space and
  green for unwritten space). Operations on zones can also be executed directly
  from the interface (reset zone write pointer, open zone, close zone, etc).

* **gzbd-viewer** provides a simple graphical user interface showing the write
  pointer position and zone state of zones of a zoned block device. Similar
  color coding as *gzbd* is used. This application automatically refresh the
  device zone information after a configurable timeout elapses. The default
  refresh cycle is 2 times per seconds (500 ms).
