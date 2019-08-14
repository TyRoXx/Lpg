#include "test_fuzz.h"
#include "fuzz_target.h"
#include "lpg_array_size.h"
#include "lpg_list_directory.h"
#include "lpg_path.h"
#include "lpg_read_file.h"
#include "lpg_thread.h"
#include "test.h"

static unicode_string find_fuzz_corpus_directory(void)
{
    unicode_view const pieces[] = {path_remove_leaf(path_remove_leaf(unicode_view_from_c_str(__FILE__))),
                                   unicode_view_from_c_str("fuzz"), unicode_view_from_c_str("corpus")};
    return path_combine(pieces, LPG_ARRAY_SIZE(pieces));
}

static void fuzz_number_of_entries(unicode_view const corpus, directory_entry const *const first, size_t const count)
{
    for (size_t i = 0; i < count; ++i)
    {
        directory_entry const entry = first[i];
        unicode_view const full_path_pieces[] = {corpus, unicode_view_from_string(entry.name)};
        unicode_string const full_path = path_combine(full_path_pieces, LPG_ARRAY_SIZE(full_path_pieces));
        blob_or_error const read_result = read_file_unicode_view_name(unicode_view_from_string(full_path));
        REQUIRE(!read_result.error);
        ParserTypeCheckerFuzzTarget((uint8_t const *)read_result.success.data, read_result.success.length);
        blob_free(&read_result.success);
        unicode_string_free(&full_path);
    }
}

typedef struct fuzzing_thread
{
    lpg_thread handle;
    unicode_view corpus;
    directory_entry const *first;
    size_t count;
} fuzzing_thread;

static void fuzz_number_of_entries_thread(void *const user)
{
    fuzzing_thread const *const thread = user;
    fuzz_number_of_entries(thread->corpus, thread->first, thread->count);
}

void test_fuzz(void)
{
    unicode_string const corpus = find_fuzz_corpus_directory();
    directory_listing const corpus_entries = list_directory(unicode_view_from_string(corpus));
    fuzzing_thread threads[3];
    size_t assigned = 0;
    size_t const share = (corpus_entries.count / LPG_ARRAY_SIZE(threads));
    for (size_t i = 0; i < LPG_ARRAY_SIZE(threads); ++i)
    {
        fuzzing_thread *const thread = threads + i;
        thread->corpus = unicode_view_from_string(corpus);
        thread->first = corpus_entries.entries + assigned;
        thread->count = ((i + 1) == LPG_ARRAY_SIZE(threads)) ? (corpus_entries.count - assigned) : share;
        assigned += share;
        create_thread_result const result = create_thread(fuzz_number_of_entries_thread, thread);
        REQUIRE(result.is_success == success_yes);
        thread->handle = result.success;
    }
    for (size_t i = 0; i < LPG_ARRAY_SIZE(threads); ++i)
    {
        join_thread(threads[i].handle);
    }
    directory_listing_free(corpus_entries);
    unicode_string_free(&corpus);
}
