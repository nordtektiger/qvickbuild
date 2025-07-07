#ifndef LEXER_H
#define LEXER_H

// token context indexes.
#define CTX_STR 0
#define CTX_VEC 1

// macro madness - this just generates the appropriate function signatures,
// along with a vector of lambda functions calling every rule.
// [note]: This does __not__ automatically match tokens inside of formatted
// strings.
#define LEXING_RULES(_MACRO)                                                   \
  _MACRO(skip_whitespace_comments)                                             \
  _MACRO(match_equals)                                                         \
  _MACRO(match_modify)                                                         \
  _MACRO(match_linestop)                                                       \
  _MACRO(match_arrow)                                                          \
  _MACRO(match_separator)                                                      \
  _MACRO(match_expressionopen)                                                 \
  _MACRO(match_expressionclose)                                                \
  _MACRO(match_taskopen)                                                       \
  _MACRO(match_taskclose)                                                      \
  _MACRO(match_literal)                                                        \
  _MACRO(match_identifier)

#define _FUNCTION_DECLARE(x) std::optional<Token> x();
#define FUNCTION_DECLARE_ALL LEXING_RULES(_FUNCTION_DECLARE)

#define _LAMBDA_DECLARE(x) [this]() { return x(); }
#define _LAMBDA_DECLARE_LIST(x) _LAMBDA_DECLARE(x),
#define LAMBDA_DECLARE_ALL LEXING_RULES(_LAMBDA_DECLARE_LIST)

#include "tracking.hpp"
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

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

// small struct for tracking the origin of symbols.
struct InputStreamPos {
  size_t index;       // ASCII stream origin
  size_t line;        // Line number
  /* size_t length */ // Length of symbol for e.g. highlighting
  bool operator==(InputStreamPos const &other) const {
    return this->index == other.index && this->line == other.line;
  }
};
// struct InternalNode {};
// using ObjectReference = std::string;
// using Origin = std::variant<InputStreamPos, ObjectReference, InternalNode>;

struct Token;
using TokenContext =
    std::optional<std::variant<std::string, std::vector<Token>>>;

// defines a general token.
struct Token {
  TokenType type;
  TokenContext context;
  StreamReference reference;
};

// work class.
class Lexer {
private:
  std::vector<unsigned char> m_input;
  std::vector<Token> m_token_stream;

  unsigned char m_current;
  unsigned char m_next;
  size_t m_index;

  unsigned char consume_byte();
  unsigned char consume_byte(int n);

  // here's the crazy macro magic.
  FUNCTION_DECLARE_ALL
  std::vector<std::function<std::optional<Token>(void)>> matching_rules{
      LAMBDA_DECLARE_ALL};

  std::optional<std::vector<Token>> parse_escaped_literal();
  std::optional<unsigned char> parse_escaped_symbol();


public:
  Lexer(std::vector<unsigned char> input_bytes);
  std::vector<Token> get_token_stream();
};

#endif
