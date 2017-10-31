#include <lpg_std_string.h>
typedef struct interface_vtable_0
{
    string_ref (*print)();
} interface_vtable_0;
typedef struct interface_reference_0
{
    interface_vtable_0 const *vtable;
    void *self;
} interface_reference_0;
static string_ref lambda_1(void *const self, string_ref const self_0);
static interface_vtable_0 const interface_impl_0_for_0 = {lambda_1};
static string_ref lambda_1(void *const self, string_ref const self_0)
{
    /*side effect*/
    string_ref_add_reference(&self_0);
    return self_0;
}
int main(void)
{
    return 0;
}
