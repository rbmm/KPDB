#pragma once

struct __declspec(novtable) SymStore 
{
	NTSTATUS GetSymbols(HMODULE hmod, PCWSTR NtSymbolPath);
	NTSTATUS GetSymbols(PCWSTR PdbPath);

	virtual void Symbol(ULONG rva, PCSTR name) = 0;
	virtual BOOL EnumSymbolsEnd() = 0;
private:
	NTSTATUS GetSymbols(HANDLE hFile, PGUID signature, DWORD age);

};

