#include "lpg_write_file.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

success_indicator write_file(unicode_view const path, unicode_view const content)
{
    unicode_string const path_zero_terminated = unicode_view_zero_terminate(path);
    FILE *file;
#ifdef _MSC_VER
    fopen_s(&file, path_zero_terminated.data, "wb");
#else
    file = fopen(path_zero_terminated.data, "wb");
#endif
    unicode_string_free(&path_zero_terminated);
    if (!file)
    {
        return success_no;
    }
    if (fwrite(content.begin, 1, content.length, file) != content.length)
    {
        fclose(file);
        return success_no;
    }
    fclose(file);
    return success_yes;
}

unicode_string write_temporary_file(char const *const content)
{
#ifdef _WIN32
    char *const name =
#ifdef _MSC_VER
        _tempnam
#else
        tempnam
#endif
        (NULL, NULL);
    ASSERT(name);
    size_t const name_length = strlen(name);
    unicode_string const result = {allocate(name_length), name_length};
    memcpy(result.data, name, name_length);
    {
        FILE *file = NULL;
        fopen_s(&file, name, "wb");
        ASSERT(file);
        ASSERT(strlen(content) == fwrite(content, 1, strlen(content), file));
        fflush(file);
        fclose(file);
    }
    // For some reason the file we just created, flush and closed cannot always be opened for reading immediately.
    for (int i = 0; i < 10; ++i)
    {
        FILE *file = NULL;
        fopen_s(&file, name, "rb");
        if (file)
        {
            fclose(file);
            break;
        }
        Sleep(i * 10);
    }
    free(name);
    return result;
#else
    char name[] = "/tmp/XXXXXX";
    int const file = mkstemp(name);
    ASSERT(file >= 0);
    ASSERT(write(file, content, strlen(content)) == (ssize_t)strlen(content));
    close(file);
    size_t const name_length = strlen(name);
    unicode_string const result = {allocate(name_length), name_length};
    memcpy(result.data, name, name_length);
    return result;
#endif
}
