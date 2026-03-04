#pragma once
#include <iostream>

template <typename... Args>
void print(Args... args) {
    auto separator = "";
    ((std::cout << separator << args, separator = " "), ...);
    std::cout << std::endl;
}

int main() {
    print("Olá", 2026, 3.14, "C++ é vida");
    return 0;
}