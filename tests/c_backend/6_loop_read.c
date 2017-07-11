int main(void)
{
    for (;;)
    {
        string_ref const r_4 = read_impl();
        unit const r_5 = {};
        fwrite(r_4.data, 1, r_4.length, stdout);
        string_ref_free(&r_4);
    }
    return 0;
}
