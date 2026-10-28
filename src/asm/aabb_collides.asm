default rel

%define WORLD_SIZE_OFFSET 8
%define WORLD_SLOTS_OFFSET 32
%define CHUNK_BLOCKS_OFFSET 16
%define SLOT_BYTES 65572
%define SLOT_CHUNK_X 65556
%define SLOT_CHUNK_Z 65560
%define SLOT_LOADED 65564

section .text
global tc_asm_aabb_collides
tc_asm_aabb_collides:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 56

    test rdi, rdi
    jz .none

    mov rax, [rdi + WORLD_SLOTS_OFFSET]
    test rax, rax
    jz .none
    mov [rsp + 0], rax

    mov eax, [rdi + WORLD_SIZE_OFFSET]
    test eax, eax
    jle .none
    mov [rsp + 8], eax

    mov [rsp + 12], esi
    mov [rsp + 16], edx
    mov [rsp + 20], ecx
    mov [rsp + 24], r8d
    mov [rsp + 28], r9d
    mov eax, [rbp + 16]
    mov [rsp + 32], eax

    mov eax, [rsp + 24]
    test eax, eax
    js .none
    cmp dword [rsp + 20], 256
    jge .none

    mov eax, [rsp + 12]
    cmp eax, [rsp + 16]
    jg .none
    mov eax, [rsp + 28]
    cmp eax, [rsp + 32]
    jg .none

    cmp dword [rsp + 20], 0
    jge .y_min_ok
    mov dword [rsp + 20], 0
.y_min_ok:
    cmp dword [rsp + 24], 255
    jle .y_max_ok
    mov dword [rsp + 24], 255
.y_max_ok:

    mov eax, [rsp + 20]
    mov [rsp + 40], eax
.loop_y:
    mov eax, [rsp + 40]
    cmp eax, [rsp + 24]
    jg .none

    mov eax, [rsp + 28]
    mov [rsp + 44], eax
.loop_z:
    mov eax, [rsp + 44]
    cmp eax, [rsp + 32]
    jg .next_y

    mov eax, [rsp + 12]
    mov [rsp + 48], eax
.loop_x:
    mov eax, [rsp + 48]
    cmp eax, [rsp + 16]
    jg .next_z

    mov r12d, [rsp + 48]
    mov r13d, [rsp + 44]
    mov r14d, [rsp + 40]

    mov eax, r12d
    sar eax, 4
    mov ebx, eax

    mov eax, r13d
    sar eax, 4
    mov r15d, eax

    mov eax, ebx
    cdq
    idiv dword [rsp + 8]
    test edx, edx
    jns .chunk_x_ok
    add edx, [rsp + 8]
.chunk_x_ok:
    mov r8d, edx

    mov eax, r15d
    cdq
    idiv dword [rsp + 8]
    test edx, edx
    jns .chunk_z_ok
    add edx, [rsp + 8]
.chunk_z_ok:

    imul edx, [rsp + 8]
    add edx, r8d
    movsxd rdx, edx
    imul rdx, SLOT_BYTES
    mov r9, [rsp + 0]
    add r9, rdx

    cmp dword [r9 + SLOT_LOADED], 0
    je .miss
    cmp dword [r9 + SLOT_CHUNK_X], ebx
    jne .miss
    cmp dword [r9 + SLOT_CHUNK_Z], r15d
    jne .miss

    mov eax, r12d
    and eax, 15
    mov ecx, r13d
    and ecx, 15
    shl ecx, 4
    add ecx, eax

    mov edx, r14d
    shl edx, 8
    add edx, ecx

    movzx eax, byte [r9 + CHUNK_BLOCKS_OFFSET + rdx]
    test eax, eax
    jnz .hit

.miss:
    inc dword [rsp + 48]
    jmp .loop_x

.next_z:
    inc dword [rsp + 44]
    jmp .loop_z

.next_y:
    inc dword [rsp + 40]
    jmp .loop_y

.hit:
    mov eax, 1
    jmp .return

.none:
    xor eax, eax
.return:
    add rsp, 56
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
