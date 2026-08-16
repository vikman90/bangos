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
    ; User CS/SS: 0x20/0x18 (bits 63:48 = 0x0010, sysret loads CS=0x23, SS=0x1B)
    ; Kernel CS/SS: 0x08/0x10 (bits 47:32 = 0x0008)
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
    ; Restore Kernel Data Segments (0x10)
    push rax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    pop rax

    mov [rel user_rsp_temp], rsp
    mov rsp, [rel kernel_rsp_temp]

    ; Push standard context_frame_t matching interrupt frame
    push qword 0x1B                ; SS (User DS)
    push qword [rel user_rsp_temp] ; User RSP
    push r11                       ; User RFLAGS
    push qword 0x23                ; CS (User CS)
    push rcx                       ; User RIP
    push qword 0                   ; Error Code (dummy)
    push qword 0x80                ; Vector 0x80 (syscall)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov r9, rsp   ; frame pointer (context_frame_t *) as 6th argument
    mov r8, r10   ; Arg 4
    mov rcx, rdx  ; Arg 3
    mov rdx, rsi  ; Arg 2
    mov rsi, rdi  ; Arg 1
    mov rdi, rax  ; Syscall Num
    call do_syscall wrt ..plt

    ; Restore registers (preserving rax return value from do_syscall)
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    add rsp, 8    ; Skip saved rax (preserve rax)
    add rsp, 16   ; Skip vec_num and error_code
    iretq         ; iretq pops RIP, CS (0x23), RFLAGS, User RSP, User SS (0x1B)

section .bss
align 16
global kernel_rsp_temp
global user_rsp_temp
kernel_rsp_temp: resq 1
user_rsp_temp:   resq 1

section .note.GNU-stack noalloc noexec nowrite progbits
