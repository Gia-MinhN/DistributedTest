#pragma once

#include <string>
#include <vector>

void print_table(const std::vector<std::string>& headers,
                 const std::vector<std::vector<std::string>>& rows);

void print_table_live(const std::vector<std::string>& headers,
                      const std::vector<std::vector<std::string>>& rows,
                      size_t& previous_height);