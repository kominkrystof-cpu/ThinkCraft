default rel

section .text
global tc_batch_bake_vertices
tc_batch_bake_vertices:
    test rdx, rdx
    jz .done

.loop:
    mov ax, [rsi]
    add ax, cx
    mov [rdi], ax

    mov ax, [rsi + 2]
    mov [rdi + 2], ax

    mov ax, [rsi + 4]
    add ax, r8w
    mov [rdi + 4], ax

    mov eax, [rsi + 6]
    mov [rdi + 6], eax

    mov ax, [rsi + 8]
    mov [rdi + 8], ax

    add rsi, 10
    add rdi, 10
    dec rdx
    jnz .loop

.done:
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
