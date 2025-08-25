#include <iostream>
#include <string>
#include <cctype>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>
#include <functional>
#include <map>
#include <fstream>

enum class TokenType {
    StartAnchor,
    EndAnchor,
    Literal,
    Digit,
    Word,
    PosGroup,
    NegGroup,
    Dot,
    Group,
    BackReference
};

struct Token {
    TokenType type;
    std::string value;
    bool one_or_more = false;
    bool zero_or_one = false;
    std::vector<std::vector<Token>> alternatives;
    int group_id = 0;
    int backref_id = 0;
};

struct MatchState {
    std::map<int, std::string> captures;
    int next_group_id = 1;
};

static std::vector<Token> parse_range(const std::string& pattern, size_t begin, size_t end, int& group_counter);

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

    int group_counter = 1;
    auto middle = parse_range(pattern, i, end, group_counter);
    tokens.insert(tokens.end(), middle.begin(), middle.end());

    if(has_end_anchor)
    {
        tokens.push_back({TokenType::EndAnchor,""});
    }
    return tokens;
}

static std::vector<Token> parse_range(const std::string& pattern, size_t begin, size_t end, int& group_counter)
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
            else if(std::isdigit(c)) {
                token = {TokenType::BackReference, ""};
                token.backref_id = c - '0';
                i += 2;
            }
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
            groupTok.group_id = group_counter++;
            for(auto &seg : segments) {
                groupTok.alternatives.push_back(parse_range(pattern,seg.first,seg.second, group_counter));
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

static std::vector<std::pair<size_t, MatchState>> match_one(const Token& token, const std::string& input, size_t pos, const MatchState& state);

static bool match_seq(const std::vector<Token>& seq, size_t idx, const std::string& input, size_t pos, MatchState& state)
{
    if (idx >= seq.size()) return true;
    const Token& t = seq[idx];

    if (t.one_or_more)
    {
        std::set<size_t> reach;
        std::vector<std::pair<size_t, MatchState>> first = match_one(t, input, pos, state);
        if (first.empty()) return false;
        
        std::vector<std::pair<size_t, MatchState>> all_positions = first;
        for (auto& p : first) reach.insert(p.first);
        std::vector<std::pair<size_t, MatchState>> frontier(first.begin(), first.end());
        
        while (!frontier.empty())
        {
            std::vector<std::pair<size_t, MatchState>> next;
            for (auto& p : frontier)
            {
                auto more = match_one(t, input, p.first, p.second);
                for (auto& q : more) {
                    if (reach.insert(q.first).second) {
                        next.push_back(q);
                        all_positions.push_back(q);
                    }
                }
            }
            frontier.swap(next);
        }
        
        for (auto& endpair : all_positions)
        {
            MatchState temp_state = endpair.second;
            if (match_seq(seq, idx + 1, input, endpair.first, temp_state)) {
                state = temp_state;
                return true;
            }
        }
        return false;
    }
    else if (t.zero_or_one)
    {
        MatchState temp_state = state;
        if (match_seq(seq, idx + 1, input, pos, temp_state)) {
            state = temp_state;
            return true;
        }
        auto ends = match_one(t, input, pos, state);
        for (auto& e : ends) {
            temp_state = e.second;
            if (match_seq(seq, idx + 1, input, e.first, temp_state)) {
                state = temp_state;
                return true;
            }
        }
        return false;
    }
    else
    {
        auto ends = match_one(t, input, pos, state);
        for (auto& e : ends) {
            MatchState temp_state = e.second;
            if (match_seq(seq, idx + 1, input, e.first, temp_state)) {
                state = temp_state;
                return true;
            }
        }
        return false;
    }
}

static std::vector<std::pair<size_t, MatchState>> match_one(const Token& token, const std::string& input, size_t pos, const MatchState& state)
{
    std::vector<std::pair<size_t, MatchState>> res;
    
    if (token.type == TokenType::Group)
    {
        for (const auto &alt : token.alternatives)
        {
            std::function<std::vector<std::pair<size_t, MatchState>>(const std::vector<Token>&, size_t, size_t, const MatchState&)> ends_after;
            ends_after = [&](const std::vector<Token>& s, size_t idx2, size_t ppos, const MatchState& current_state) -> std::vector<std::pair<size_t, MatchState>> {
                if (idx2 >= s.size()) return std::vector<std::pair<size_t, MatchState>>{{ppos, current_state}};
                
                const Token &tt = s[idx2];
                std::vector<std::pair<size_t, MatchState>> outpos;
                
                if (tt.one_or_more)
                {
                    std::set<size_t> reach;
                    std::vector<std::pair<size_t, MatchState>> first = match_one(tt, input, ppos, current_state);
                    if (first.empty()) return outpos;
                    
                    std::vector<std::pair<size_t, MatchState>> all_positions = first;
                    for (auto& p : first) reach.insert(p.first);
                    std::vector<std::pair<size_t, MatchState>> frontier(first.begin(), first.end());
                    
                    while (!frontier.empty())
                    {
                        std::vector<std::pair<size_t, MatchState>> next;
                        for (auto& p : frontier)
                        {
                            auto more = match_one(tt, input, p.first, p.second);
                            for (auto& q : more) {
                                if (reach.insert(q.first).second) {
                                    next.push_back(q);
                                    all_positions.push_back(q);
                                }
                            }
                        }
                        frontier.swap(next);
                    }
                    
                    for (auto& after : all_positions)
                    {
                        auto later = ends_after(s, idx2+1, after.first, after.second);
                        outpos.insert(outpos.end(), later.begin(), later.end());
                    }
                }
                else if (tt.zero_or_one)
                {
                    auto later = ends_after(s, idx2+1, ppos, current_state);
                    outpos.insert(outpos.end(), later.begin(), later.end());
                    
                    auto one = match_one(tt, input, ppos, current_state);
                    for (auto& p : one)
                    {
                        auto later = ends_after(s, idx2+1, p.first, p.second);
                        outpos.insert(outpos.end(), later.begin(), later.end());
                    }
                }
                else
                {
                    auto one = match_one(tt, input, ppos, current_state);
                    for (auto& p : one)
                    {
                        auto later = ends_after(s, idx2+1, p.first, p.second);
                        outpos.insert(outpos.end(), later.begin(), later.end());
                    }
                }
                return outpos;
            };

            auto ends = ends_after(alt, 0, pos, state);
            for (auto& e : ends) {
                MatchState new_state = e.second;
                std::string captured = input.substr(pos, e.first - pos);
                new_state.captures[token.group_id] = captured;
                res.push_back({e.first, new_state});
            }
        }
    }
    else if (token.type == TokenType::BackReference)
    {
        auto it = state.captures.find(token.backref_id);
        if (it != state.captures.end())
        {
            const std::string& captured = it->second;
            if (pos + captured.length() <= input.size() && 
                input.substr(pos, captured.length()) == captured)
            {
                res.push_back({pos + captured.length(), state});
            }
        }
    }
    else if (token.type == TokenType::StartAnchor)
    {
        if (pos == 0) res.push_back({pos, state});
    }
    else if (token.type == TokenType::EndAnchor)
    {
        if (pos == input.size()) res.push_back({pos, state});
    }
    else
    {
        if (pos < input.size() && match_token(token, static_cast<unsigned char>(input[pos])))
        {
            res.push_back({pos + 1, state});
        }
    }
    
    return res;
}

bool match_at(const std::string& input, size_t pos, const std::vector<Token>& tokens)
{
    MatchState state;
    return match_seq(tokens, 0, input, pos, state);
}

bool match_pattern(const std::string& input_line, const std::string& pattern) {
    auto tokens = tokenize(pattern);
    bool anchored_start = !tokens.empty() && tokens[0].type == TokenType::StartAnchor;
    bool anchored_end = !tokens.empty() && tokens.back().type == TokenType::EndAnchor;
    if (anchored_start && anchored_end) {
        return match_at(input_line, 0, tokens);
    } else if (anchored_start) {
        return match_at(input_line, 0, tokens);
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

    if (argc < 3) {
        std::cerr << "Expected at least two arguments" << std::endl;
        return 1;
    }

    std::string flag = argv[1];
    std::string pattern = argv[2];

    if (flag != "-E") {
        std::cerr << "Expected first argument to be '-E'" << std::endl;
        return 1;
    }

    bool found_match = false;

    if(argc >= 4) {
        // File input mode (single or multiple files)
        bool multiple_files = (argc > 4);
        
        for(int file_idx = 3; file_idx < argc; file_idx++) {
            std::string filename = argv[file_idx];
            std::ifstream file(filename);
            if(!file.is_open()) {
                std::cerr << "Could not open file: " << filename << std::endl;
                return 1;
            }

            std::string line;
            while(std::getline(file, line)) {
                try {
                    if (match_pattern(line, pattern)) {
                        if(multiple_files) {
                            std::cout << filename << ":" << line << std::endl;
                        } else {
                            std::cout << line << std::endl;
                        }
                        found_match = true;
                    }
                } catch (const std::runtime_error& e) {
                    std::cerr << e.what() << std::endl;
                    return 1;
                }
            }
            file.close();
        }
    } else {
        // Stdin input mode
        std::string input_line;
        std::getline(std::cin, input_line);
        try {
            if (match_pattern(input_line, pattern)) {
                std::cout << input_line << std::endl;
                found_match = true;
            }
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
            return 1;
        }
    }
    
    return found_match ? 0 : 1;
}