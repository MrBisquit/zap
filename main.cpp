#include <iostream>
#define VERSION "pre alpha 0.1"

void help() {
    std::cout << "Ignis V" <<VERSION << std::endl;
    std::cout << "      -flag [<value>]"<< std::endl;
    std::cout << "\n";
    std::cout << "      -c [<path>]"<< std::endl;
    std::cout << "          compiles provied files\n";
    std::cout << "      -v\n";
    std::cout << "          displays installed version of Ignis\n";
    std::cout << "      -h\n";
    std::cout << "        displays this help\n";
}

int main(int argc, char* argv[]) {
    //ignis -c file.ign fil1.ign

    if (argc < 2) {
        help();
    }else {
        std::string command = argv[1];
        if (command == "-h" || command == "-help") {
            help();
        }else if (command == "-v" || command == "-version") {
            std::cout << VERSION << std::endl;
        }else {

        }
    }
    return 0;
}