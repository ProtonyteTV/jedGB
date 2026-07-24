#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinyfiledialogs.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>

static char g_win_path_buffer[MAX_PATH];

const char * tinyfd_openFileDialog(
    char const * aTitle,
    char const * aDefaultPathAndFile,
    int aNumOfFilterPatterns,
    char const * const * aFilterPatterns,
    char const * aSingleFilterDescription,
    int aAllowMultipleSelects)
{
    OPENFILENAMEA ofn;
    char szFile[260] = { 0 };

    if (aDefaultPathAndFile && strlen(aDefaultPathAndFile) > 0) {
        strncpy(szFile, aDefaultPathAndFile, sizeof(szFile) - 1);
    }

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Game Boy ROMs (*.gb;*.gbc)\0*.gb;*.gbc\0All Files (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = aTitle;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        strncpy(g_win_path_buffer, ofn.lpstrFile, sizeof(g_win_path_buffer) - 1);
        return g_win_path_buffer;
    }

    return NULL;
}

#else

// Basic stub for POSIX/Mac/Linux fallback
const char * tinyfd_openFileDialog(
    char const * aTitle,
    char const * aDefaultPathAndFile,
    int aNumOfFilterPatterns,
    char const * const * aFilterPatterns,
    char const * aSingleFilterDescription,
    int aAllowMultipleSelects)
{
    (void)aTitle;
    (void)aDefaultPathAndFile;
    (void)aNumOfFilterPatterns;
    (void)aFilterPatterns;
    (void)aSingleFilterDescription;
    (void)aAllowMultipleSelects;
    return NULL;
}

#endif