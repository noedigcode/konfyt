![Konfyt Logo](icons/konfytReadmeLogo.png)
Konfyt
======
Digital Keyboard Workstation for Linux
--------------------------------------

2014-2023 Gideon van der Kolf, noedigcode@gmail.com

Welkom by Konfyt.

Konfyt is a digital keyboard workstation for Linux which allows you to set up
patches, each with multiple layers, and instantly switch between these patches
for live keyboard playing. Patches may consist of multiple layers of soundfonts
(.sf2), SFZs, audio input ports and MIDI output ports. Konfyt features a library
which scans the filesystem for and allows quick access to soundfont programs and
SFZs.

While I use Konfyt on a daily basis and I'm confident using it for a live performance,
do your own testing and use it at your own risk. I am not responsible for any trauma
due to failures during live performances.

Konfyt is open source under the GPL license and makes use of Qt, JACK, Carla,
liblscp and Fluidsynth. You have the option (see command line arguments) of
loading SFZ files with Carla (which uses SFZero), or with Linuxsampler (which
is licensed under GPL with a commercial exception).

More information is available at www.noedig.co.za/konfyt/


Requirements for running:
-------------------------

Konfyt is mainly developed on Linux Mint 21.1 (based on Ubuntu 22.04).

The following are required to run Konfyt:

* Qt 5.15.3
* Fluidsynth 2.2.5
* liblscp 0.6.0 and thus Linuxsampler
* Carla 2.5.4

The versions listed above are known to work, but different versions should work too.

Linuxsampler is available in the KXStudio repositories (http://kxstudio.linuxaudio.org/Repositories)
for Ubuntu based systems.

If you try to run a Konfyt binary on Ubuntu 21+ that was built on Ubuntu 20, you may need to create a symlink for `libfluidsynth.so.2` in `/usr/lib/x86_64-linux-gnu` pointing to `libfluidsynth.so.3...`.


Requirements for building:
--------------------------

See the testing branch for the latest features. Although it's still technically
under testing, it should be quite stable.

The following are required to build Konfyt:

* Qt5: Ensure that the Qt5 development packages are installed.

  On Ubuntu 20.04: `qt5-default`

  On Ubuntu 21+: `qtbase5-dev`

  Ensure that Qt5 is used to build. Check the Qt version that qmake uses with:
  ```
  qmake -v
  ```

  If qmake uses Qt4 by default, force it to use Qt5 with:
  ```
  qmake --qt=5
  ```
  
  On some distros, the qmake command is
  ```
  qmake-qt5
  ```

* `pkg-config`

* JACK development files

  Ubuntu: `libjack-dev` (JACK 1) or `libjack-jackd2-dev` (JACK 2)

  Fedora: `jack-audio-connection-kit-devel`

* Fluidsynth development files
  
  Ubuntu: `libfluidsynth-dev`

  **Note**: On Ubuntu 21 and 22, if you use JACK 2, ensure `libjack-jackd2-dev` is installed prior to or at the same time as installing `libfluidsynth-dev`. If `libjack-jackd2-dev` is not installed at the time of installing `libfluidsynth-dev`, the package manager may default to `libjack-dev` (JACK 1) and possibly trigger removal of JACK 2 and several other packages. **Read the apt messages!** If apt wants to remove JACK 2, try specifying both JACK and the Fluidsynth dev package in the same install command:
  ```
  sudo apt install libfluidsynth-dev libjack-jackd2-dev
  ```
  
  Fedora: `fluidsynth-devel`

* Carla

  Optional - to build without Carla support, either add the line `CONFIG+=KONFYT_NO_CARLA` to the `konfyt.pro` file (see the comments in the file for the correct location) or pass it as an option to qmake, as described below.
  
  For the lastest version, use the KXStudio repositories. Otherwise, the packages are also available in the standard Ubuntu repositories.

  Ubuntu 20.04: `carla`

  Ubuntu 21+: `carla-dev`
  
  Fedora: `Carla-devel`

* `g++`

* liblscp development files
  
  Ubuntu: `liblscp-dev`
  
  Fedora: `liblscp-devel`, available from the Planet CCRMA third-party repository


Building:
---------

To build from the command line, run the following from the source code directory:
```
mkdir build
cd build
qmake ../konfyt.pro
make
```

To build without Carla support, alter the qmake command as follows:
```
qmake "CONFIG+=KONFYT_NO_CARLA" ../konfyt.pro
```

A "konfyt" executable file will be produced.

