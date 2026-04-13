// include/utils/wildcard.hh
#ifndef WILDCARD_HH
#define WILDCARD_HH

#include <string>
#include <regex>

class Wildcard {
public:
    static bool match(const std::string& pattern, const std::string& str) {
        std::string regex_str = "^";
        for (size_t i = 0; i < pattern.size(); ++i) {
            if (pattern[i] == '*') {
                regex_str += ".*";
            } else if (pattern[i] == '?') {
                regex_str += ".";
            } else if (pattern[i] == '.') {
                regex_str += "\\.";  // Escape literal dot
            } else {
                regex_str += pattern[i];
            }
        }
        regex_str += "$";

        try {
            std::regex re(regex_str);
            return std::regex_match(str, re);
        } catch (...) {
            return pattern == str;
        }
    }
};

#endif // WILDCARD_HH
