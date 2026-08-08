/* SPDX-License-Identifier: Apache-2.0 */

#include <rPlayer/ImageRenderer.hpp>
#include <rPlayer/VideoPlayer.hpp>

#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <iostream>
#include <string>

using namespace rasterm::rPlayer;

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 20);
    std::cout << "rasterm player\n";
    std::cout << "1. Video file\n2. Image file\nChoose (1-2): ";
    int opt = 0;
    if (!(std::cin >> opt)) {
        return 0;
    }
    std::cin.ignore();
    std::cout << "Path: ";
    std::string src;
    std::getline(std::cin, src);

    switch (opt) {
    case 1: playVideoFile(src); break;
    case 2: renderImageFile(src); break;
    default: break;
    }
    return 0;
}