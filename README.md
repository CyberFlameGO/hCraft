hCraft
======

![hCraft](https://raw.github.com/BizarreCake/hCraft/master/etc/banner.png)

Public Test Server
------------------

A public test server is now live (devbox.hCraft.org:25565)!

What is hCraft?
---------------

hCraft is a custom implementation of a Minecraft server, currently supprting the
4th revision of the new protocol (version 1.7.2). hCraft strives to be fast,
customizable and easy to use.

Features
--------

The _currently_ implemented features are:
*  IRC support!
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
   that supports 10 biomes, other generators include include "plains", "flatgrass",
   "flatplains" and "overhang" (amazing overhangs!) (more will be added in the future).
*  Selections: Players can "select" areas using the implemented selections
   commands (/select), and then subsequently fill them with any block (or manipulate
   them in some way).
*  Block tracking, players may use /whodid to check who modified certain blocks.
*  An /undo command that lets the user undo block changes made by any player!
*  Portals! (World to world portals are supported)
*  World ownership\membership - Owners/members may be configured for every world.
     

Building
--------

<<<<<<< HEAD
To build hCraft, you will need a C++11-compatible compiler and a copy ofCMake. 
Just change to the directory that contains
hCraft, and type `cmake CMakeLists.txt`. That will generate a makefile for your
platform, then execute make -jX where X is the number of processors. this will 
compile and link the executable in Debug mode. An executable 
(can be found in the created "build" directory).

### NEW BUILDING NOTE
hCraft now uses CMake to automaticly configure all the paths and dependencies it is
no longer required to edit the build files, just follow the above directions.
=======
To build hCraft, you will need a C++11-compatible compiler and a working copy
of [CMake](http://www.cmake.org/). Change to the directory that contains
hCraft, and invoke (on Linux) `cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release`
to generate a makefile, and then proceed by typing `make`. After compilation has
ended, the executable could then be found inside the newly created `build`
directory.

>>>>>>> upstream/master

### Dependencies
*  [PThreads]
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

hCraft's IRC channel can be found in `irc.panicirc.net/6667`, `#hCraft`.

Copyright
---------

hCraft is released under GNU's general public license (GPLv3), more information
can be found [here](http://www.gnu.org/licenses/gpl.html).

![hCraft](https://raw.github.com/BizarreCake/hCraft/master/etc/45-small.png)

