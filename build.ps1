$ErrorActionPreference = "Stop"

$INCLUDE_FLAGS = "-Iarch -Idrivers -Ikernel -Ilib"
$CLANG_FLAGS = "-target i386-unknown-none-elf -ffreestanding -mno-sse -mno-mmx -O2 -Wall -Wextra $INCLUDE_FLAGS"

Write-Host "Assembling boot.asm..."
nasm -f elf32 arch/boot.asm -o arch/boot.o

Write-Host "Assembling interrupts.asm..."
nasm -f elf32 arch/interrupts.asm -o arch/interrupts.o

Write-Host "Compiling lib..."
clang $CLANG_FLAGS.Split() -c lib/string.c -o lib/string.o

Write-Host "Compiling arch..."
clang $CLANG_FLAGS.Split() -c arch/idt.c -o arch/idt.o
clang $CLANG_FLAGS.Split() -c arch/isr.c -o arch/isr.o
clang $CLANG_FLAGS.Split() -c arch/pic.c -o arch/pic.o
clang $CLANG_FLAGS.Split() -c arch/irq.c -o arch/irq.o

Write-Host "Compiling drivers..."
clang $CLANG_FLAGS.Split() -c drivers/fb.c -o drivers/fb.o
clang $CLANG_FLAGS.Split() -c drivers/vga.c -o drivers/vga.o
clang $CLANG_FLAGS.Split() -c drivers/timer.c -o drivers/timer.o
clang $CLANG_FLAGS.Split() -c drivers/keyboard.c -o drivers/keyboard.o
clang $CLANG_FLAGS.Split() -c drivers/mouse.c -o drivers/mouse.o
clang $CLANG_FLAGS.Split() -c drivers/ui.c -o drivers/ui.o

Write-Host "Compiling kernel..."
clang $CLANG_FLAGS.Split() -c kernel/kernel.c -o kernel/kernel.o

Write-Host "Linking kernel..."
clang -target i386-unknown-none-elf -fuse-ld=lld -nostdlib "-Wl,-T,linker.ld" -o kernel.bin `
    arch/boot.o arch/interrupts.o arch/idt.o arch/isr.o arch/pic.o arch/irq.o `
    drivers/fb.o drivers/vga.o drivers/timer.o drivers/keyboard.o drivers/mouse.o drivers/ui.o `
    lib/string.o kernel/kernel.o

Write-Host "Launching QEMU..."
qemu-system-i386 -M pc -vga std -kernel kernel.bin