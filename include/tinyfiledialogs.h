#ifndef TINYFILEDIALOGS_H
#define TINYFILEDIALOGS_H

#ifdef __cplusplus
extern "C" {
#endif

const char * tinyfd_openFileDialog(
    char const * aTitle,
    char const * aDefaultPathAndFile,
    int aNumOfFilterPatterns,
    char const * const * aFilterPatterns,
    char const * aSingleFilterDescription,
    int aAllowMultipleSelects);

#ifdef __cplusplus
}
#endif

#endif