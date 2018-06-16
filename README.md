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

Konfyt is open source under the GPL license and makes use of Qt, JACK, Carla and
Fluidsynth. Note however that Linuxsampler is used to load SFZ files (via Carla)
which is licensed under GPL with a commercial exception.

More information is available at www.noedig.co.za/konfyt/


Requirements:
-------------
Konfyt was developed and tested with the following:
* Linux Mint 18.3 (based on Ubuntu 16.04).
* Qt4
* Fluidsynth 1.1.6 (Version 1.1.5 has a nasty polyphony bug)
* Carla

Carla is available in the KXStudio repositories (http://kxstudio.linuxaudio.org/Repositories).
All versions are not guaranteed to work since Carla is under active development and the API is still subject to change.

Tested with version
```
1.9.8 (2.0-beta6) (KXStudio Package version 2:1.9.8+git20180610.2v5)
```


Building:
---------
The following are required to build:

* Qt4: Ensure that the Qt4 development packages are installed.
Check the Qt version that qmake uses with:
```
qmake -v
```

Sometimes, if both Qt5 and Qt4 are installed, the Qt4 qmake command is:
```
qmake-qt4
```

* pkg-config

* Fluidsynth: Ensure the development files are installed (libfluidsynth-dev in Ubuntu).

* Carla

* g++-4.8


The Konfyt .pro file can be opened with QtCreator and configured and built by following
the steps in the IDE.

To build from the command line, run the following:
```
qmake konfyt.pro
make
```

A "konfyt" executable will be produced.


