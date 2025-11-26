#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>


std::vector<std::string> split_lines(const std::string &text);
std::vector<std::string> wrap_line(const std::string &line, size_t maxWidth);
std::vector<std::string> wrap_paragraph(const std::vector<std::string> &lines, size_t width);
std::string merge_columns(
        const std::string &A,
        const std::string &B,
        size_t colWidth,
        const std::string &separator);
std::string expand_tabs(const std::string &line);