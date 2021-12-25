.686

_TEXT segment

?findDWORD@NT@@YIPAKKPBKK@Z proc
	jecxz @@0
	xchg edi,edx
	mov eax,[esp + 4]
	repne scasd
	lea eax,[edi-4]
	cmovne eax,ecx
	mov edi,edx
	ret 4
@@0:
	mov eax,ecx
	ret 4
?findDWORD@NT@@YIPAKKPBKK@Z endp

_TEXT ends
end