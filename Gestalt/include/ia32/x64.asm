.code _text

__reades proc
	mov rax, es
	ret
__reades endp

__readcs proc
	mov rax, cs
	ret
__readcs endp

__readss proc
	mov rax, ss
	ret
__readss endp

__readfs proc
	mov rax, fs
__readfs endp

__readds proc
	mov rax, ds
	ret
__readds endp

__readgs proc
	mov rax, gs
	ret
__readgs endp

__readldtr proc
	sldt rax
	ret
__readldtr endp

__readtr proc
	str rax
	ret
__readtr endp


__get_rip proc
	mov rax, [rsp]
	ret
__get_rip endp

__get_rsp proc
	mov rax, rsp
	add rax, 8
	ret
__get_rsp endp


__segment_access_rights proc
	lar rax, rcx
	jz success
	xor rax, rax
success:
	ret
__segment_access_rights endp

end