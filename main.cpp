#include "daemon.h"

int main(int argc, char** argv)
{
    Daemon d(argc, argv);
    d.setup();
    return 0;
}
