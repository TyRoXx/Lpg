#include <lpg_std_unit.h>
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
static unit lambda_2(interface_reference_0 const printed_0);
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
static unit lambda_2(interface_reference_0 const printed_0)
{
    /*side effect*/
    unit const r_3 = unit_impl;
    return r_3;
}
int main(void)
{
    interface_reference_0 r_2 = {&interface_impl_0_for_0, malloc(sizeof(size_t) + sizeof(string_literal("", 0)))};
    *(size_t *)r_2.self = 1;
    r_2.self = (char *)r_2.self + sizeof(size_t);
    *(string_ref *)r_2.self = string_literal("", 0);
    unit const r_3 = lambda_2(r_2);
    r_2.vtable->_add_reference(r_2.self, -1);
    return 0;
}
