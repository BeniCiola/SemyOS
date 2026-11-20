#!/bin/bash

set -e

cd "$(dirname "$0")/.."

echo "Building Semyorka..."

mkdir -p compifiles

# Bootloader
nasm -f bin boot/zagruzchik.asm -o compifiles/boot.bin

# Kernel: Assembly
mapfile -d '' asm_files < <(
	find . \
		-type d \( -name .git -o -name compifiles \) -prune -o \
		-type f -name '*.asm' \
		! -path './boot/zagruzchik.asm' \
		-print0 | sort -z
)

asm_objects=()

for file in "${asm_files[@]}"; do
	relative_path="${file#./}"
	object="compifiles/objects/${relative_path%.asm}.o"

	mkdir -p "$(dirname "$object")"

	echo "NASM: $file"
	nasm -f elf32 "$file" -o "$object"

	asm_objects+=("$object")
done

# Kernel: C
i686-elf-gcc -c kernel/yadro.c \
	-o compifiles/yadro.o \
	-ffreestanding -O2 -Wall -Wextra

# Linker
i686-elf-ld -T tools/klasterizatsiya.ld \
	"${asm_objects[@]}" \
	compifiles/yadro.o \
	-o compifiles/kernel.elf

i686-elf-objcopy -O binary \
	compifiles/kernel.elf compifiles/kernel.bin

# (Soviet) Union
cat compifiles/boot.bin compifiles/kernel.bin > semyos.img

kernel_size=$(stat -c %s compifiles/kernel.bin)

if (( kernel_size > 64 * 512 )); then
	echo "ERROR: kernel exceeds the 32 KiB bootloader limit."
	exit 1
fi

truncate -s $((20480 * 512)) semyos.img

echo "Done."

executer_pro_max() {
	read -p "Run Semyorka image? (Y/n): " execute

	if [[ -z "$execute" || "${execute,,}" == "y" ]]; then
		echo "Running Semyorka..."
		PULSE_SERVER=unix:/mnt/wslg/PulseServer qemu-system-i386 -m 256M \
        -drive file=semyos.img,format=raw,index=0,media=disk \
        -drive id=sata1,file=semyos.img,format=raw,if=none,file.locking=off \
        -device ahci,id=ahci \
        -device ide-hd,drive=sata1,bus=ahci.0 \
        -audiodev pa,id=snd0 \
        -device intel-hda \
        -device hda-output,audiodev=snd0

		exit 0
	elif [[ "${execute,,}" == "n" ]]; then
		echo "Operation complete."
		exit 0
	else
		echo "Write it again, better"
		executer_pro_max
	fi
}

executer_pro_max
