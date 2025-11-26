
#include "view/pager.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

static constexpr size_t TAB_WIDTH = 4;


std::string expand_tabs(const std::string &line) {
    std::string out;
    out.reserve(line.size());

    size_t column = 0;

    for (char c : line) {
        if (c == '\t') {
            size_t spaces = TAB_WIDTH - (column % TAB_WIDTH);
            out.append(spaces, ' ');
            column += spaces;
        } else {
            out.push_back(c);
            column++;
        }
    }
    return out;
}


std::vector<std::string> split_lines(const std::string &text) {
    std::stringstream ss(text);
    std::string line;
    std::vector<std::string> lines;

    while (std::getline(ss, line)) {
        // Expand tabs for each physical line
        lines.push_back(expand_tabs(line));
    }
    return lines;
}


std::vector<std::string> wrap_line(const std::string &line, size_t maxWidth) {
    std::vector<std::string> wrapped;
    std::stringstream ss(line);
    std::string word;

    std::string current;

    while (ss >> word) {
        if (current.empty()) {
            if (word.size() > maxWidth) {
                wrapped.push_back(word.substr(0, maxWidth));
                size_t idx = maxWidth;
                while (idx < word.size()) {
                    wrapped.push_back(word.substr(idx, maxWidth));
                    idx += maxWidth;
                }
            } else {
                current = word;
            }
        } else {
            if (current.size() + 1 + word.size() > maxWidth) {
                wrapped.push_back(current);
                current = word;
            } else {
                current += " " + word;
            }
        }
    }

    if (!current.empty())
        wrapped.push_back(current);

    if (wrapped.empty())
        wrapped.push_back("");

    return wrapped;
}

std::vector<std::string> wrap_paragraph(
    const std::vector<std::string> &lines,
    size_t width
) {
    std::vector<std::string> out;
    for (auto &l : lines) {
        auto wrapped = wrap_line(l, width);
        out.insert(out.end(), wrapped.begin(), wrapped.end());
    }
    return out;
}

std::string merge_columns(
    const std::string &A,
    const std::string &B,
    size_t colWidth,
    const std::string &separator = " │ "
) {
    auto a_lines = wrap_paragraph(split_lines(A), colWidth);
    auto b_lines = wrap_paragraph(split_lines(B), colWidth);

    size_t count = std::max(a_lines.size(), b_lines.size());

    std::ostringstream out;

    for (size_t i = 0; i < count; i++) {
        std::string L = (i < a_lines.size() ? a_lines[i] : "");
        std::string R = (i < b_lines.size() ? b_lines[i] : "");

        L.resize(colWidth, ' ');
        R.resize(colWidth, ' ');

        out << L << separator << R;
        if (i + 1 < count) out << "\n";
    }

    return out.str();
}
