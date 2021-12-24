#pragma once

class CModule : LIST_ENTRY
{
	PVOID _ImageBase;
	ULONG _size;
	ULONG _nSymbols;
	char _name[32];
	ULARGE_INTEGER _offsets[/*_nSymbols { rva, ofs }*/];
	//CHAR Names[];
	void Init(PCSTR name, PVOID ImageBase, ULONG size)
	{
		_size = size, _ImageBase = ImageBase;
		strcpy_s(_name, _countof(_name), name);
		DbgPrint("++CModule<%p>(%s) %p\n", this, name, ImageBase);
	}

	static LIST_ENTRY s_head;

	CModule()
	{
		InsertHeadList(&s_head, this);
	}

	~CModule()
	{
		DbgPrint("--CModule<%p>(%s) %p\n", this, _name, _ImageBase);
	}

	PCSTR GetNameFromRva(ULONG rva, PULONG pdisp, PCSTR* ppname);
public:

	void* operator new(size_t s, ULONG nSymbols, ULONG cbNames)
	{
		if (PVOID pv = ExAllocatePool(PagedPool, s + nSymbols * sizeof(ULARGE_INTEGER) + cbNames))
		{
			reinterpret_cast<CModule*>(pv)->_nSymbols = nSymbols;
			return pv;
		}
		return 0;
	}

	void operator delete(void* pv)
	{
		ExFreePool(pv);
	}

	static NTSTATUS Create(PCSTR name, PVOID ImageBase, ULONG size);
	static PCSTR GetNameFromVa(PVOID pv, PULONG pdisp, PCSTR* ppname);

	static void Cleanup();
};

void LoadNtModule(ULONG n, const ULONG ph[]);
void DumpStack(PCSTR txt);
ULONG HashString(PCSTR lpsz, ULONG hash = 0);