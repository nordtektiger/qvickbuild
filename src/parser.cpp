#include "parser.hpp"
#include "errors.hpp"
#include "lexer.hpp"
#include <iostream>
#include <memory>

// equality operators for AST objects.
bool Identifier::operator==(Identifier const &other) const {
  // return this->content == other.content && this->origin == other.origin;
  return this->content == other.content;
}
bool Literal::operator==(Literal const &other) const {
  return this->content == other.content;
}
bool FormattedLiteral::operator==(FormattedLiteral const &other) const {
  return this->contents == other.contents;
}
bool Boolean::operator==(Boolean const &other) const {
  return this->content == other.content;
}
bool Replace::operator==(Replace const &other) const {
  return this->identifier == other.identifier &&
         *(this->original) == *(other.original) &&
         *(this->replacement) == *(other.replacement);
}
bool List::operator==(List const &other) const {
  return this->contents == other.contents;
}
bool Field::operator==(Field const &other) const {
  return this->identifier == other.identifier &&
         this->expression == other.expression;
}
bool Task::operator==(Task const &other) const {
  return this->identifier == other.identifier &&
         this->iterator == other.iterator && this->fields == other.fields;
}

// initialises fields.
Parser::Parser(std::vector<Token> token_stream) : m_ast() {
  m_token_stream = token_stream;
  m_index = 0;
  if (m_token_stream.size() >= m_index + 1)
    m_current = m_token_stream[m_index];
  else
    m_current = std::nullopt;
  if (m_token_stream.size() >= m_index + 2)
    m_next = m_token_stream[m_index + 1];
  else
    m_next = std::nullopt;
}

// token checking, no side effects.
bool Parser::check_current(TokenType token_type) {
  return m_current && m_current->type == token_type;
}

// token checking, no side effects.
bool Parser::check_next(TokenType token_type) {
  return m_next && m_next->type == token_type;
}

// consume a token.
std::optional<Token> Parser::consume_token() { return consume_token(1); }

// consume n tokens.
std::optional<Token> Parser::consume_token(int n) {
  m_index += n;
  std::optional<Token> m_previous = m_current;
  if (m_token_stream.size() >= m_index + 1)
    m_current = m_token_stream[m_index];
  else
    m_current = std::nullopt;
  if (m_token_stream.size() >= m_index + 2)
    m_next = m_token_stream[m_index + 1];
  else
    m_next = std::nullopt;
  return m_previous;
}

// consume a token if the type matches.
std::optional<Token> Parser::consume_if(TokenType token_type) {
  if (check_current(token_type))
    return consume_token();
  return std::nullopt;
}

// parses the entire token stream.
AST Parser::parse_tokens() {
  AST ast;
  while (m_current) {
    std::optional<Field> field = parse_field();
    if (field) {
      ast.fields.push_back(*field);
      continue;
    }
    std::optional<Task> task = parse_task();
    if (task) {
      ast.tasks.push_back(*task);
      continue;
    }
    // ErrorHandler::halt(EInvalidGrammar{m_current->reference});
  }
  return AST(ast);
}

// attempts to parse a field.
std::optional<Field> Parser::parse_field() {
  if (!check_current(TokenType::Identifier) || !check_next(TokenType::Equals))
    return std::nullopt;

  Token identifier_token = *consume_token();
  Identifier identifier = Identifier{
      std::get<CTX_STR>(*identifier_token.context), identifier_token.reference};
  StreamReference reference = identifier_token.reference;
  consume_token(); // consume the `=`.

  std::optional<ASTObject> ast_object = parse_ast_object();
  // if (!ast_object)
  //   ErrorHandler::halt(ENoValue{identifier});

  ASTObject expression = *ast_object;
  if (!consume_if(TokenType::LineStop)) {}
    // ErrorHandler::halt(
    //     ENoLinestop{m_current->reference}); // todo: not guaranteed?
  return Field{identifier, expression, reference};
}

// attempts to parse a task.
std::optional<Task> Parser::parse_task() {
  std::optional<ASTObject> identifier = parse_ast_object();
  if (!identifier)
    return std::nullopt;
  StreamReference reference = std::visit(ASTVisitReference{}, *identifier);
  Identifier iterator = Identifier{"__task__", reference};
  // check if an explicit iterator name has been declared.
  if (consume_if(TokenType::IterateAs)) {
    std::optional<Token> iterator_token = consume_if(TokenType::Identifier);
    //   ErrorHandler::push_error_throw(origin, P_TASK_NO_ITERATOR);
    iterator = Identifier{std::get<CTX_STR>(*iterator_token->context),
                          iterator_token->reference};
  }
  if (!consume_if(TokenType::TaskOpen)) {
  }
  //   ErrorHandler::push_error_throw(origin, P_TASK_NO_OPEN);

  std::optional<Field> field;
  std::vector<Field> fields;
  while ((field = parse_field()))
    fields.push_back(*field);

  if (!consume_if(TokenType::TaskClose)) {
  }
  //   ErrorHandler::push_error_throw(origin, P_TASK_NO_CLOSE);

  return Task{*identifier, iterator, fields, reference};
}

/*
 * grammar:
 * ========
 * ASTOBJ -> LIST
 * LIST -> REPLACE ("," REPLACE)*
 * REPLACE -> (PRIMARY ":" PRIMARY "->" PRIMARY) | PRIMARY
 * PRIMARY -> token_literal | token_formattedliteral | token_identifier |
 *            token_true | token_false | ("[" ASTOBJ "]")
 */

std::optional<ASTObject> Parser::parse_ast_object() { return parse_list(); }

// recursive descent parser, see grammar.
std::optional<ASTObject> Parser::parse_list() {
  std::optional<ASTObject> ast_obj;
  ast_obj = parse_replace();
  std::vector<ASTObject> contents;
  while (ast_obj && consume_if(TokenType::Separator)) {
    contents.push_back(*ast_obj);
    ast_obj = parse_ast_object();
  }

  if (!ast_obj && !contents.empty()) {
  }
  //   ErrorHandler::push_error_throw(
  //       std::visit(ASTVisitOrigin{}, list.contents[0]), P_AST_INVALID_END);
  else if (!ast_obj && contents.empty())
    return std::nullopt;

  contents.push_back(*ast_obj);

  if (contents.size() == 1)
    return contents[0];
  StreamReference reference = std::visit(ASTVisitReference{}, contents[0]);

  return List{contents, reference};
}

// recursive descent parser, see grammar.
std::optional<ASTObject> Parser::parse_replace() {
  std::optional<ASTObject> identifier = parse_primary();
  if (!consume_if(TokenType::Modify))
    return identifier; // not a replace.
  StreamReference reference = std::visit(ASTVisitReference{}, *identifier);

  std::optional<ASTObject> original = parse_primary();
  if (!consume_if(TokenType::Arrow)) {
  }
  //   ErrorHandler::push_error_throw(origin, P_AST_NO_ARROW);

  std::optional<ASTObject> replacement = parse_primary();
  // if (!identifier || !original || !replacement)
  //   ErrorHandler::push_error_throw(origin, P_AST_INVALID_REPLACE);

  return Replace{
      std::make_shared<ASTObject>(*identifier),
      std::make_shared<ASTObject>(*original),
      std::make_shared<ASTObject>(*replacement),
      reference,
  };
}

// recursive descent parser, see grammar.
std::optional<ASTObject> Parser::parse_primary() {
  std::optional<Token> token;
  if ((token = consume_if(TokenType::Literal)))
    return Literal{std::get<CTX_STR>(*token->context), token->reference};
  else if ((token = consume_if(TokenType::Identifier)))
    return Identifier{std::get<CTX_STR>(*token->context), token->reference};
  else if ((token = consume_if(TokenType::True)))
    return Boolean{true, token->reference};
  else if ((token = consume_if(TokenType::False)))
    return Boolean{false, token->reference};
  else if ((token = consume_if(TokenType::FormattedLiteral))) {
    std::vector<ASTObject> contents;
    std::vector<Token> internal_token_stream =
        std::get<CTX_VEC>(*token->context);
    StreamReference reference = internal_token_stream[0].reference;
    // note: only identifiers and literals may be present.
    for (size_t i = 0; i < internal_token_stream.size(); i++) {
      Token internal_token = internal_token_stream[i];
      if (internal_token.type == TokenType::Literal)
        contents.push_back(Literal{std::get<CTX_STR>(*internal_token.context),
                                   internal_token.reference});
      else if (internal_token.type == TokenType::Identifier)
        contents.push_back(
            Identifier{std::get<CTX_STR>(*internal_token.context),
                       internal_token.reference});
      else {
      }
      // ErrorHandler::push_error_throw(internal_token.origin,
      //                                P_AST_INVALID_ESCAPE);
    }
    return FormattedLiteral{contents, reference};
  } else if ((token = consume_if(TokenType::ExpressionOpen))) {
    std::optional<ASTObject> ast_object = parse_ast_object();
    if (!consume_if(TokenType::ExpressionClose)) {
    }
    //   ErrorHandler::push_error_throw(std::visit(ASTVisitOrigin{},
    //   *ast_object),
    //                                  P_AST_NO_CLOSE);
    // if (!ast_object)
    //   ErrorHandler::push_error(token->origin, P_EMPTY_EXPRESSION);

    return ast_object;
  }

  return std::nullopt;
}
