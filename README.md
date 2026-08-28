# MyOS

This is my personal OS project.

## Installing dependencies
`sudo apt install build-essential libc6-i386 mtools nasm xorriso qemu qemu-system-i386 bochs bochs-sdl bochsbios vgabios grub2`

Cross-Compiler: https://github.com/lordmilko/i686-elf-tools

At the time of writing, the bochsbios package has a bug in Ubuntu that causes bochs not to work. In that case, replace /usr/share/bochs/BIOS-bochs-latest with the one from https://github.com/ipxe/bochs/blob/master/bios/BIOS-bochs-latest

Don't forget to add the cross-compiler's bin folder to the PATH environment variable, as well as execute permission to all the cross-compiler binaries.

### WSL Configuration

If on Windows 10, an X server must be installed to use Qemu or Bochs.

Bochs may not play sound due to ALSA not being supported by WSLg. To fix it, follow this tutorial: https://github.com/microsoft/wslg/issues/864


To use Bochs's GUI, the following packages must be installed:\
`sudo apt install libx11-dev libxrandr-dev libxext-dev libxpm-dev libgtk-3-dev libsdl2-dev pkg-config libgtk2.0-dev flex bison docbook-dsssl`\
Afterwards, Bochs must be built from source with the following configuration flags:\
`./configure --enable-debugger --enable-debugger-gui`
