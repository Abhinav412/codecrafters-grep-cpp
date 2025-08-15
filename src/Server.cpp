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
    bool one_or_more = false;
    bool zero_or_one = false;
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

    while(i < end)
    {
        Token token;
        if(pattern[i] == '\\' && i+1 < end)
        {
            if(pattern[i+1] == 'd')
            {
                token = {TokenType::Digit, ""};
                i += 2;
            }
            else if(pattern[i+1] == 'w')
            {
                token = {TokenType::Word, ""};
                i += 2;
            }
            else
            {
                token = {TokenType::Literal, std::string(1,pattern[i+1])};
                i += 2;
            }
        }
        else if (pattern[i] == '[')
        {
            size_t close = pattern.find(']',i);
            if(close == std::string::npos) throw std::runtime_error("Unclosed [");
            if(i + 1 < end && pattern[i+1] == '^')
            {
                token = {TokenType::NegGroup, pattern.substr(i+2, close-i-2)};
            }
            else
            {
                token = {TokenType::PosGroup, pattern.substr(i+1,close-i-1)};
            }
            i = close + 1;
        }
        else
        {
            token = {TokenType::Literal, std::string(1,pattern[i])};
            i++;
        }
        if(i < end && pattern[i] == '+')
        {
            token.one_or_more = true;
            i++;
        }
        else if(i < end && pattern[i] == '?')
        {
            token.zero_or_one = true;
            i++;
        }
        tokens.push_back(token);
    }
    if(has_end_anchor)
    {
        tokens.push_back({TokenType::EndAnchor,""});
    }
    return tokens;
}

bool match_token(const Token& token, unsigned char ch)
{
    switch(token.type)
    {
        case TokenType::Digit: return std::isdigit(ch);
        case TokenType::Word: return std::isalnum(ch) || ch == '_';
        case TokenType::Literal: return ch == token.value[0];
        case TokenType::PosGroup: return token.value.find(ch) != std::string::npos;
        case TokenType::NegGroup: return token.value.find(ch) == std::string::npos;
        default: return false;
    }
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
        if(tokens[t].one_or_more)
        {
            size_t start = i;
            size_t count = 0;
            while(i < input.size() && match_token(tokens[t],input[i]))
            {
                i++;
                count++;
            }
            if(count == 0) return false;
            for(size_t split = count; split>=1; --split)
            {
                if(match_at(input,start+split,std::vector<Token>(tokens.begin()+t+1, tokens.end()))){
                    return true;
                }
            }
            return false;
        }
        else if(tokens[t].zero_or_one)
        {
            if(match_at(input, i, std::vector<Token>(tokens.begin()+t+1, tokens.end())))
            return true;
            if(i < input.size() && match_token(tokens[t], input[i]))
            {
                if(match_at(input, i+1, std::vector<Token>(tokens.begin()+t+1, tokens.end())))
                {
                    return true;
                }
            }
            return false;
        }
        else
        {
            if(i >= input.size() || !match_token(tokens[t],input[i])) return false;
            i++;
        }
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
        for(size_t pos = 0; pos <= input_line.size(); ++pos)
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
