<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/banner.png">
  <source media="(prefers-color-scheme: light)" srcset="assets/banner2.png">
  <img alt="SemyOS Banner" src="assets/banner.png">
</picture>

# SemyOS - Semyorka Operating System

## We (now we are two) have for you:

### Type of system
- System architecture x86, 32-bits.
- Our own custom bootloader.
- An unforgettable kernel.
- A functional File System MIKFS (MIK File System).
- Multitasking service for all of your programs.
- A powerful shell that can interpret your commands.
- Multiple drivers, such as:
  - Keyboard PS/2 driver.
  - Intel HDA driver.
  - USB xHCI driver.
  - SATA AHCI and MSD drivers.

## How can you run it?
If you want to build SemyOS from source and run it right away, execute the builder script located in `tools/`:
```bash
chmod +x tools/fabrika88.sh
./tools/fabrika88.sh
```

*(Make sure you have the required dependencies installed: `nasm`, `gcc`, `qemu-system-i386`, etc.)*

If you just want to boot the included `semyos.img` directly with **QEMU** (requires SATA AHCI support):

Here you only need **QEMU** installed to run it:
```bash
qemu-system-i386 -m 256M \
  -drive file=semyos.img,format=raw,index=0,media=disk \
  -drive id=sata1,file=semyos.img,format=raw,if=none,file.locking=off \
  -device ahci,id=ahci -device ide-hd,drive=sata1,bus=ahci.0 \
  -audiodev sdl,id=snd0 -device intel-hda -device hda-output,audiodev=snd0
```

> **Note for WSL2 users:** Add `PULSE_SERVER=unix:/mnt/wslg/PulseServer` at the beginning and replace `sdl` with `pa` for audio support.

## If you are a bit lost with commands...
- Remember: Your starman is "help" command.

---
Check the [CHANGELOG.md](CHANGELOG.md) for version history. Licensed under [GPL-3.0](LICENSE).
