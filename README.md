Important
---------

Be sure to read the new "Building" section, as a lot of things have changed in
recent versions.

hCraft
======

![hCraft](https://raw.github.com/BizarreCake/hCraft/master/etc/45-small.png)

What is hCraft?
---------------

hCraft is a custom implementation of a Minecraft server, currently supprting the
74th revision of the protocol (version 1.6.2). hCraft strives to be fast,
customizable and easy to use.

Features
--------

The _currently_ implemented features are:
*  MySQL* integration (used to be SQLite).
*  Authentication and encryption are supported.
*  Worlds can be loaded from/saved to an experimental world format (*HWv1*) -
   a compact single-file world container.
*  Players can easily switch between worlds using the /w command (Multiworld!).
*  A permissions-like rank system.
*  Custom physics (still very experimental)! The current implementation can handle
   around 50,000 falling sand blocks (with 4 physics threads).
   Custom block mechanics can be easily added.
*  Custom world generation - currently featuring an experimental world generator
   that supports 10 biomee, other generators include include "plains", "flatgrass",
   "flatplains" and "overhang" (amazing overhangs!) (more will be added in the future).
*  Selections: Players can "select" areas using the implemented selections
   commands (/select), and then subsequently fill them with any block (or manipulate
   them in some way).
*  Block tracking, players may do /whodid to check who modified certain blocks.
     

Building
--------

To build hCraft, you will need a C++11-compatible compiler and a copy of
[SCons](http://www.scons.org/). Just change to the directory that contains
hCraft, and type `scons`. That will compile and link the source code into
an executable (can be found in the created "build" directory).

### NEW BUILDING NOTE
hCraft currently uses a *very* simplistic SCons build files (>.>), so if you
wish to build hCraft, you *will* have to modify the build files to match
your computer! (SConstruct and src/SConscript).

Or more specifically, hCraft currently uses pre-set include\lib paths for
various libraries - such as MySQL and SOCI, so you will have to change those,
so the compiler would know where to find the libraries on your computer.

### Dependencies
*  [libevent](http://libevent.org/)
*  [MySQL](http://www.mysql.com/)
*  [SOCI](http://www.soci.sourceforge.net/)
*  [Crypto++](http://www.cryptopp.com/)
*  [zlib](http://www.zlib.net/)
*  [libnoise](http://libnoise.sourceforge.net/)
*  [tbb](http://threadingbuildingblocks.org/)
*  [curl](http://curl.haxx.se/)

IRC
---

hCraft's IRC channel can be found in `irc.divineirc.net/6667`, `#hCraft`.

Copyright
---------

hCraft is released under GNU's general public license (GPLv3), more information
can be found [here](http://www.gnu.org/licenses/gpl.html).

