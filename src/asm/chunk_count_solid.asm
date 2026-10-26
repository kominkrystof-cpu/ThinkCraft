default rel

section .text
global tc_chunk_count_solid
tc_chunk_count_solid:
    xor eax, eax
    test rsi, rsi
    jz .done
    xor ecx, ecx

.loop:
    cmp byte [rdi + rcx], 0
    je .next
    inc eax

.next:
    inc rcx
    cmp rcx, rsi
    jb .loop

.done:
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
