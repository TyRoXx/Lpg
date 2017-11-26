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
int main(void)
{
    return 0;
}
