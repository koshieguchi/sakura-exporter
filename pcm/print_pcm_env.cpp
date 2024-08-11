#include "cpucounters.h"
#include <iostream>

using namespace pcm;

int main()
{
    PCM *m = PCM::getInstance();
    if (m->program() != PCM::Success)
    {
        std::cerr << "PCM programming error" << std::endl;
        return 1;
    }
    else
    {
        std::cout << "PCM programming success" << std::endl;
    }

    m->cleanup();

    return 0;
}
