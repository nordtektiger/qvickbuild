#ifndef TOKEN_TYPES_HPP
#define TOKEN_TYPES_HPP

#include "tracking.hpp"

#include <string>
#include <vector>
#include <optional>
#include <variant>

// token context indexes.
#define CTX_STR 0
#define CTX_VEC 1


// defines what type of token it is.
enum class TokenType {
  Identifier,       // e.g. variable names
  Literal,          // pure strings
  FormattedLiteral, // formatted strings
  Equals,           // `=`
  Modify,           // `:`
  LineStop,         // ';`
  Arrow,            // `->`
  IterateAs,        // `as`
  Separator,        // ','
  ExpressionOpen,   // `[`
  ExpressionClose,  // `]`
  TaskOpen,         // `{`
  TaskClose,        // `}`
  True,             // `true`
  False,            // `false`
};

struct Token;
using TokenContext =
    std::optional<std::variant<std::string, std::vector<Token>>>;

// defines a general token.
struct Token {
  TokenType type;
  TokenContext context;
  StreamReference reference;
};

#endif
