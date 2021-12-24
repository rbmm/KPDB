# KPDB
 
static ULONG ha[] = {
	0x********, // = HashString("drv_0");
	...
	0x********, // = HashString("drv_n-1");
};

LoadNtModule(_countof(ha), ha);

...

DumpStack("xyz");

...

CModule::Cleanup();