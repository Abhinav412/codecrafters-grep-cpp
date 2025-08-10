#include <iostream>
#include <string>
#include <cctype>
#include <stdexcept>

bool match_pattern(const std::string& input_line, const std::string& pattern) {
    if (pattern == "\\d")
    {
        for(unsigned char ch: input_line)
        {
            if (std::isdigit(ch))
            {
                return true;
            }
        }
        return false;
    }
    else if (pattern == "\\w")
    {
        for (unsigned char ch : input_line)
        {
            if (std::isalnum(ch) || ch == '_')
            {
                return true;
            }
        }
        return false;
    }
    else if (pattern.size() >=2 && pattern.front() == '[' && pattern.back() == ']')
    {
        const std::string inside = pattern.substr(1,pattern.size()-2);
        if (inside.empty()) return false;

        for(unsigned char ch:input_line)
        {
            if (inside.find(ch) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }
    else if (pattern.length() == 1) {
        return input_line.find(pattern) != std::string::npos;
    }
    else {
        throw std::runtime_error("Unhandled pattern " + pattern);
    }
}

int main(int argc, char* argv[]) {
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    std::cerr << "Logs from your program will appear here" << std::endl;

    if (argc != 3) {
        std::cerr << "Expected two arguments" << std::endl;
        return 1;
    }

    std::string flag = argv[1];
    std::string pattern = argv[2];

    if (flag != "-E") {
        std::cerr << "Expected first argument to be '-E'" << std::endl;
        return 1;
    }

    //Uncomment this block to pass the first stage
    
    std::string input_line;
    std::getline(std::cin, input_line);
    
    try {
        if (match_pattern(input_line, pattern)) {
            return 0;
        } else {
            return 1;
        }
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
