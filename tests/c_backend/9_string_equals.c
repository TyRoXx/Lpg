int main(void)
{
    string_ref const r_6 = read_impl();
    bool const r_8 = string_ref_equals(r_6, string_ref_create("", 0));
    unit const r_9 = assert_impl(r_8);
    string_ref_free(&r_6);
    return 0;
}
