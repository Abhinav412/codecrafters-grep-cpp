#include <iostream>
#include <string>
#include <cctype>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>
#include <functional>

enum class TokenType {
    StartAnchor,
    EndAnchor,
    Literal,
    Digit,
    Word,
    PosGroup,
    NegGroup,
    Dot,
    Group
};

struct Token {
    TokenType type;
    std::string value;
    bool one_or_more = false;
    bool zero_or_one = false;
    std::vector<std::vector<Token>> alternatives;
};

static std::vector<Token> parse_range(const std::string& pattern, size_t begin, size_t end);

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

    auto middle = parse_range(pattern, i, end);
    tokens.insert(tokens.end(), middle.begin(), middle.end());

    if(has_end_anchor)
    {
        tokens.push_back({TokenType::EndAnchor,""});
    }
    return tokens;
}

static std::vector<Token> parse_range(const std::string& pattern, size_t begin, size_t end)
{
    std::vector<Token> out;
    size_t i = begin;
    while(i < end)
    {
        Token token;
        if(pattern[i] == '\\' && i+1 < end)
        {
            char c = pattern[i+1];
            if (c == 'd') {token = {TokenType::Digit,""}; i += 2;}
            else if(c == 'w') {token = {TokenType::Word,""}; i += 2;}
            else {token = {TokenType::Literal, std::string(1,c)}; i += 2;}
        }
        else if (pattern[i] == '[')
        {
            size_t close = i + 1;
            while(close < end && pattern[close] != ']')
            {
                if(pattern[close] == '\\' && close + 1 < end) close += 2;
                else close++;
            }
            if(close >= end || pattern[close] != ']') throw std::runtime_error("Unclosed [");
            if(i + 1 < end && pattern[i+1] == '^')
            {
                token = {TokenType::NegGroup, pattern.substr(i+2, close-(i+2))};
            } else {
                token = {TokenType::PosGroup, pattern.substr(i+1, close - (i+1))};
            }
            i = close + 1;
        }
        else if (pattern[i] == '.')
        {
            token = {TokenType::Dot,""};
            i++;
        }
        else if (pattern[i] == '(')
        {
            size_t j = i + 1;
            int depth = 0;
            bool in_brackets = false;
            bool closed = false;
            for(; j < end; ++j)
            {
                if(pattern[j] == '\\') { if(j+1 < end) ++ j; continue;}
                if(!in_brackets && pattern[j] == '[') { in_brackets = true; continue;}
                if(in_brackets && pattern[j] == ']') { in_brackets = false; continue;}
                if(in_brackets) continue;
                if(pattern[j] == '(') {++depth; continue;}
                if(pattern[j] == ')')
                {
                    if(depth == 0) {closed = true; break;}
                    --depth;
                }
            }
            if(!closed) throw std::runtime_error("Unclosed (");

            size_t s = i+1, e=j;
            std::vector<std::pair<size_t,size_t>> segments;
            size_t seg_start = s;
            int seg_depth = 0;
            bool seg_in_brackets = false;
            for(size_t p =s; p < e; ++p)
            {
                if (pattern[p] == '\\') { if (p+1 < e) ++p; continue; }
                if (!seg_in_brackets && pattern[p] == '[') { seg_in_brackets = true; continue; }
                if (seg_in_brackets && pattern[p] == ']') { seg_in_brackets = false; continue; }
                if (seg_in_brackets) continue;
                if (pattern[p] == '(') { ++seg_depth; continue; }
                if (pattern[p] == ')') { --seg_depth; continue; }
                if (pattern[p] == '|' && seg_depth == 0) {
                    segments.emplace_back(seg_start, p);
                    seg_start = p + 1;
                }
            }
            segments.emplace_back(seg_start,e);

            Token groupTok;
            groupTok.type = TokenType::Group;
            for(auto &seg : segments) {
                groupTok.alternatives.push_back(parse_range(pattern,seg.first,seg.second));
            }
            token = std::move(groupTok);
            i = j + 1;
        }
        else
        {
            token = {TokenType::Literal, std::string(1,pattern[i])};
            i++;
        }
        if(i < end && pattern[i] == '+') { token.one_or_more = true; i++;}
        else if (i < end && pattern[i] == '?') { token.zero_or_one = true; i++;}

        out.push_back(std::move(token));
    }
    return out;
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
        case TokenType::Dot: return true;
        default: return false;
    }
}

static std::vector<size_t> match_one(const Token& token, const std::string& input, size_t pos);

// Recursive backtracking matcher for a sequence of tokens starting at seq[idx]
static bool match_seq(const std::vector<Token>& seq, size_t idx, const std::string& input, size_t pos)
{
    if (idx >= seq.size()) return true;
    const Token& t = seq[idx];

    if (t.one_or_more)
    {
        // Need at least one match of t, then any number
        std::set<size_t> reach;
        std::vector<size_t> first = match_one(t, input, pos);
        if (first.empty()) return false; // No initial match
        
        for (auto p : first) reach.insert(p);
        std::vector<size_t> frontier(first.begin(), first.end());
        
        while (!frontier.empty())
        {
            std::vector<size_t> next;
            for (auto p : frontier)
            {
                auto more = match_one(t, input, p);
                for (auto q : more) {
                    if (reach.insert(q).second) next.push_back(q);
                }
            }
            frontier.swap(next);
        }
        
        for (auto endpos : reach)
        {
            if (match_seq(seq, idx + 1, input, endpos)) return true;
        }
        return false;
    }
    else if (t.zero_or_one)
    {
        // Try skipping
        if (match_seq(seq, idx + 1, input, pos)) return true;
        // Try taking one
        auto ends = match_one(t, input, pos);
        for (auto e : ends) {
            if (match_seq(seq, idx + 1, input, e)) return true;
        }
        return false;
    }
    else
    {
        auto ends = match_one(t, input, pos);
        for (auto e : ends) {
            if (match_seq(seq, idx + 1, input, e)) return true;
        }
        return false;
    }
}

static std::vector<size_t> match_one(const Token& token, const std::string& input, size_t pos)
{
    std::vector<size_t> res;
    
    if (token.type == TokenType::Group)
    {
        for (const auto &alt : token.alternatives)
        {
            // For each alternative, try to match the whole alternative sequence starting at pos
            std::function<std::vector<size_t>(const std::vector<Token>&, size_t, size_t)> ends_after;
            ends_after = [&](const std::vector<Token>& s, size_t idx2, size_t ppos) -> std::vector<size_t> {
                if (idx2 >= s.size()) return std::vector<size_t>{ppos};
                
                const Token &tt = s[idx2];
                std::vector<size_t> outpos;
                
                if (tt.one_or_more)
                {
                    std::set<size_t> reach;
                    std::vector<size_t> first = match_one(tt, input, ppos);
                    if (first.empty()) return outpos; // No match
                    
                    for (auto p : first) reach.insert(p);
                    std::vector<size_t> frontier(first.begin(), first.end());
                    
                    while (!frontier.empty())
                    {
                        std::vector<size_t> next;
                        for (auto p : frontier)
                        {
                            auto more = match_one(tt, input, p);
                            for (auto q : more) {
                                if (reach.insert(q).second) next.push_back(q);
                            }
                        }
                        frontier.swap(next);
                    }
                    
                    for (auto after : reach)
                    {
                        auto later = ends_after(s, idx2+1, after);
                        outpos.insert(outpos.end(), later.begin(), later.end());
                    }
                }
                else if (tt.zero_or_one)
                {
                    // Try skipping
                    auto later = ends_after(s, idx2+1, ppos);
                    outpos.insert(outpos.end(), later.begin(), later.end());
                    
                    // Try taking one
                    auto one = match_one(tt, input, ppos);
                    for (auto p : one)
                    {
                        auto later = ends_after(s, idx2+1, p);
                        outpos.insert(outpos.end(), later.begin(), later.end());
                    }
                }
                else
                {
                    auto one = match_one(tt, input, ppos);
                    for (auto p : one)
                    {
                        auto later = ends_after(s, idx2+1, p);
                        outpos.insert(outpos.end(), later.begin(), later.end());
                    }
                }
                return outpos;
            };

            auto ends = ends_after(alt, 0, pos);
            for (auto e : ends) res.push_back(e);
        }
    }
    else if (token.type == TokenType::StartAnchor)
    {
        if (pos == 0) res.push_back(pos);
    }
    else if (token.type == TokenType::EndAnchor)
    {
        if (pos == input.size()) res.push_back(pos);
    }
    else
    {
        if (pos < input.size() && match_token(token, static_cast<unsigned char>(input[pos])))
        {
            res.push_back(pos + 1);
        }
    }
    
    // Remove duplicates
    std::sort(res.begin(), res.end());
    res.erase(std::unique(res.begin(), res.end()), res.end());
    return res;
}

bool match_at(const std::string& input, size_t pos, const std::vector<Token>& tokens)
{
    return match_seq(tokens, 0, input, pos);
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
