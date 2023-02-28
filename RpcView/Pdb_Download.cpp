#include "Pdb_Download.h"
#include <iostream>
#include <Windows.h>
#include <stdio.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <sstream>
#include <urlmon.h>
#pragma comment(lib,"urlmon.lib")

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

EXTERN_C BOOL WINAPI download_Pdb(UCHAR* pPdbPath, UINT PdbPathSize, CV_INFO_PDB70* Pdb70Info)
{
    CHAR			NtSymbolPath[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableA("RpcViewSymbolPath", NtSymbolPath, sizeof(NtSymbolPath)) == 0)
    {
        if (GetEnvironmentVariableA("_NT_SYMBOL_PATH", NtSymbolPath, sizeof(NtSymbolPath)) == 0)
        {
            return FALSE;
        }
    }
    MessageBox(NULL, "hello", "goood", MB_OK);
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
                    Pdb70Info->PdbFileName,
                    Pdb70Info->Signature.Data1,
                    Pdb70Info->Signature.Data2,
                    Pdb70Info->Signature.Data3,
                    Pdb70Info->Signature.Data4[0],
                    Pdb70Info->Signature.Data4[1],
                    Pdb70Info->Signature.Data4[2],
                    Pdb70Info->Signature.Data4[3],
                    Pdb70Info->Signature.Data4[4],
                    Pdb70Info->Signature.Data4[5],
                    Pdb70Info->Signature.Data4[6],
                    Pdb70Info->Signature.Data4[7],
                    Pdb70Info->Age,
                    Pdb70Info->PdbFileName
                    );
            // check exist
            if (_is_file_win((char*)pPdbPath))
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
                Pdb70Info->PdbFileName,
                Pdb70Info->Signature.Data1,
                Pdb70Info->Signature.Data2,
                Pdb70Info->Signature.Data3,
                Pdb70Info->Signature.Data4[0],
                Pdb70Info->Signature.Data4[1],
                Pdb70Info->Signature.Data4[2],
                Pdb70Info->Signature.Data4[3],
                Pdb70Info->Signature.Data4[4],
                Pdb70Info->Signature.Data4[5],
                Pdb70Info->Signature.Data4[6],
                Pdb70Info->Signature.Data4[7],
                Pdb70Info->Age,
                Pdb70Info->PdbFileName
                );
        StringCbPrintfA((STRSAFE_LPSTR)pPdbPath, PdbPathSize, "%s\\%s\\%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%X\\%s",
                saving_prefix.c_str(),
                Pdb70Info->PdbFileName,
                Pdb70Info->Signature.Data1,
                Pdb70Info->Signature.Data2,
                Pdb70Info->Signature.Data3,
                Pdb70Info->Signature.Data4[0],
                Pdb70Info->Signature.Data4[1],
                Pdb70Info->Signature.Data4[2],
                Pdb70Info->Signature.Data4[3],
                Pdb70Info->Signature.Data4[4],
                Pdb70Info->Signature.Data4[5],
                Pdb70Info->Signature.Data4[6],
                Pdb70Info->Signature.Data4[7],
                Pdb70Info->Age,
                Pdb70Info->PdbFileName
                );
        ensure_directory_create((char*)pPdbPath);
        HRESULT hr = URLDownloadToFileA(0, downloadurl, (LPCSTR)pPdbPath, 0, NULL);
        if (hr != S_OK)
        {
            return FALSE;
        }
        // check exist
        if (_is_file_win((CHAR*)pPdbPath))
        {
            return TRUE;
        }

    }

    return 0;
}
