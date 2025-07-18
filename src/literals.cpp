#include "literals.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>

std::vector<std::string> Globbing::compute_paths(std::string literal) {
  std::vector<StrComponent> filter = Wildcards::tokenize_components(literal);
  std::vector<std::string> paths;
  for (std::filesystem::directory_entry const &dir_entry :
       std::filesystem::recursive_directory_iterator(".")) {
    try {
      Wildcards::match_components(filter, dir_entry.path().string());
      paths.push_back(dir_entry.path().string());
    } catch (Wildcards::__MatchFailure &_) {
      continue;
    }
  }
  return paths;
}

std::vector<StrComponent> Wildcards::tokenize_components(std::string in) {
  std::vector<StrComponent> parsed;
  std::string str_buf;

  for (size_t i = 0; i < in.size(); i++) {
    if (in[i] == '*') {
      if (!str_buf.empty()) {
        parsed.push_back(str_buf);
        str_buf = "";
      }
      parsed.push_back(Wildcard{});
    } else {
      str_buf += in[i];
    }
  }

  if (!str_buf.empty())
    parsed.push_back(str_buf);

  return parsed;
}

std::vector<std::string>
Wildcards::match_components(std::vector<StrComponent> filter, std::string in) {
  size_t i_str = 0; // index of input string
  bool matches = true;
  std::vector<std::string> output;

  // iterate over every component
  for (size_t i_comp = 0; i_comp < filter.size(); i_comp++) {
    StrComponent const &component = filter[i_comp];
    if (std::holds_alternative<std::string>(component)) {
      // --- match exact characters
      std::string str_component = std::get<std::string>(component);
      if (str_component.size() + i_str > in.size()) {
        // component is greater than input string
        matches = false;
        break;
      } else if (str_component != in.substr(i_str, str_component.size())) {
        // component simply doesn't match
        matches = false;
        break;
      } else if (i_comp == filter.size() - 1 &&
                 i_str + str_component.size() < in.size()) {
        // all components match, but there is input string left
        matches = false;
        break;
      };
      // std::cerr << i_str << " + " << str_component.size() << " < " <<
      // in.size() << std::endl;
      i_str += str_component.size();
    } else {
      // --- match wildcard
      if (i_comp >= filter.size() - 1) {
        // final asterisk: don't forget to save it as a reconstruction group.
        output.push_back(in.substr(i_str, in.size() - i_str));
        break;
      }
      // adjacent wildcards cannot be parsed.
      if (std::holds_alternative<Wildcard>(filter[i_comp + 1]))
        throw LiteralsAdjacentWildcards{};

      // index of input string where segment after asterisk starts
      bool seg_match = false;
      std::string str_component = std::get<std::string>(filter[i_comp + 1]);

      for (size_t i_seg = 0;
           i_seg < in.size() - i_str - str_component.size() + 1; i_seg++) {
        if (in.substr(i_str + i_seg, str_component.size()) == str_component) {
          if (i_comp == filter.size() - 2 && i_str + i_seg + str_component.size() < in.size())
            continue;
          seg_match = true;
          output.push_back(in.substr(i_str, i_seg));
          i_str += i_seg + str_component.size();
          break;
        }
        // std::cerr << i_str << " + " << i_seg << " + " << str_component.size() << " < " << in.size() << std::endl;
      }
      if (!seg_match) {
        matches = false;
        break;
      } else {
        // increment this twice because two components have been matched.
        i_comp++;
      }
    }
  }

  if (!matches)
    throw __MatchFailure();
  else
    return output;
}

std::vector<std::string>
Wildcards::compute_replace(std::vector<std::string> data, std::string filter,
                           std::string product) {
  std::vector<std::string> output;

  std::vector<StrComponent> filter_components = tokenize_components(filter);
  std::vector<StrComponent> product_components = tokenize_components(product);

  size_t wildcards_filter = std::count_if(
      filter_components.begin(), filter_components.end(),
      [](StrComponent s) { return std::holds_alternative<Wildcard>(s); });

  size_t wildcards_product = std::count_if(
      product_components.begin(), product_components.end(),
      [](StrComponent s) { return std::holds_alternative<Wildcard>(s); });

  if (wildcards_product > wildcards_filter)
    throw LiteralsChunksLength{};

  for (const std::string &element : data) {
    try {
      std::vector<std::string> wildcard_groups =
          match_components(filter_components, element);
      // weave wildcard with product strcomponents
      std::string final_str;
      for (const StrComponent &component : product_components) {
        if (std::holds_alternative<std::string>(component))
          final_str += std::get<std::string>(component);
        else { // wildcard
          final_str += *wildcard_groups.begin();
          wildcard_groups.erase(wildcard_groups.begin());
        }
      }
      output.push_back(final_str);
    } catch (__MatchFailure &_) {
      // did not match.
      output.push_back(element);
    }
  }
  return output;
}
