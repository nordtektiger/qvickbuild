#ifndef LEXER_H
#define LEXER_H

/*!
 * macro madness - this just generates the appropriate function signatures,
 * along with a vector of lambda functions calling every rule.
 * note: This does __not__ automatically match tokens inside of formatted
 * strings.
 */
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

#include "types.hpp"
#include <functional>
#include <optional>
#include <vector>

/*!
 * lexes a configuration source.
 */
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
  /*!
   * initialises the lexer with a configuration source.
   * \param input_bytes configuration source
   */
  Lexer(std::vector<unsigned char> input_bytes);
  /*!
   * runs the lexer and produces a token stream.
   * \return token stream in the form of a std::vector<Token>
   */
  std::vector<Token> get_token_stream();
};

#endif
