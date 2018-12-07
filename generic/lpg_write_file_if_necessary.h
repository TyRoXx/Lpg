#pragma once

static void write_file_if_necessary(unicode_view const path, unicode_view const content)
{
    unicode_string const source_file_zero_terminated = unicode_view_zero_terminate(path);
    blob_or_error const existing = read_file(source_file_zero_terminated.data);
    if ((existing.error != NULL) ||
        !unicode_view_equals(unicode_view_create(existing.success.data, existing.success.length), content))
    {
        if (success_yes != write_file(path, content))
        {
            LPG_TO_DO();
        }
    }
    blob_free(&existing.success);
    unicode_string_free(&source_file_zero_terminated);
}
