#include "lpg_cli.h"
#if LPG_WITH_VLD
#include <vld.h>
#endif

int main(int const argc, char **const argv)
{
    return run_cli(argc, argv);
}
