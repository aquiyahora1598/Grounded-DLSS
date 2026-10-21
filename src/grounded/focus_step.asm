option casemap:none
EXTERN GroundedFocusBlend:PROC
.code
; Called in place of one scalar MULSS. Preserve flags, all volatile GPRs,
; XMM1-5 and the upper lanes of XMM0. The native caller supplies camera in RDI.
GroundedFocusStep PROC FRAME
 lea rsp,[rsp-0c8h]
 .allocstack 0c8h
 .endprolog
 movaps [rsp+20h],xmm0
 movaps [rsp+30h],xmm1
 movaps [rsp+40h],xmm2
 movaps [rsp+50h],xmm3
 movaps [rsp+60h],xmm4
 movaps [rsp+70h],xmm5
 mov [rsp+80h],rax
 mov [rsp+88h],rcx
 mov [rsp+90h],rdx
 mov [rsp+98h],r8
 mov [rsp+0a0h],r9
 mov [rsp+0a8h],r10
 mov [rsp+0b0h],r11
 pushfq
 pop qword ptr [rsp+0b8h]
 mov rdx,rdi
 call GroundedFocusBlend
 movss dword ptr [rsp+20h],xmm0
 movaps xmm0,[rsp+20h]
 movaps xmm1,[rsp+30h]
 movaps xmm2,[rsp+40h]
 movaps xmm3,[rsp+50h]
 movaps xmm4,[rsp+60h]
 movaps xmm5,[rsp+70h]
 mov rax,[rsp+80h]
 mov rcx,[rsp+88h]
 mov rdx,[rsp+90h]
 mov r8,[rsp+98h]
 mov r9,[rsp+0a0h]
 mov r10,[rsp+0a8h]
 mov r11,[rsp+0b0h]
 push qword ptr [rsp+0b8h]
 popfq
 lea rsp,[rsp+0c8h]
 ret
GroundedFocusStep ENDP
END
