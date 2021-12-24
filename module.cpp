#include "stdafx.h"

_NT_BEGIN

#include "module.h"
#include "pdb_util.h"

LIST_ENTRY CModule::s_head = { &s_head, &s_head };

PCSTR CModule::GetNameFromVa(PVOID pv, PULONG pdisp, PCSTR* ppname)
{
	PLIST_ENTRY entry = &s_head;
	
	while ((entry = entry->Flink) != &s_head)
	{
		CModule* p = static_cast<CModule*>(entry);

		ULONG_PTR rva = (ULONG_PTR)pv - (ULONG_PTR)p->_ImageBase;
		
		if (rva < p->_size)
		{
			return p->GetNameFromRva((ULONG)rva, pdisp, ppname);
		}
	}

	return 0;
}

PCSTR CModule::GetNameFromRva(ULONG rva, PULONG pdisp, PCSTR* ppname)
{
	* ppname = _name;
	PULARGE_INTEGER offsets = _offsets;
	ULONG a = 0, b = _nSymbols, o, ofs;

	do 
	{
		if (rva == (ofs = offsets[o = (a + b) >> 1].LowPart))
		{
			*pdisp = 0;
			return RtlOffsetToPointer(this, offsets[o].HighPart);
		}

		rva < ofs ? b = o : a = o + 1;

	} while (a < b);

	if (rva < ofs)
	{
		if (!o)
		{
			return 0;
		}
		ofs = offsets[--o].LowPart;
	}

	*pdisp = (ULONG)rva - ofs;

	return RtlOffsetToPointer(this, offsets[o].HighPart);
}

NTSTATUS CModule::Create(PCSTR name, PVOID ImageBase, ULONG size)
{
	if (ImageBase == &__ImageBase) return 0;

	struct Z : SymStore 
	{
		CModule* _pModule = 0;
		PSTR _psz = 0;
		PULARGE_INTEGER _pSymbol = 0;
		ULONG _nSymbols = 0;
		ULONG _cbNames = 0;

		static int __cdecl cmp(const void* pa, const void* pb)
		{
			ULONG a = ((ULARGE_INTEGER*)pa)->LowPart, b = ((ULARGE_INTEGER*)pb)->LowPart;
			if (a < b) return -1;
			if (a > b) return +1;
			return 0;
		}

		virtual BOOL EnumSymbolsEnd()
		{
			CModule * pDll = _pModule;
			ULONG nSymbols = _nSymbols;

			if (!pDll)
			{
				if (nSymbols)
				{
					if (pDll = new(nSymbols, _cbNames) CModule)
					{
						_pModule = pDll;
						_pSymbol = pDll->_offsets;
						_psz = (PSTR)&pDll->_offsets[nSymbols];

						return FALSE;
					}
				}

				return TRUE;
			}

			qsort(pDll->_offsets, nSymbols, sizeof(ULARGE_INTEGER), cmp);

			return TRUE;
		}

		virtual void Symbol(ULONG rva, PCSTR name)
		{
			if ((name[0] == '?' && name[1] == '?') ||
				(name[0] == 'W' && name[1] == 'P' && name[2] == 'P' && name[3] == '_') ||
				(name[0] == '_' && name[1] == '_' && name[2] == 'i' && name[3] == 'm' && name[4] == 'p' && name[5] == '_') ||
				(name[0] == '_' && name[1] == '_' && name[2] == 'h' && name[3] == 'm' && name[4] == 'o' && name[5] == 'd') ||
				(name[0] == '_' && name[1] == '_' && name[2] == 'I' && name[3] == 'M' && name[4] == 'P' && name[5] == 'O') ||
				(name[0] == '_' && name[1] == '_' && name[2] == 'D' && name[3] == 'E' && name[4] == 'L' && name[5] == 'A')
				)
			{
				return ;
			}

			ULONG len = (ULONG)strlen(name) + 1;

			CModule * pDll = _pModule;

			if (!pDll)
			{
				_nSymbols++;
				_cbNames += len;
				return ;
			}

			_pSymbol->HighPart = RtlPointerToOffset(pDll, memcpy(_psz, name, len));
			_pSymbol++->LowPart = rva;
			_psz += len;
		}
	} ss;

	NTSTATUS status = ss.GetSymbols((HMODULE)ImageBase, L"\\systemroot\\symbols");

	if (0 <= status)
	{
		if (CModule* pModule = ss._pModule)
		{
			pModule->Init(name, ImageBase, size);
			return STATUS_SUCCESS;
		}

		return STATUS_UNSUCCESSFUL;
	}

	return status;
}

void CModule::Cleanup()
{
	PLIST_ENTRY entry = s_head.Flink;

	while (entry != &s_head)
	{
		CModule* p = static_cast<CModule*>(entry);
		
		entry = entry->Flink;

		delete p;
	}
}

_NT_END

