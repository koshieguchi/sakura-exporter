#include "cpucounters.h"
#include <iostream>

int main()
{
    pcm::PCM *m = pcm::PCM::getInstance();
    if (m->program() != pcm::PCM::Success)
    {
        std::cerr << "PCM programming error" << std::endl;
        return 1;
    }
    else
    {
        std::cout << "PCM programming success" << std::endl;
    }

    pcm::SystemCounterState before = pcm::getSystemCounterState();
    // ここに測定したいコードを挿入
    pcm::SystemCounterState after = pcm::getSystemCounterState();

    std::cout << "Instructions retired: "
              << pcm::getInstructionsRetired(before, after) << std::endl;

    m->cleanup();

    return 0;
}
