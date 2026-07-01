# MyOS

This is my personal OS project.

### Installing dependencies
```sudo apt install build-essential libc6-i386 mtools nasm xorriso qemu qemu-system-i386 bochs bochs-sdl bochsbios vgabios grub2```

Cross-Compiler: https://github.com/lordmilko/i686-elf-tools

At the time of writing, the bochsbios package has a bug in Ubuntu that causes bochs not to work. In that case replace /usr/share/bochs/BIOS-bochs-latest with the one from https://github.com/ipxe/bochs/blob/master/bios/BIOS-bochs-latest

If using WSL, bochs may not play sound due to ALSA not being supported by WSLg. To fix it, follow this tutorial: https://github.com/microsoft/wslg/issues/864

Don't forget to add the cross-compiler's bin folder to the PATH environment variable, as well as add execute permission to all the files in the cross-compiler folder and the shell scripts and Makefiles in the project.
