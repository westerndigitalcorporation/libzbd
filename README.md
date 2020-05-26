SPDX-License-Identifier: LGPL-3.0-or-later

SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.

# libzbd

*libzbd* is a library providing functions to simplify the execution of zone
management operations on zoned block devices.

An example command line application using *libzbd* is available under the tools
directory. Additionally, graphical user interface applications are also
implemented to visually represent the state and usage of zones of a zoned block
device.

### Library version

*libzbd* current version is 0.0.2 (prototype state).

### Kernel Versions Supported

Any kernel providing zoned block device support, starting with kernel version
4.10.

### License

*libzbd* is distributed under the terms of the of the GNU Lesser General Public
License Version 3 or later (LGPL-3.0-or-later). A copy of this license can be
found in the file [LICENSES/LGPL-3.0-or-later.txt](LICENSES/LGPL-3.0-or-later.txt).

All example applications under the tools directory are distributed under
the terms of the GNU General Public License version 3, or any later version.
A copy of this license can be found in the file
[LICENSES/GPL-3.0-or-later.txt](LICENSES/GPL-3.0-or-later.txt).

*libzbd* and all its example applications are distributed "as is," without
technical support, and WITHOUT ANY WARRANTY, without even the implied warranty
of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. Along with *libzbd*, you
should have received a copy of the GNU Lesser General Public License version 3.
If not, please see https://www.gnu.org/licenses/lgpl-3.0.html.

### Contributions and Bug Reports

Contributions are accepted as github pull requests. Any problem may also be
reported through github issue page or by contacting:

* Damien Le Moal (damien.lemoal@wdc.com)
* Niklas Cassel (niklas.cassel@wdc.com)

PLEASE DO NOT SUBMIT CONFIDENTIAL INFORMATION OR INFORMATION SPECIFIC TO DRIVES
THAT ARE VENDOR SAMPLES OR NOT PUBLICLY AVAILABLE.

## Compilation and installation

*libzbd* requires the following packages for compilation:

* autoconf
* autoconf-archive
* automake
* libtool

The GTK3 and GTK3 development packages must be installed to automatically enable
compiling the *gzbd* and *gzbd-viewer* applications.

To compile the library and all example applications under the tools directory,
execute the following commands.

```
$ sh ./autogen.sh
$ ./configure
$ make
```

To install the library and all example applications compiled under the tools
directory, execute the following command.

```
$ sudo make install
```

The library file is by default installed under `/usr/lib` (or `/usr/lib64`). The
library header file is installed in `/usr/include/libzbd`. The executable files
for the example applications are installed under `/usr/bin`.

These default installation pathes can be changed. Executing the following
command displays the options used to control the installation paths.

```
$ ./configure --help
```

## Building rpm packages

The following command will build redistributable rpm packages.

```
$ make rpm
```

Three rpm packages are built: a binary package providing the library and
executable tools, a development package providing *libzbd* header files and a
source rpm package. The source rpm package can be used to build the binary and
development rpm packages outside of *libzbd* source tree using the following
command.

```
$ rpmbuild --rebuild libzbd-<version>.src.rpm
```

## Library Overview


TO DO

### Library Functions

TO DO

### Functions Documentation

More detailed information on *libzbd* functions and data types is available
through the comments in the file `include/libzbd/zbd.h`.

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
