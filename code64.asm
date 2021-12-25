_TEXT segment 'CODE'

?findDWORD@NT@@YAPEAK_KPEBKK@Z proc
	jrcxz @@0
	xchg rdi,rdx
	mov rax,r8
	repne scasd
	lea rax, [rdi-4]
	cmovne rax, rcx
	mov rdi,rdx
	ret
@@0:
	mov eax,ecx
	ret
?findDWORD@NT@@YAPEAK_KPEBKK@Z endp
_TEXT ENDS
END