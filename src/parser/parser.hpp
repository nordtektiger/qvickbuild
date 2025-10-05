#ifndef PARSER_H
#define PARSER_H

#include "../lexer/types.hpp"
#include "types.hpp"

#include <vector>

// visitor that simply returns the origin of an AST object.
struct ASTVisitReference {
  StreamReference operator()(Identifier const &identifier) {
    return identifier.reference;
  }
  StreamReference operator()(Literal const &literal) {
    return literal.reference;
  }
  StreamReference operator()(FormattedLiteral const &formatted_literal) {
    return formatted_literal.reference;
  }
  StreamReference operator()(List const &list) { return list.reference; }
  StreamReference operator()(Boolean const &boolean) {
    return boolean.reference;
  }
  StreamReference operator()(Replace const &replace) {
    return replace.reference;
  }
};

class Parser {
private:
  std::vector<Token> m_token_stream;
  AST m_ast;

  size_t m_index;
  std::optional<Token> m_current;
  std::optional<Token> m_next;

  std::optional<Token> consume_token();
  std::optional<Token> consume_token(int n);
  std::optional<Token> consume_if(TokenType token_type);
  bool check_current(TokenType token_type);
  bool check_next(TokenType token_type);

  std::optional<ASTObject> parse_ast_object();
  std::optional<ASTObject> parse_list();
  std::optional<ASTObject> parse_replace();
  std::optional<ASTObject> parse_primary();
  std::optional<Field> parse_field();
  std::optional<Task> parse_task();

public:
  Parser(std::vector<Token> token_stream);
  AST parse_tokens();
};

#endif
