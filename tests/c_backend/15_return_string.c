static string_ref lambda_1(void)
{
    string_ref const r_2 = read_impl();
    return r_2;
}
int main(void)
{
    string_ref const r_3 = lambda_1();
    unit const r_4 = {};
    fwrite(r_3.data, 1, r_3.length, stdout);
    string_ref_free(&r_3);
    return 0;
}
