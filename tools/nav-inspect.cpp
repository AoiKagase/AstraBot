// SPDX-License-Identifier: MPL-2.0
#include "nav_inspection.hpp"
#include <iostream>
int main(int argc, char** argv) {
    return astrabot::tools::inspection::cli(argc, argv, std::cout);
}
