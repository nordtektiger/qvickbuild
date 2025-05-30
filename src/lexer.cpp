#include "lexer.hpp"
#include "errors.hpp"
#include <functional>
#include <variant>

// used for determining e.g. variable names.
inline bool is_alphabetic(char x) {
  return ((x >= 'A') && (x <= 'Z')) || ((x >= 'a') && (x <= 'z')) || x == '_' ||
         x == '-' || (x >= '0' && x <= '9');
}

// initializes new lexer.
Lexer::Lexer(std::vector<unsigned char> input_bytes) {
  m_index = 0;
  m_input = input_bytes;
  m_current = (m_input.size() >= m_index + 1) ? m_input[m_index] : '\0';
  m_next = (m_input.size() >= m_index + 2) ? m_input[m_index + 1] : '\0';
}

unsigned char Lexer::consume_byte() { return consume_byte(1); }

unsigned char Lexer::consume_byte(int n) {
  m_index += n;
    m_current = (m_input.size() >= m_index + 1) ? m_input[m_index] : '\0';
    m_next = (m_input.size() >= m_index + 2) ? m_input[m_index + 1] : '\0';
  return m_current;
}

// gets next token from stream.
std::vector<Token> Lexer::get_token_stream() {
  while (m_current != '\0') {
    bool match = false;
    for (const auto &fn : matching_rules) {
      std::optional<Token> token = fn();
      if (token) {
        m_token_stream.push_back(*token);
        match = true;
        break;
      }
    }
    if (m_current == '\0')
      break;
    if (!match) {
      ErrorHandler::halt(EInvalidSymbol{{m_index, 1}, std::to_string(m_current)});
    }
  }
  return m_token_stream;
}

// skip all whitespace characters and comments.
std::optional<Token> Lexer::skip_whitespace_comments() {
  while (m_current == ' ' || m_current == '\n' || m_current == '\t' ||
         m_current == '#') {
    while (m_current == ' ' || m_current == '\n' || m_current == '\t')
      consume_byte();
    if (m_current != '#')
      return std::nullopt; // not a comment.
    while (m_current != '\n') {
      consume_byte(); // currently in a comment.
    }
  }
  return std::nullopt;
}

// match =
std::optional<Token> Lexer::match_equals() {
  if (m_current != '=')
    return std::nullopt;
  consume_byte();
  return Token{TokenType::Equals, std::nullopt, {m_index - 1, 1}};
}

// match :
std::optional<Token> Lexer::match_modify() {
  if (m_current != ':')
    return std::nullopt;
  consume_byte();
  return Token{TokenType::Modify, std::nullopt, {m_index - 1, 1}};
}

// match ;
std::optional<Token> Lexer::match_linestop() {
  if (m_current != ';')
    return std::nullopt;
  consume_byte();
  return Token{TokenType::LineStop, std::nullopt, {m_index - 1, 1}};
}

// match ->
std::optional<Token> Lexer::match_arrow() {
  if (!(m_current == '-' && m_next == '>'))
    return std::nullopt;
  consume_byte(2);
  return Token{TokenType::Arrow, std::nullopt, {m_index - 2, 2}};
}

// match ,
std::optional<Token> Lexer::match_separator() {
  if (m_current != ',')
    return std::nullopt;
  consume_byte();
  return Token{TokenType::Separator, std::nullopt, {m_index - 1, 1}};
}

// match [
std::optional<Token> Lexer::match_expressionopen() {
  if (m_current != '[')
    return std::nullopt;
  consume_byte();
  return Token{TokenType::ExpressionOpen, std::nullopt, {m_index - 1, 1}};
}

// match ]
std::optional<Token> Lexer::match_expressionclose() {
  if (m_current != ']')
    return std::nullopt;
  consume_byte();
  return Token{TokenType::ExpressionClose, std::nullopt, {m_index - 1, 1}};
}

// match {
std::optional<Token> Lexer::match_taskopen() {
  if (m_current != '{')
    return std::nullopt;
  consume_byte();
  return Token{TokenType::TaskOpen, std::nullopt, {m_index - 1, 1}};
}

// match }
std::optional<Token> Lexer::match_taskclose() {
  if (m_current != '}')
    return std::nullopt;
  consume_byte();
  return Token{TokenType::TaskClose, std::nullopt, {m_index - 1, 1}};
}

// match literals
std::optional<Token> Lexer::match_literal() {
  if (m_current != '\"')
    return std::nullopt;
  size_t origin = m_index;
  consume_byte();

  std::vector<Token> internal_stream;
  std::string substr;
  while (m_current != '\"') {
    if (m_current == '[') {
      internal_stream.push_back(Token{TokenType::Literal, substr, {m_index - substr.size(), substr.size()}});
      consume_byte(); // consume the `[`.
      substr = "";
      // lex escaped expression.
      while (m_current != ']') {

        std::optional<Token> inner_token;
        skip_whitespace_comments();
        // note: the parser only supports escaped identifiers.
        if ((inner_token = match_modify())) {
        } else if ((inner_token = match_arrow())) {
        } else if ((inner_token = match_separator())) {
        } else if ((inner_token = match_identifier())) {
        }

        if (!inner_token)
          ErrorHandler::halt(EInvalidLiteral{{m_index, m_index - origin}});

        internal_stream.push_back(*inner_token);
      }
      consume_byte(); // consume the `]`
      continue;
    }

    // lex "pure" literal:
    substr += m_current;
    consume_byte();
  }
  internal_stream.push_back(Token{TokenType::Literal, substr, {m_index - substr.size(), substr.size()}});
  consume_byte(); // consume the `"`
  return Token {
    TokenType::FormattedLiteral,
    internal_stream,
    {origin, m_index - origin},
  };
}

// match identifiers
std::optional<Token> Lexer::match_identifier() {
  if (!is_alphabetic(m_current))
    return std::nullopt;
  std::string identifier;
  while (is_alphabetic(m_current)) {
    identifier += m_current;
    consume_byte();
  }

  if (identifier == "as")
    return Token{TokenType::IterateAs, std::nullopt, {m_index - 2, 2}};
  else if (identifier == "true")
    return Token{TokenType::True, std::nullopt, {m_index - 4, 4}};
  else if (identifier == "false")
    return Token{TokenType::False, std::nullopt, {m_index - 5, 5}};
  else
    return Token{TokenType::Identifier, identifier, {m_index - identifier.size(), identifier.size()}};
}
