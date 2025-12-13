.section .text

.globl EnterNaked
.globl LeaveNaked
.globl TailcallNaked

EnterNaked:

    push %rax
    push %rcx
    push %rdx
    call EnterStub
    pop %rdx
    pop %rcx
    pop %rax
    ret

LeaveNaked:

    push %rax
    push %rcx
    push %rdx
    call LeaveStub
    pop %rdx
    pop %rcx
    pop %rax
    ret

TailcallNaked:

    push %rax
    push %rcx
    push %rdx
    call TailcallStub
    pop %rdx
    pop %rcx
    pop %rax
    ret
