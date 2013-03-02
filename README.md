hCraft
======

![hCraft](https://raw.github.com/BizarreCake/hCraft/master/etc/45-small.png)

What is hCraft?
---------------

hCraft is a custom implementation of a Minecraft server, currently supprting the
51th revision of the protocol (version 1.4.7). hCraft strives to be fast,
customizable and easy to use.

Features
--------

The _currently_ implemented features are:
*  The client can connect to the server.
*  Movement and block modification are relayed between connected players.
*  Worlds can be loaded from/saved to an experimental world format (*HWv1*) -
   a compact single-file world container.
*  Players can easily switch between worlds using the /w command (Multiworld!).
*  A permissions-like rank system.
*  SQLite support.

*  Players can create and manipulate various types of world selections (spheres, cuboids, etc...),
   this includes filling them with blocks (large fills cause resending of chunks).
*  Custom physics (still very experimental)! The current implementation can handle
   around 10,000 falling sand blocks (with 4 physics threads).
   Custom block mechanics can be easily added.
*  Custom world generation - a very simplistic plains generator is set as default.
   Current world generators include "plains" and "flatgrass" (more will be added in the future).


Building
--------

To build hCraft, you will need a C++11-compatible compiler and a copy of
[SCons](http://www.scons.org/). Just change to the directory that contains
hCraft, and type `scons`. That will compile and link the source code into
an executable (can be found in the created "build" directory).

### Dependencies
*  [libevent](http://libevent.org/)
*  [sqlite3](http://www.sqlite.org/)
*  [zlib](http://www.zlib.net/)
*  [yaml-cpp](http://code.google.com/p/yaml-cpp/)
*  [libnoise](http://libnoise.sourceforge.net/)

IRC
---

hCraft's IRC channel can be found in `irc.divineirc.net/6667`, `#hCraft`.

Copyright
---------

hCraft is released under GNU's general public license (GPLv3), more information
can be found [here](http://www.gnu.org/licenses/gpl.html).

