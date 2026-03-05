#pragma once
#include <iostream>

namespace core_utils
{

    template <typename... Args>
    void print(Args... args) {
        auto separator = "";
        ((std::cout << separator << args, separator = " "), ...);
        std::cout << std::endl;
    }

}

