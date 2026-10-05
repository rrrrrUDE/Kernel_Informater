#include "../include/kicmd_internal.h"
#include <stdlib.h>

int main(int argc, char **argv)
{
	atexit(close_ki);
	return kicmd_dispatch(argc, argv);
}
