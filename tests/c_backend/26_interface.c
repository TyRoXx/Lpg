#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stddef.h>
typedef struct interface_vtable_0
{
    void (*_add_reference)(void *, ptrdiff_t);
    string_ref (*print)(void *);
} interface_vtable_0;
typedef struct interface_reference_0
{
    interface_vtable_0 const *vtable;
    void *self;
} interface_reference_0;
static string_ref lambda_1(void *const self);
static string_ref lambda_2(interface_reference_0 const printed_0);
static void add_reference_0_for_0(void *const self, ptrdiff_t const difference)
{
    size_t *const counter = (size_t *)((char *)self - sizeof(*counter));
    *counter = (size_t)((ptrdiff_t)*counter + difference);
    if (*counter == 0)
    {
        string_ref_free(&(*(string_ref *)self));
        free(counter);
    }
}
static interface_vtable_0 const interface_impl_0_for_0 = {add_reference_0_for_0, lambda_1};
static string_ref lambda_1(void *const self)
{
    string_ref const self_0 = *(string_ref const *)self;
    /*side effect*/
    unit const r_3 = unit_impl;
    string_ref_add_reference(&self_0);
    return self_0;
}
static string_ref lambda_2(interface_reference_0 const printed_0)
{
    /*side effect*/
    unit const r_3 = unit_impl;
    interface_reference_0 r_4 = printed_0;
    string_ref const r_5 = r_4.vtable->print(r_4.self);
    return r_5;
}
int main(void)
{
    interface_reference_0 r_7 = {&interface_impl_0_for_0, malloc(sizeof(size_t) + sizeof(string_literal("a", 1)))};
    *(size_t *)r_7.self = 1;
    r_7.self = (char *)r_7.self + sizeof(size_t);
    *(string_ref *)r_7.self = string_literal("a", 1);
    string_ref const r_8 = lambda_2(r_7);
    bool const r_9 = string_ref_equals(string_literal("a", 1), r_8);
    unit const r_10 = assert_impl(r_9);
    interface_reference_0 r_17 = {&interface_impl_0_for_0, malloc(sizeof(size_t) + sizeof(string_literal("a", 1)))};
    *(size_t *)r_17.self = 1;
    r_17.self = (char *)r_17.self + sizeof(size_t);
    *(string_ref *)r_17.self = string_literal("a", 1);
    string_ref const r_18 = lambda_2(r_17);
    interface_reference_0 r_19 = {&interface_impl_0_for_0, malloc(sizeof(size_t) + sizeof(r_18))};
    *(size_t *)r_19.self = 1;
    r_19.self = (char *)r_19.self + sizeof(size_t);
    *(string_ref *)r_19.self = r_18;
    string_ref_add_reference(&r_18);
    string_ref const r_20 = lambda_2(r_19);
    bool const r_21 = string_ref_equals(string_literal("a", 1), r_20);
    unit const r_22 = assert_impl(r_21);
    r_7.vtable->_add_reference(r_7.self, -1);
    string_ref_free(&r_8);
    r_17.vtable->_add_reference(r_17.self, -1);
    string_ref_free(&r_18);
    r_19.vtable->_add_reference(r_19.self, -1);
    string_ref_free(&r_20);
    return 0;
}
