#include "daemon.h"

int main(int argc, char** argv)
{
    Daemon d(argc, argv);
    d.run();
    return 0;
}
