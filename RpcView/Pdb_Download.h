#ifndef _PDB_DOWNLOAD_H_
#define _PDB_DOWNLOAD_H_

#include <windows.h>

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

EXTERN_C BOOL WINAPI download_Pdb(UCHAR* pPdbPath, UINT PdbPathSize, CV_INFO_PDB70* Pdb70Info);
#endif
