#include "STDAFX.H"

_NT_BEGIN

#include "PDB.h"

class PdbReader : public DbiHeader
{
	PVOID _BaseAddress, _PdbBase;
	PLONG _StreamSizes;
	PULONG _StreamPages;
	ULONG _pageSize, _pagesUsed, _nStreams;

	PULONG GetStreamPages(ULONG iStream)
	{
		if (iStream < _nStreams)
		{
			ULONG pageSize = _pageSize;
			PLONG StreamSizes = _StreamSizes;
			PULONG StreamPages = _StreamPages;

			do 
			{
				if (ULONG StreamSize = *StreamSizes++)
				{
					StreamPages += (StreamSize + pageSize - 1) / pageSize;
				}
			} while (--iStream);

			return StreamPages;
		}

		return 0;
	}

	BOOL Read(ULONG StreamPages[], ULONG ofs, PBYTE pb, ULONG cb)
	{
		PVOID PdbBase = _PdbBase;
		ULONG cbCopy, pagesUsed = _pagesUsed, Page, PageSize = _pageSize;

		StreamPages += ofs / PageSize;

		if (ofs %= PageSize)
		{
			if ((Page = *StreamPages++) >= pagesUsed) 
			{
				return FALSE;
			}

			memcpy(pb, RtlOffsetToPointer(PdbBase, Page * PageSize) + ofs, cbCopy = min(PageSize - ofs, cb));

			pb += cbCopy, cb -= cbCopy;
		}

		if (cb)
		{
			do 
			{
				if ((Page = *StreamPages++) >= pagesUsed) 
				{
					return FALSE;
				}

				memcpy(pb, RtlOffsetToPointer(PdbBase, Page * PageSize), cbCopy = min(PageSize, cb));

			} while (pb += cbCopy, cb -= cbCopy);
		}

		return TRUE;
	}

	BOOL ValidatePdbHeader(PGUID signature = 0)
	{
		PdbHeader psh;
		return _StreamSizes[PDB_STREAM_PDB] > sizeof(PdbHeader) && 
			Read(PDB_STREAM_PDB, 0, &psh, sizeof(psh)) && 
			psh.impv == PDBImpv && 
			(!signature || psh.signature == *signature);
	}

public:

	BOOL Read(DWORD iStream, DWORD ofs, PVOID pb, DWORD cb)
	{
		PULONG StreamPages = GetStreamPages(iStream);
		return StreamPages ? Read(StreamPages, ofs, (PBYTE)pb, cb) : FALSE;
	}

	int Init(PdbFileHeader* header, SIZE_T ViewSize, PGUID signature, ULONG Age)
	{
		ULONG pageSize = header->pageSize, pagesUsed = header->pagesUsed;

		if (ViewSize < pageSize * pagesUsed)
		{
			return __LINE__;
		}

		ULONG directoryRoot = header->directoryRoot, directorySize = header->directorySize;

		if (directorySize < sizeof(ULONG))
		{
			return __LINE__;
		}

		ULONG directoryEnd = ((((directorySize + pageSize - 1) / pageSize) * sizeof(ULONG) + pageSize - 1) / pageSize) + directoryRoot;

		if (directoryEnd <= directoryRoot || directoryEnd > pagesUsed)
		{
			return __LINE__;
		}

		PULONG pd = (PULONG)ExAllocatePool(PagedPool, directorySize);
		if (!pd)
		{
			return __LINE__;
		}

		_BaseAddress = pd, _PdbBase = header, _pageSize = pageSize, _pagesUsed = pagesUsed;

		if (!Read((PULONG)RtlOffsetToPointer(header, directoryRoot * pageSize), 0, (PBYTE)pd, directorySize))
		{
			return __LINE__;
		}

		ULONG nStreams = *pd++;;
		_nStreams = nStreams;
		_StreamSizes = (PLONG)pd, _StreamPages = pd + _nStreams;

		if (nStreams <= PDB_STREAM_DBI || directorySize < (1 + nStreams) * sizeof(ULONG))
		{
			return __LINE__;
		}

		ULONG i = nStreams, k = 0;
		do 
		{
			k += (*pd++ + pageSize - 1) / pageSize;
		} while (--i);

		ULONG dbiSize = _StreamSizes[PDB_STREAM_DBI];

		if (
			directorySize < (1 + nStreams + k) * sizeof(ULONG)
			||
			!ValidatePdbHeader(signature)
			||
			dbiSize < sizeof(DbiHeader) 
			|| 
			!Read(PDB_STREAM_DBI, 0, static_cast<DbiHeader*>(this), sizeof(DbiHeader))
			||
			ver != DBIImpv
			||
			(signature && Age != age)
			)
		{
			return __LINE__;
		}

		return 0;
	}

	DWORD getStreamSize(ULONG iStream)
	{
		return iStream < _nStreams ? _StreamSizes[iStream] : 0;
	}

	BOOL ReadDbgHeader(DbiDbgHdr* dbgHdr)
	{
		// [Module Info)[Section Contribution)[Section Map)[File Info)[TSM)[EC)[DbiDbgHdr)
		ULONG Size = dbghdrSize;
		LONG dbghdrOfs = sizeof(DbiHeader) + gpmodiSize + secconSize + secmapSize + filinfSize + tsmapSize + ecinfoSize;
		LONG dbghdrEnd = Size + dbghdrOfs;

		return Size >= sizeof(DbiDbgHdr) 
			&& 
			dbghdrOfs < dbghdrEnd 
			&& 
			dbghdrEnd <= _StreamSizes[PDB_STREAM_DBI]
			&&
			Read(PDB_STREAM_DBI, dbghdrOfs, dbgHdr, sizeof(DbiDbgHdr));
	}

	PdbReader()
	{
		_BaseAddress = 0;
	}

	~PdbReader()
	{
		if (_BaseAddress)
		{
			ExFreePool(_BaseAddress);
		}
	}
};

static ULONG RvaFromSrc(ULONG rva, OMAP* pvOmapFromSrc, DWORD b)
{
	ULONG a = 0, o, r;
	OMAP *s = 0, *q;

	if (rva < pvOmapFromSrc->rva)
	{
		return 0;
	}

	do 
	{
		o = (a + b) >> 1;

		q = pvOmapFromSrc + o;
		r = q->rva;

		if (r == rva)
		{
			return q->rvaTo;
		}

		if (r < rva)
		{
			s = q;
			a = o + 1;
		}
		else
		{
			b = o;
		}

	} while (a < b);

	return (rva - s->rva) + s->rvaTo;
}

#include "../winz/str.h"
#include "pdb_util.h"

ULONG ParsePDB(PdbFileHeader* header, SIZE_T ViewSize, PGUID signature, DWORD age, SymStore* pss)
{
	PdbReader pdb;
	int Line = pdb.Init(header, ViewSize, signature, age);

	if (Line)
	{
		return Line;
	}

	ULONG symSize = pdb.getStreamSize(pdb.symrecStream);
	DbiDbgHdr dbgHdr;

	if (symSize < sizeof(SYM_HEADER) || !pdb.ReadDbgHeader(&dbgHdr))
	{
		return __LINE__;
	}

	ULONG Size, nOmapFromSrc = 0;
	OMAP * pvOmapFromSrc = 0;

	struct AUTO_FREE_MEM 
	{
		PVOID BaseAddress;

		AUTO_FREE_MEM()
		{
			BaseAddress = 0;
		}

		~AUTO_FREE_MEM()
		{
			if (BaseAddress) ExFreePool(BaseAddress);
		}

		PVOID Allocate(ULONG Size)
		{
			return BaseAddress = ExAllocatePool(PagedPool, Size);
		}
	};

	AUTO_FREE_MEM afm;

	if (dbgHdr.snOmapFromSrc != MAXUSHORT)
	{
		if (
			!(Size = pdb.getStreamSize(dbgHdr.snOmapFromSrc)) 
			|| 
			(Size % sizeof(OMAP))
			|| 
			!(pvOmapFromSrc = (OMAP *)afm.Allocate(Size))
			||
			!pdb.Read(dbgHdr.snOmapFromSrc, 0, pvOmapFromSrc, Size)
			)
		{
			return __LINE__;
		}

		nOmapFromSrc = Size / sizeof(OMAP), pvOmapFromSrc = (OMAP*)afm.BaseAddress;
		dbgHdr.snSectionHdr = dbgHdr.snSectionHdrOrig;
	}

	Size = pdb.getStreamSize(dbgHdr.snSectionHdr);
	ULONG nSections = Size / sizeof(IMAGE_SECTION_HEADER);

	if (Size != nSections * sizeof(IMAGE_SECTION_HEADER) || !nSections)
	{
		return __LINE__;
	}

	PIMAGE_SECTION_HEADER pish = (PIMAGE_SECTION_HEADER)alloca(Size);
	if (!pdb.Read(dbgHdr.snSectionHdr, 0, pish, Size))
	{
		return __LINE__;
	}

	union {
		PVOID pv;
		PBYTE pb;
		SYM_HEADER* psh;
		ANNOTATIONSYM* pas;
		PUBSYM32* pbs;
	} sym;

	AUTO_FREE_MEM pv;

	if (!(sym.pv = pv.Allocate(symSize)))
	{
		return __LINE__;
	}

	if (!pdb.Read(pdb.symrecStream, 0, sym.pb, symSize))
	{
		return __LINE__;
	}

	ULONG Ss = symSize, len;
	do 
	{
		sym.pv = pv.BaseAddress, symSize = Ss;

		do 
		{
			len = (DWORD)sym.psh->len + sizeof(WORD);

			if ((symSize -= len) < 0) 
			{
				return __LINE__;
			}

			if (sym.psh->type == S_PUB32 && --(sym.pbs->seg) < nSections)
			{
				PCSTR name = sym.pbs->name;
				PBYTE pb = sym.pb + len;
				if ((PBYTE)name < pb)
				{
					ULONG rva = sym.pbs->off + pish[sym.pbs->seg].VirtualAddress;

					if (pvOmapFromSrc)
					{
						rva = RvaFromSrc(rva, pvOmapFromSrc, nOmapFromSrc);
					}

					if (rva)
					{
						pss->Symbol(rva, name);
					}
				}
			}

		} while (sym.pb += len, symSize);


	} while (!pss->EnumSymbolsEnd());

	return 0;
}

_NT_END