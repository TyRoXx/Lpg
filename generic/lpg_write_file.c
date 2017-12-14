#include "lpg_write_file.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
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
        return failure;
    }
    if (fwrite(content.begin, 1, content.length, file) != content.length)
    {
        fclose(file);
        return failure;
    }
    fclose(file);
    return success;
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
    FILE *file = NULL;
    fopen_s(&file, name, "wb");
    ASSERT(file);
    ASSERT(strlen(content) == fwrite(content, 1, strlen(content), file));
    fclose(file);
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
