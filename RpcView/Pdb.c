#define _CRT_SECURE_NO_WARNINGS
#include "Pdb.h"
#include "Pdb_Download.h"
#include <conio.h>
#include <Strsafe.h>
#include <Dbghelp.h>

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#define RSDS_SIGNATURE 'SDSR'
#define PDB_MAX_SYMBOL_SIZE	1000


//------------------------------------------------------------------------------
BOOL WINAPI GetModulePdbInfo(HANDLE hProcess, VOID* pModuleBase, CV_INFO_PDB70* pPdb70Info)
{
	UCHAR*					pBase = (UCHAR*)pModuleBase;
	IMAGE_DOS_HEADER		ImageDosHeader;
	IMAGE_NT_HEADERS		ImageNtHeaders;
	IMAGE_DEBUG_DIRECTORY	ImageDebugDirectory;
	BOOL					bResult = FALSE;
#ifdef _WIN64
	BOOL					bWow64;
	IMAGE_NT_HEADERS32		ImageNtHeaders32;

	if (hProcess==NULL) hProcess = GetCurrentProcess();

	if (!IsWow64Process(hProcess, &bWow64)) goto End;
	if (bWow64==TRUE)
	{
		if (!ReadProcessMemory(hProcess, pModuleBase, &ImageDosHeader, sizeof(ImageDosHeader), NULL)) goto End;
		if (!ReadProcessMemory(hProcess, pBase + ImageDosHeader.e_lfanew, &ImageNtHeaders32, sizeof(ImageNtHeaders32), NULL)) goto End;
		if (!ReadProcessMemory(hProcess, pBase + ImageNtHeaders32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress, &ImageDebugDirectory, sizeof(ImageDebugDirectory), NULL)) goto End;
		if (!ReadProcessMemory(hProcess, pBase + ImageDebugDirectory.AddressOfRawData, pPdb70Info, sizeof(*pPdb70Info), NULL)) goto End;
	}
	else
#endif
	{
		if (!ReadProcessMemory(hProcess, pModuleBase, &ImageDosHeader, sizeof(ImageDosHeader), NULL)) goto End;
		if (!ReadProcessMemory(hProcess, pBase + ImageDosHeader.e_lfanew, &ImageNtHeaders, sizeof(ImageNtHeaders), NULL)) goto End;
		if (!ReadProcessMemory(hProcess, pBase + ImageNtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress, &ImageDebugDirectory, sizeof(ImageDebugDirectory), NULL)) goto End;
		if (!ReadProcessMemory(hProcess, pBase + ImageDebugDirectory.AddressOfRawData, pPdb70Info, sizeof(*pPdb70Info), NULL)) goto End;
	}
	if (pPdb70Info->CvSignature != RSDS_SIGNATURE )
	{
		_cprintf("Invalid CvSignature");
		goto End;
	}
	bResult = TRUE;
End:
	return (bResult);
}


//------------------------------------------------------------------------------
BOOL WINAPI GetPdbFilePath(HANDLE hProcess, VOID* pModuleBase, UCHAR* pPdbPath, UINT PdbPathSize)
{
	CV_INFO_PDB70	Pdb70Info;

	if (!GetModulePdbInfo(hProcess, pModuleBase, &Pdb70Info))
    {
        return FALSE;
    }
    if (strchr((char*)Pdb70Info.PdbFileName, '\\') != NULL)
    {
        StringCbPrintfA((STRSAFE_LPSTR)pPdbPath, PdbPathSize, "%hs", Pdb70Info.PdbFileName);
        return TRUE;
    }
    else
    {
        return download_Pdb(pPdbPath, PdbPathSize, &Pdb70Info);
    }
}


//------------------------------------------------------------------------------
__checkReturn void* WINAPI PdbInit(__in HANDLE hProcess, __in VOID* pModuleBase, __in UINT ModuleSize)
{
	UCHAR		PdbPath[MAX_PATH];
	PdbCtxt_T*	pPdbCtxt			= NULL;

    if (!SymInitialize(hProcess, NULL, FALSE)) goto End;
	if (!GetPdbFilePath(hProcess, pModuleBase, PdbPath, sizeof(PdbPath))) goto End;
	if (!SymLoadModule64(hProcess, NULL, (STRSAFE_LPCSTR)PdbPath, NULL,(DWORD64) pModuleBase, ModuleSize))
	{
		printf("SymLoadModule64 failed\n");
		SymCleanup(hProcess);
		goto End;
	}
	//
	// All is ok, so we can allocate and init the PDB private context
	//
	pPdbCtxt = (PdbCtxt_T*)malloc(sizeof(PdbCtxt_T));
	if (pPdbCtxt!=NULL)
	{
		ZeroMemory(pPdbCtxt, sizeof(PdbCtxt_T));

		pPdbCtxt->hProcess		= hProcess;
		pPdbCtxt->ModuleSize	= ModuleSize;
		pPdbCtxt->pModuleBase	= pModuleBase;
	}
End:
	return (pPdbCtxt);
}


//------------------------------------------------------------------------------
__checkReturn BOOL WINAPI PdbGetSymbolName(__in void* hCtxt, __in VOID* pSymbol, __out WCHAR* pName, __in UINT NameLength)
{
	BOOL				bResult		= FALSE;
	IMAGEHLP_SYMBOL64*	pSymbolInfo	= NULL;
	DWORD64				dwDisp		= 0;
	PdbCtxt_T*			pPdbCtxt	= (PdbCtxt_T*)hCtxt;

	if (pPdbCtxt==NULL) goto End;

	pSymbolInfo=(IMAGEHLP_SYMBOL64*)malloc( PDB_MAX_SYMBOL_SIZE );
	if (pSymbolInfo==NULL) goto End;
	ZeroMemory(pSymbolInfo, PDB_MAX_SYMBOL_SIZE);

	pSymbolInfo->MaxNameLength	= NameLength;
	pSymbolInfo->SizeOfStruct	= sizeof(IMAGEHLP_SYMBOL64);

	bResult = SymGetSymFromAddr64( pPdbCtxt->hProcess, (DWORD64)pSymbol, &dwDisp, pSymbolInfo);
	if (!bResult)	goto End;
	if (dwDisp!=0)	goto End;
	StringCbPrintfW(pName,NameLength,L"%S",pSymbolInfo->Name);
End:
	if (pSymbolInfo!=NULL) free(pSymbolInfo);
	return (bResult);
}


//------------------------------------------------------------------------------
void WINAPI PdbUninit(__in void* hCtxt)
{
	PdbCtxt_T*			pPdbCtxt	= (PdbCtxt_T*)hCtxt;

	if (pPdbCtxt==NULL) goto End;
	SymUnloadModule64(pPdbCtxt->hProcess,(DWORD64)pPdbCtxt->pModuleBase);
	free(pPdbCtxt);
End:
	return;
}
