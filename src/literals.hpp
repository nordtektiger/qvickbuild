#ifndef LITERALS_HPP
#define LITERALS_HPP

#include <string>
#include <variant>
#include <vector>

class Globbing {
public:
  static std::vector<std::string> compute_paths(std::string);
};

// used in matching algorithms.
struct Wildcard {};
using StrComponent = std::variant<Wildcard, std::string>;

class Wildcards {
  friend class Globbing;

private:
  static std::vector<StrComponent> tokenize_components(std::string);
  // returns wildcard groups
  static std::vector<std::string> match_components(std::vector<StrComponent>,
                                                   std::string);
  // internal error type to communicate the failure of the function above.
  class __MatchFailure : std::exception {};

public:
  static std::vector<std::string> compute_replace(std::vector<std::string>,
                                                  std::string, std::string);
};

// dummy classes to specify failure.
class LiteralsAdjacentWildcards : std::exception {};
class LiteralsChunksLength : std::exception {};

#endif
