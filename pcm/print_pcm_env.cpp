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

    // SystemCounterState before = getSystemCounterState();
    // // ここに測定したいコードを挿入
    // SystemCounterState after = getSystemCounterState();

    // std::cout << "Instructions retired: "
    //           << getInstructionsRetired(before, after) << std::endl;

    m->cleanup();

    return 0;
}