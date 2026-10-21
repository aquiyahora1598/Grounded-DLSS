option casemap:none
EXTERN GroundedFocusStep:PROC
EXTERN focusTestCamera:QWORD
.const
align 16
pattern DWORD 040800000h,011223344h,055667788h,099aabbcch
quarter REAL4 0.25
.code
; Deliberately damage everything the inserted instruction must preserve.
GroundedFocusBlend PROC
 mov focusTestCamera,rdx
 mov rax,101
 mov rcx,102
 mov rdx,103
 mov r8,104
 mov r9,105
 mov r10,106
 mov r11,107
 xorps xmm1,xmm1
 xorps xmm2,xmm2
 xorps xmm3,xmm3
 xorps xmm4,xmm4
 xorps xmm5,xmm5
 mulss xmm0,quarter
 stc
 ret
GroundedFocusBlend ENDP
RunFocusAbi PROC FRAME
 push rdi
 .pushreg rdi
 sub rsp,30h
 .allocstack 30h
 .endprolog
 mov [rsp+20h],rcx
 mov rax,1
 mov rcx,2
 mov rdx,3
 mov r8,4
 mov r9,5
 mov r10,6
 mov r11,7
 mov rdi,8
 movaps xmm0,pattern
 movaps xmm1,pattern
 movaps xmm2,pattern
 movaps xmm3,pattern
 movaps xmm4,pattern
 movaps xmm5,pattern
 cmp rdi,rdi
 call GroundedFocusStep
 mov [rsp+28h],rdi
 mov rdi,[rsp+20h]
 mov [rdi],rax
 mov [rdi+8],rcx
 mov [rdi+16],rdx
 mov [rdi+24],r8
 mov [rdi+32],r9
 mov [rdi+40],r10
 mov [rdi+48],r11
 mov rax,[rsp+28h]
 mov [rdi+56],rax
 pushfq
 pop qword ptr [rdi+64]
 movups [rdi+72],xmm0
 movups [rdi+88],xmm1
 movups [rdi+104],xmm2
 movups [rdi+120],xmm3
 movups [rdi+136],xmm4
 movups [rdi+152],xmm5
 add rsp,30h
 pop rdi
 ret
RunFocusAbi ENDP
END
