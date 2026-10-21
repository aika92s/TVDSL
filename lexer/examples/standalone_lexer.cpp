#include <iostream>
#include <iterator>
#include <string>

#include "dfa_table.hpp"

int main(int argc, char** argv) {
    using namespace funny_dfa;
    std::string src = argc > 1 ? argv[1] : std::string(std::istreambuf_iterator<char>(std::cin), {});
    int errors = 0;
    for (size_t i = 0; i < src.size();) {
        int state = kStartState, tok = 0;
        size_t end = i;  // конец последнего принимающего префикса
        bool skip = false;
        for (size_t j = i; j < src.size(); ++j) {
            unsigned char c = src[j];
            state = c < kAlphabetSize ? kTransitions[state][c] : kTrapState;  // байты >= 128 -> ловушка
            if (state == kTrapState) break;
            if (kAcceptToken[state]) { end = j + 1; tok = kAcceptToken[state]; skip = kIsSkip[state]; }
        }
        if (end == i) { std::cout << "ERROR  byte " << int(static_cast<unsigned char>(src[i])) << "  @" << i << "\n"; ++errors; ++i; continue; }
        if (!skip) std::cout << kTokenNames[tok] << "  '" << src.substr(i, end - i) << "'  @" << i << "\n";
        i = end;
    }
    return errors ? 1 : 0;
}
