![Konfyt Logo](icons/konfytReadmeLogo.png)
Konfyt
======
Digital Keyboard Workstation for Linux
--------------------------------------

2014-2018 Gideon van der Kolf, noedigcode@gmail.com

Welkom by Konfyt.

Konfyt is a digital keyboard workstation for Linux which allows you to set up
patches, each with multiple layers, and instantly switch between these patches
for live keyboard playing. Patches may consist of multiple layers of soundfonts
(.sf2), SFZs, audio input ports and MIDI output ports. Konfyt features a library
which scans the filesystem for and allows quick access to soundfont programs and
SFZs.

While I have used Konfyt successfully in various "live" environments, and do try
to test it thoroughly, use it at your own risk. I am not responsible for any trauma
due to failures during live performances.

Konfyt is open source under the GPL license and makes use of Qt, JACK, Carla,
liblscp and Fluidsynth. You have the option (see command line arguments) of
loading SFZ files with Carla (which uses SFZero), or with Linuxsampler (which
is licensed under GPL with a commercial exception).

More information is available at www.noedig.co.za/konfyt/


Requirements:
-------------
Konfyt was developed and tested with the following:
* Linux Mint 18.3 (based on Ubuntu 16.04).
* Qt 5
* Fluidsynth 1.1.6 (Version 1.1.5 has a nasty polyphony bug)
* liblscp 0.5.6 and thus Linuxsampler
* Carla

Carla is available in the KXStudio repositories (http://kxstudio.linuxaudio.org/Repositories).
All versions are not guaranteed to work since Carla is under active development and the API is still subject to change.

Tested with version
```
1.9.9 (2.0-beta7)
(KXStudio Package version 2:1.9.9+git20180824v5)
```


Building:
---------
See the testing branch for the latest features. Although it's still technically
under testing, it should be quite stable.

The following are required to build:

* Qt5: Ensure that the Qt5 development packages are installed.
Check the Qt version that qmake uses with:
```
qmake -v
```

If qmake uses Qt4 by default, force it to use Qt5 with:
```
qmake --qt=5
```

* pkg-config

* Fluidsynth development files (libfluidsynth-dev in Ubuntu).

* Carla

* g++

* liblscp development files (liblscp-dev in Ubuntu)


The Konfyt .pro file can be opened with QtCreator and configured and built by following
the steps in the IDE.

To build from the command line, run the following:
```
qmake konfyt.pro
make
```

A "konfyt" executable will be produced.


