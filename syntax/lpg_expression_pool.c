#include "lpg_expression_pool.h"
#include "lpg_allocate.h"

static expression_bucket expression_bucket_create(size_t used, expression *elements)
{
    expression_bucket const result = {used, elements};
    return result;
}

static void expression_bucket_free(expression_bucket const freed)
{
    if (freed.elements)
    {
        deallocate(freed.elements);
    }
}

expression_pool expression_pool_create(void)
{
    expression_pool const result = {NULL, 0};
    return result;
}

void expression_pool_free(expression_pool const freed)
{
    for (size_t i = 0; i < freed.bucket_count; ++i)
    {
        expression_bucket_free(freed.buckets[i]);
    }
    if (freed.buckets)
    {
        deallocate(freed.buckets);
    }
}

static size_t nth_bucket_size(size_t n)
{
    size_t const first_size = 32;
    size_t const result = (first_size << n);
    ASSUME(result >= first_size);
    return result;
}

expression *expression_pool_allocate(expression_pool *const pool)
{
    if (!pool->buckets || (pool->buckets[pool->bucket_count - 1].used == nth_bucket_size(pool->bucket_count - 1)))
    {
        pool->buckets = reallocate_array(pool->buckets, pool->bucket_count + 1, sizeof(*pool->buckets));
        pool->buckets[pool->bucket_count] =
            expression_bucket_create(0, allocate_array(nth_bucket_size(pool->bucket_count), sizeof(expression)));
        pool->bucket_count += 1;
    }
    expression_bucket *const current_bucket = &pool->buckets[pool->bucket_count - 1];
    ASSUME(current_bucket->used < nth_bucket_size(pool->bucket_count - 1));
    expression *const result = current_bucket->elements + current_bucket->used;
    current_bucket->used += 1;
    return result;
}
