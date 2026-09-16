[BITS 32]

MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
VIDMODE  equ  1 << 2
FLAGS    equ  MBALIGN | MEMINFO | VIDMODE
MAGIC    equ  0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    ; 20 bytes of mandatory padding
    dd 0, 0, 0, 0, 0 
    ; Video mode request (1080p)
    dd 0                ; 0 = linear graphics mode
    dd 1920             ; Width
    dd 1080             ; Height
    dd 32               ; Bits per pixel

section .bss
align 16
stack_bottom:
    resb 16384          ; 16 KB stack
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    push ebx
    push eax
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang