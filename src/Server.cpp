#include <iostream>
#include <string>
#include <cctype>
#include <stdexcept>
#include <vector>

enum class TokenType {
    StartAnchor,
    EndAnchor,
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
    size_t i = 0;
    if(!pattern.empty() && pattern[0] == '^')
    {
        tokens.push_back({TokenType::StartAnchor,""});
        i = 1;
    }
    size_t end = pattern.size();
    bool has_end_anchor = (end > i && pattern[end-1] == '$');
    if(has_end_anchor) end--;

    for(;i<end;)
    {
        if(pattern[i] == '\\' && i+1 < end)
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
            if(i + 1 < end && pattern[i+1] == '^')
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
    if(has_end_anchor)
    {
        tokens.push_back({TokenType::EndAnchor,""});
    }
    return tokens;
}

bool match_at(const std::string& input, size_t pos, const std::vector<Token>& tokens)
{
    size_t i = pos;
    size_t t = 0;
    if (!tokens.empty() && tokens[0].type == TokenType::StartAnchor)
    {
        if (pos != 0) return false;
        t = 1;
    }
    for(; t<tokens.size(); ++t)
    {
        if(tokens[t].type == TokenType::EndAnchor)
        {
            return i == input.size();
        }
        if(i >= input.size()) return false;
        unsigned char ch = input[i];
        switch(tokens[t].type)
        {
            case TokenType::Digit:
                if(!std::isdigit(ch)) return false;
                break;
            case TokenType::Word:
                if(!(std::isalnum(ch) || ch == '_')) return false;
                break;
            case TokenType::Literal:
                if(ch != tokens[t].value[0]) return false;
                break;
            case TokenType::PosGroup:
                if(tokens[t].value.find(ch) == std::string::npos) return false;
                break;
            case TokenType::NegGroup:
                if(tokens[t].value.find(ch) != std::string::npos) return false;
                break;
            case TokenType::StartAnchor:
                break;
            case TokenType::EndAnchor:
                break;
        }
        i++;
    }    
    return true;
}

bool match_pattern(const std::string& input_line, const std::string& pattern) {
    auto tokens = tokenize(pattern);
    bool anchored_start = !tokens.empty() && tokens[0].type == TokenType::StartAnchor;
    bool anchored_end = !tokens.empty() && tokens.back().type == TokenType::EndAnchor;
    if (anchored_start) {
        return match_at(input_line,0,tokens);
    } else if (anchored_end) {
        size_t match_len = tokens.size();
        if(input_line.size() + 1 < match_len) return false;
        size_t pos = input_line.size() - (match_len - 1);
        return match_at(input_line,pos,tokens);
    } else {
        for(size_t pos = 0; pos+tokens.size() <= input_line.size(); ++pos)
    {
        if(match_at(input_line,pos,tokens)) return true;
    }
    return false;
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
