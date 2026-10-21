; Preserve all register arguments, resolve outside DllMain, then tail-forward.
option casemap:none
extern ResolveGroundedDxgi:proc
.code
PROXY_STUB macro exportName, exportIndex
proxy_&exportName proc frame
 sub rsp, 0A8h
 .allocstack 0A8h
 .endprolog
 mov [rsp+20h],rcx
 mov [rsp+28h],rdx
 mov [rsp+30h],r8
 mov [rsp+38h],r9
 movdqu [rsp+40h],xmm0
 movdqu [rsp+50h],xmm1
 movdqu [rsp+60h],xmm2
 movdqu [rsp+70h],xmm3
 mov ecx,exportIndex
 call ResolveGroundedDxgi
 mov r11,rax
 mov rcx,[rsp+20h]
 mov rdx,[rsp+28h]
 mov r8,[rsp+30h]
 mov r9,[rsp+38h]
 movdqu xmm0,[rsp+40h]
 movdqu xmm1,[rsp+50h]
 movdqu xmm2,[rsp+60h]
 movdqu xmm3,[rsp+70h]
 add rsp,0A8h
 jmp r11
proxy_&exportName endp
endm
PROXY_STUB ApplyCompatResolutionQuirking, 0
PROXY_STUB CompatString, 1
PROXY_STUB CompatValue, 2
PROXY_STUB CreateDXGIFactory, 3
PROXY_STUB CreateDXGIFactory1, 4
PROXY_STUB CreateDXGIFactory2, 5
PROXY_STUB DXGID3D10CreateDevice, 6
PROXY_STUB DXGID3D10CreateLayeredDevice, 7
PROXY_STUB DXGID3D10GetLayeredDeviceSize, 8
PROXY_STUB DXGID3D10RegisterLayers, 9
PROXY_STUB DXGIDeclareAdapterRemovalSupport, 10
PROXY_STUB DXGIDisableVBlankVirtualization, 11
PROXY_STUB DXGIDumpJournal, 12
PROXY_STUB DXGIGetDebugInterface1, 13
PROXY_STUB DXGIReportAdapterConfiguration, 14
PROXY_STUB PIXBeginCapture, 15
PROXY_STUB PIXEndCapture, 16
PROXY_STUB PIXGetCaptureState, 17
PROXY_STUB UpdateHMDEmulationStatus, 19
end

