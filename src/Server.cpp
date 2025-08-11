#include <iostream>
#include <string>
#include <cctype>
#include <stdexcept>
#include <vector>

enum class TokenType {
    Literal,
    Digit,
    Word,
    PosGroup,
    NegGroup
};

struct Token {
    TokenType type;
    std::string value;
};

std::vector<Token> tokenize(const std::string& pattern)
{
    std::vector<Token> tokens;
    for(size_t i=0; i<pattern.size();)
    {
        if(pattern[i] == '\\' && i+1 < pattern.size())
        {
            if(pattern[i+1] == 'd')
            {
                tokens.push_back({TokenType::Digit, ""});
                i += 2;
            }
            else if(pattern[i+1] == 'w')
            {
                tokens.push_back({TokenType::Word, ""});
                i += 2;
            }
            else
            {
                tokens.push_back({TokenType::Literal, std::string(1,pattern[i+1])});
                i += 2;
            }
        }
        else if (pattern[i] == '[')
        {
            size_t close = pattern.find(']',i);
            if(close == std::string::npos) throw std::runtime_error("Unclosed [");
            if(i + 1 < pattern.size() && pattern[i+1] == '^')
            {
                tokens.push_back({TokenType::NegGroup, pattern.substr(i+2, close-i-2)});
            }
            else
            {
                tokens.push_back({TokenType::PosGroup, pattern.substr(i+1,close-i-1)});
            }
            i = close + 1;
        }
        else
        {
            tokens.push_back({TokenType::Literal, std::string(1,pattern[i])});
            i++;
        }
    }
    return tokens;
}

bool match_at(const std::string& input, size_t pos, const std::vector<Token>& tokens)
{
    size_t i = pos;
    for(const auto& token : tokens)
    {
        if(i >= input.size()) return false;
        unsigned char ch = input[i];
        switch(token.type)
        {
            case TokenType::Digit:
                if(!std::isdigit(ch)) return false;
                break;
            case TokenType::Word:
                if(!(std::isalnum(ch) || ch == '_')) return false;
                break;
            case TokenType::Literal:
                if(ch != token.value[0]) return false;
                break;
            case TokenType::PosGroup:
                if(token.value.find(ch) == std::string::npos) return false;
                break;
            case TokenType::NegGroup:
                if(token.value.find(ch) != std::string::npos) return false;
                break;
        }
        i++;
    }    
    return true;
}

bool match_pattern(const std::string& input_line, const std::string& pattern) {
    auto tokens = tokenize(pattern);
    for(size_t pos = 0; pos+tokens.size() <= input_line.size(); ++pos)
    {
        if(match_at(input_line,pos,tokens)) return true;
    }
    return false;
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
