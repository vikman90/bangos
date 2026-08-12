[BITS 64]
global syscall_entry
global syscall_init_msrs
extern do_syscall

section .text

; void syscall_init_msrs(void)
syscall_init_msrs:
    push rbp
    mov rbp, rsp

    ; Enable SCE (System Call Enable) bit in IA32_EFER MSR (0xC0000080)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1
    wrmsr

    ; MSR_STAR (0xC0000081):
    mov ecx, 0xC0000081
    mov edx, 0x00100008
    mov eax, 0x00000000
    wrmsr

    ; MSR_LSTAR (0xC0000082): Target RIP for syscall instruction
    mov ecx, 0xC0000082
    lea rax, [rel syscall_entry]
    mov rdx, rax
    shr rdx, 32
    wrmsr

    ; MSR_SFMASK (0xC0000084): RFLAGS mask (mask out IF = 0x200)
    mov ecx, 0xC0000084
    mov edx, 0
    mov eax, 0x200
    wrmsr

    pop rbp
    ret

align 16
syscall_entry:
    ; Restore Kernel Data Segments (0x10) FIRST before accessing any memory!
    push rax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    pop rax

    mov [rel user_rsp_temp], rsp
    mov rsp, [rel kernel_rsp_temp]

    push qword [rel user_rsp_temp] ; User RSP
    push r11                       ; User RFLAGS
    push rcx                       ; User RIP
    push r15
    push r14
    push r13
    push r12
    push rbp
    push rbx
    push r9
    push r8
    push r10
    push rdx
    push rsi
    push rdi

    mov r9, r8    ; Arg 5
    mov r8, r10   ; Arg 4
    mov rcx, rdx  ; Arg 3
    mov rdx, rsi  ; Arg 2
    mov rsi, rdi  ; Arg 1
    mov rdi, rax  ; Syscall Num

    call do_syscall

    pop rdi
    pop rsi
    pop rdx
    pop r10
    pop r8
    pop r9
    pop rbx
    pop rbp
    pop r12
    pop r13
    pop r14
    pop r15
    pop rcx
    pop r11
    pop rsp

    ; Set User Data Segments (0x1B) before sysret
    push rax
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    pop rax

    db 0x48, 0x0f, 0x07 ; sysretq

section .bss
align 16
global kernel_rsp_temp
global user_rsp_temp
kernel_rsp_temp: resq 1
user_rsp_temp:   resq 1

section .note.GNU-stack noalloc noexec nowrite progbits
