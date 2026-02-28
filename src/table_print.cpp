#include "table_print.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

static std::string repeat(char c, size_t n) {
    return std::string(n, c);
}

static void print_sep(const std::vector<size_t>& widths) {
    std::cout << "+";
    for (size_t w : widths) std::cout << repeat('-', w + 2) << "+";
    std::cout << "\n";
}

static void print_row(const std::vector<size_t>& widths,
                      const std::vector<std::string>& cols) {
    std::cout << "|";
    for (size_t i = 0; i < widths.size(); i++) {
        std::cout << " " << std::left << std::setw((int)widths[i]) << cols[i] << " |";
    }
    std::cout << "\n";
}

void print_table(const std::vector<std::string>& headers,
                 const std::vector<std::vector<std::string>>& rows) {
    if (headers.empty()) return;

    std::vector<size_t> widths(headers.size());
    for (size_t i = 0; i < headers.size(); i++) widths[i] = headers[i].size();

    for (const auto& r : rows) {
        for (size_t i = 0; i < headers.size() && i < r.size(); i++) {
            widths[i] = std::max(widths[i], r[i].size());
        }
    }

    print_sep(widths);
    print_row(widths, headers);
    print_sep(widths);

    for (const auto& r : rows) {
        std::vector<std::string> padded = r;
        padded.resize(headers.size());
        print_row(widths, padded);
    }

    print_sep(widths);
}

void print_table_live(const std::vector<std::string>& headers,
                      const std::vector<std::vector<std::string>>& rows,
                      size_t& previous_height) {
    if (headers.empty()) return;

    size_t table_height = 5 + rows.size();

    if (previous_height > 0) {
        std::cout << "\033[" << previous_height << "A";
    }

    std::vector<size_t> widths(headers.size());
    for (size_t i = 0; i < headers.size(); i++) widths[i] = headers[i].size();
    for (const auto& r : rows) {
        for (size_t i = 0; i < headers.size() && i < r.size(); i++)
            widths[i] = std::max(widths[i], r[i].size());
    }

    print_sep(widths);
    print_row(widths, headers);
    print_sep(widths);

    for (const auto& r : rows) {
        std::vector<std::string> padded = r;
        padded.resize(headers.size());
        print_row(widths, padded);
    }

    print_sep(widths);
    std::cout << std::flush;

    previous_height = table_height;
}