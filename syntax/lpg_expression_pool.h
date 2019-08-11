#pragma once
#include "lpg_expression.h"

typedef struct expression_bucket
{
    size_t used;
    expression *elements;
} expression_bucket;

typedef struct expression_pool
{
    expression_bucket *buckets;
    size_t bucket_count;
} expression_pool;

expression_pool expression_pool_create(void);
void expression_pool_free(expression_pool const freed);
expression *expression_pool_allocate(expression_pool *const pool);
