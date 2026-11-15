# Semyorka Operating System

## All notable changes will be found here / Todos los cambios relevantes serán encontrados aquí


## English

## Version 0.1.0

### Added
- Bootloader and protected mode initialization.
- 32-bit Kernel.
- Global Descriptor Table (GDT).
- Interrupt Descriptor Table (IDT).
- Programmable Interrupt Controller (PIC).
- Hardware drivers.
- Support for VBE graphics.
- Basic memory management system.
- Round-Robin multitasking system.
- Interactive shell with, finally, arguments support. Better than PotatOS.

### Subdirectories
- boot/: Contains the bootloader.
- kernel/: Contains the kernel.
- lib/: Contains all the necessary libraries.
- compifiles/: Contains all the files that are compiled.
- tools/: Contains the tools the system uses to build itself.
- programs/: Contains the included programs in kernel

## Version 0.1.1

### Added
- Avanced memory management system.
- Physical Memory Manager (PMM).
- Virtual Memory Manager (VMM).
- SATA AHCI and Mass Storage Device drivers.
- Keyboard PS/2 driver (compatibility with 20 years ago (I hope (really hope))).

## In process
- More functionalities for shell.
- Intel HDA driver.


## Spanish

## Versión 0.1.0

### Añadido
- Inicialización del Bootloader y Protected Mode.
- Kernel de 32 bits.
- Global Descriptor Table (GDT).
- Interrupt Descriptor Table (IDT).
- Programmable Interrupt Controller (PIC).
- Controladores de Hardware.
- Soporte para gráficos VBE.
- Sistema básico para gestión de memoria.
- Sistema de multitarea tipo Round-Robin.
- Shell interactiva con soporte para argumentos (¡Al fin!). Mucho mejor que PotatOS.

### Subdirectorios
- boot/: Contiene el bootloader.
- kernel/: Contiene el kernel.
- lib/: Contiene todas las librerías necesarias.
- compifiles/: Contiene todos los archivos compilados.
- tools/: Contiene las herramientas que el sistema usa para crearse.

## Verión 0.1.1

### Añadido
- Sistema gestor de memoria avanzado.
- Physical Memory Manager (PMM).
- Virutal Memory Manager (VMM).
- Controladores para SATA AHCI y Mass Storage Devices.
- Controlador para teclado PS/2 (Compatibilidad para hace 20 años atras (espero (realmente espero))).

### En proceso
- Más funcionalidades para la shell.
- Controlador para el Intel HDA.
