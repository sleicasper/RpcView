#define _CRT_SECURE_NO_WARNINGS
#include "Pdb.h"
#include <conio.h>
#include <Strsafe.h>
#include <Dbghelp.h>

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#define RSDS_SIGNATURE 'SDSR'
#define PDB_MAX_SYMBOL_SIZE	1000


#include <iostream>
#include <Windows.h>
#include <stdio.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <sstream>
#include <urlmon.h>
#pragma comment(lib,"urlmon.lib")

//Only for PDB2.0 format!
struct CV_INFO_PDB20{
  DWORD	CvSignature;
  DWORD	Offset;
  DWORD Signature;
  DWORD Age;
  BYTE PdbFileName[MAX_PATH];
};

//Only for PDB7.0 format!
typedef struct _CV_INFO_PDB70{
	DWORD	CvSignature;
	GUID	Signature;
	DWORD	Age;
	BYTE	PdbFileName[MAX_PATH];
} CV_INFO_PDB70;


typedef struct _PdbCtxt_T{
	HANDLE	hProcess;
	void*	pModuleBase;
	ULONG	ModuleSize;
}PdbCtxt_T;



std::vector<std::string> split2vec(std::string text, char delim)
{
    std::string line;
    std::vector<std::string> vec;
    std::stringstream ss(text);
    while (std::getline(ss, line, delim))
    {
        vec.push_back(line);
    }
    return vec;
}

bool _is_dir_win(std::string aPath)
{
    DWORD attrib = GetFileAttributes(aPath.c_str());
    if (attrib == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }
    bool res = false;

    if ((attrib & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        res = true;
    }
    return res;
}
bool _is_file_win(std::string aPath)
{
    DWORD attrib = GetFileAttributesA(aPath.c_str());
    bool res;

    if ((attrib & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        res = false;
    }
    else
    {
        res = true;
    }
    return res;
}
void ensure_directory_create(std::string aPath)
{
    std::vector<std::string> path_vec = split2vec(aPath, '\\');
    size_t endidx = path_vec.size();
    if (aPath[aPath.size() - 1] != '\\')
    {
        endidx--;
    }
    std::string res;
    for(size_t idx=0; idx < endidx; idx++)
    {
       res = res + path_vec[idx] + "\\";
       CreateDirectoryA(res.c_str(), NULL);
    }
}

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
	CHAR			SymbolPath[MAX_PATH] = { 0 };
	CHAR			NtSymbolPath[MAX_PATH] = { 0 };
	BOOL			bResult = FALSE;

	if (!GetModulePdbInfo(hProcess, pModuleBase, &Pdb70Info)) goto End;
	/*
	if (!SymFindFileInPath(hProcess, "c:\\symbols\\", Pdb70Info.PdbFileName, &Pdb70Info.Signature, Pdb70Info.Age, 0, SSRVOPT_GUIDPTR, pPdbPath, NULL, NULL))
	{
		printf("SymFindFileInPath failed %u\n", GetLastError());
		goto End;
	}
	*/
    if (strchr((char*)Pdb70Info.PdbFileName, '\\') != NULL)
    {
        StringCbPrintfA((STRSAFE_LPSTR)pPdbPath, PdbPathSize, "%hs", Pdb70Info.PdbFileName);
    }
    else
    {
        if (GetEnvironmentVariableA("RpcViewSymbolPath", NtSymbolPath, sizeof(NtSymbolPath)) == 0)
        {
            if (GetEnvironmentVariableA("_NT_SYMBOL_PATH", NtSymbolPath, sizeof(NtSymbolPath)) == 0)
            {
                return FALSE;
            }
        }
        std::string NtSymbolPathStr = NtSymbolPath;
        std::string ntsymbolprefix = NtSymbolPathStr.substr(0, 4);
        if (_stricmp(ntsymbolprefix.c_str(), "SRV*"))
        {
            return FALSE;
        }
        NtSymbolPathStr = NtSymbolPath + 4;
        std::vector<std::string> path_vec = split2vec(NtSymbolPathStr, '*');
        std::string saving_prefix;
        std::string symserver_prefix;

        for (auto iter : path_vec)
        {
            if (_stricmp(iter.substr(0, 4).c_str(), "http"))
            {
                symserver_prefix = iter;
            }
            else
            {
                if (saving_prefix.size() == 0)
                {
                    saving_prefix = iter;
                }
                StringCbPrintfA((STRSAFE_LPSTR)pPdbPath, PdbPathSize, "%s\\%s\\%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%X\\%s",
                        iter.c_str(),
                        Pdb70Info.PdbFileName,
                        Pdb70Info.Signature.Data1,
                        Pdb70Info.Signature.Data2,
                        Pdb70Info.Signature.Data3,
                        Pdb70Info.Signature.Data4[0],
                        Pdb70Info.Signature.Data4[1],
                        Pdb70Info.Signature.Data4[2],
                        Pdb70Info.Signature.Data4[3],
                        Pdb70Info.Signature.Data4[4],
                        Pdb70Info.Signature.Data4[5],
                        Pdb70Info.Signature.Data4[6],
                        Pdb70Info.Signature.Data4[7],
                        Pdb70Info.Age,
                        Pdb70Info.PdbFileName
                        );
                // check exist
                if (_is_file_win(pPdbPath))
                {
                    return TRUE;
                }
            }
        }
        if (symserver_prefix.size() > 0 && saving_prefix.size() > 0)
        {
            CHAR downloadurl[MAX_PATH];
            StringCbPrintfA((STRSAFE_LPSTR)downloadurl, MAX_PATH, "%s/%s/%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%X/%s",
                    symserver_prefix.c_str(),
                    Pdb70Info.PdbFileName,
                    Pdb70Info.Signature.Data1,
                    Pdb70Info.Signature.Data2,
                    Pdb70Info.Signature.Data3,
                    Pdb70Info.Signature.Data4[0],
                    Pdb70Info.Signature.Data4[1],
                    Pdb70Info.Signature.Data4[2],
                    Pdb70Info.Signature.Data4[3],
                    Pdb70Info.Signature.Data4[4],
                    Pdb70Info.Signature.Data4[5],
                    Pdb70Info.Signature.Data4[6],
                    Pdb70Info.Signature.Data4[7],
                    Pdb70Info.Age,
                    Pdb70Info.PdbFileName
                    );
            StringCbPrintfA((STRSAFE_LPSTR)pPdbPath, PdbPathSize, "%s\\%s\\%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%X\\%s",
                    saving_prefix.c_str(),
                    Pdb70Info.PdbFileName,
                    Pdb70Info.Signature.Data1,
                    Pdb70Info.Signature.Data2,
                    Pdb70Info.Signature.Data3,
                    Pdb70Info.Signature.Data4[0],
                    Pdb70Info.Signature.Data4[1],
                    Pdb70Info.Signature.Data4[2],
                    Pdb70Info.Signature.Data4[3],
                    Pdb70Info.Signature.Data4[4],
                    Pdb70Info.Signature.Data4[5],
                    Pdb70Info.Signature.Data4[6],
                    Pdb70Info.Signature.Data4[7],
                    Pdb70Info.Age,
                    Pdb70Info.PdbFileName
                    );
            ensure_directory_create(pPdbPath);
            HRESULT hr = URLDownloadToFileA(0, downloadurl, pPdbPath, 0, NULL);
            if (hr != S_OK)
            {
                return FALSE;
            }
            // check exist
            if (_is_file_win(pPdbPath))
            {
                return TRUE;
            }

        }

        return 0;
    }
	bResult = TRUE;
End:
	return (bResult);
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
