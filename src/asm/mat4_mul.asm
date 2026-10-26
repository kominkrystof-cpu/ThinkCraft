default rel

section .text
global tc_mat4_mul
tc_mat4_mul:
    movups xmm0, [rsi]
    movss xmm1, [rdx]
    shufps xmm1, xmm1, 0
    mulps xmm0, xmm1
    movups xmm2, [rsi + 16]
    movss xmm1, [rdx + 4]
    shufps xmm1, xmm1, 0
    mulps xmm2, xmm1
    addps xmm0, xmm2
    movups xmm2, [rsi + 32]
    movss xmm1, [rdx + 8]
    shufps xmm1, xmm1, 0
    mulps xmm2, xmm1
    addps xmm0, xmm2
    movups xmm2, [rsi + 48]
    movss xmm1, [rdx + 12]
    shufps xmm1, xmm1, 0
    mulps xmm2, xmm1
    addps xmm0, xmm2
    movups [rdi], xmm0

    movups xmm0, [rsi]
    movss xmm1, [rdx + 16]
    shufps xmm1, xmm1, 0
    mulps xmm0, xmm1
    movups xmm2, [rsi + 16]
    movss xmm1, [rdx + 20]
    shufps xmm1, xmm1, 0
    mulps xmm2, xmm1
    addps xmm0, xmm2
    movups xmm2, [rsi + 32]
    movss xmm1, [rdx + 24]
    shufps xmm1, xmm1, 0
    mulps xmm2, xmm1
    addps xmm0, xmm2
    movups xmm2, [rsi + 48]
    movss xmm1, [rdx + 28]
    shufps xmm1, xmm1, 0
    mulps xmm2, xmm1
    addps xmm0, xmm2
    movups [rdi + 16], xmm0

    movups xmm0, [rsi]
    movss xmm1, [rdx + 32]
    shufps xmm1, xmm1, 0
    mulps xmm0, xmm1
    movups xmm2, [rsi + 16]
    movss xmm1, [rdx + 36]
    shufps xmm1, xmm1, 0
    mulps xmm2, xmm1
    addps xmm0, xmm2
    movups xmm2, [rsi + 32]
    movss xmm1, [rdx + 40]
    shufps xmm1, xmm1, 0
    mulps xmm2, xmm1
    addps xmm0, xmm2
    movups xmm2, [rsi + 48]
    movss xmm1, [rdx + 44]
    shufps xmm1, xmm1, 0
    mulps xmm2, xmm1
    addps xmm0, xmm2
    movups [rdi + 32], xmm0

    movups xmm0, [rsi]
    movss xmm1, [rdx + 48]
    shufps xmm1, xmm1, 0
    mulps xmm0, xmm1
    movups xmm2, [rsi + 16]
    movss xmm1, [rdx + 52]
    shufps xmm1, xmm1, 0
    mulps xmm2, xmm1
    addps xmm0, xmm2
    movups xmm2, [rsi + 32]
    movss xmm1, [rdx + 56]
    shufps xmm1, xmm1, 0
    mulps xmm2, xmm1
    addps xmm0, xmm2
    movups xmm2, [rsi + 48]
    movss xmm1, [rdx + 60]
    shufps xmm1, xmm1, 0
    mulps xmm2, xmm1
    addps xmm0, xmm2
    movups [rdi + 48], xmm0

    ret

section .note.GNU-stack noalloc noexec nowrite progbits
