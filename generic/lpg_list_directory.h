#pragma once

#include "lpg_unicode_view.h"

typedef struct directory_entry
{
    unicode_string name;
} directory_entry;

directory_entry directory_entry_create(unicode_string name);
void directory_entry_free(directory_entry const freed);

typedef struct directory_listing
{
    directory_entry *entries;
    size_t count;
} directory_listing;

directory_listing directory_listing_create(directory_entry *entries, size_t count);
void directory_listing_free(directory_listing const freed);

directory_listing list_directory(unicode_view const directory);
