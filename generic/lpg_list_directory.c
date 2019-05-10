#include "lpg_list_directory.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_path.h"
#ifdef _WIN32
#include "lpg_win32.h"
#include <windows.h>
#else
#include <dirent.h>
#endif

directory_entry directory_entry_create(unicode_string name)
{
    directory_entry const result = {name};
    return result;
}

void directory_entry_free(directory_entry const freed)
{
    unicode_string_free(&freed.name);
}

directory_listing directory_listing_create(directory_entry *entries, size_t count)
{
    directory_listing const result = {entries, count};
    return result;
}

void directory_listing_free(directory_listing const freed)
{
    for (size_t i = 0; i < freed.count; ++i)
    {
        directory_entry_free(freed.entries[i]);
    }
    if (freed.entries)
    {
        deallocate(freed.entries);
    }
}

directory_listing list_directory(unicode_view const directory)
{
#ifdef _WIN32
    unicode_view const search_string_pieces[] = {directory, unicode_view_from_c_str("*")};
    unicode_string const search_string = path_combine(search_string_pieces, LPG_ARRAY_SIZE(search_string_pieces));
    win32_string const search_string_c_str = to_win32_path(unicode_view_from_string(search_string));
    WIN32_FIND_DATAW findData;
    HANDLE const reader = FindFirstFileW(search_string_c_str.c_str, &findData);
    if (reader == INVALID_HANDLE_VALUE)
    {
        LPG_TO_DO();
    }
    directory_entry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    for (;;)
    {
        if (wcscmp(findData.cFileName, L".") && wcscmp(findData.cFileName, L".."))
        {
            directory_entry const new_entry = directory_entry_create(
                from_win32_string(win32_string_create(findData.cFileName, wcslen(findData.cFileName))));
            if (count == capacity)
            {
                capacity *= 2u;
                if (capacity == 0)
                {
                    capacity = 10;
                }
                entries = reallocate_array(entries, capacity, sizeof(*entries));
            }
            entries[count] = new_entry;
            ++count;
        }

        if (!FindNextFileW(reader, &findData))
        {
            break;
        }
    }
    FindClose(reader);
    return directory_listing_create(entries, count);
#else
    unicode_string directory_c_str = unicode_view_copy(directory);
    DIR *reader = opendir(unicode_string_c_str(&directory_c_str));
    unicode_string_free(&directory_c_str);
    if (!reader)
    {
        LPG_TO_DO();
    }
    directory_entry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    for (;;)
    {
        struct dirent *const entry = readdir(reader);
        if (!entry)
        {
            break;
        }
        if (!strcmp(entry->d_name, "."))
        {
            continue;
        }
        if (!strcmp(entry->d_name, ".."))
        {
            continue;
        }
        directory_entry const new_entry = directory_entry_create(unicode_string_from_c_str(entry->d_name));
        if (count == capacity)
        {
            capacity *= 2u;
            if (capacity == 0)
            {
                capacity = 10;
            }
            entries = reallocate_array(entries, capacity, sizeof(*entries));
        }
        entries[count] = new_entry;
        ++count;
    }
    closedir(reader);
    return directory_listing_create(entries, count);
#endif
}
