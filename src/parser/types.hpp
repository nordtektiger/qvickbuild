#ifndef PARSER_TYPES_HPP
#define PARSER_TYPES_HPP

#include "../lexer/types.hpp"
#include <map>
#include <memory>

struct Identifier;
struct Literal;
struct FormattedLiteral;
struct List;
struct Boolean;
struct Replace;
using ASTObject =
    std::variant<Identifier, Literal, FormattedLiteral, List, Boolean, Replace>;

// Logic: Expressions
struct Identifier {
  std::string content;
  StreamReference reference;
  bool operator==(Identifier const &other) const;
  // Identifier() = delete;
};
struct Literal {
  std::string content;
  StreamReference reference;
  bool operator==(Literal const &other) const;
  // Literal() = delete;
};
struct FormattedLiteral {
  std::vector<ASTObject> contents;
  StreamReference reference;
  bool operator==(FormattedLiteral const &other) const;
  // FormattedLiteral() = delete;
};
struct Boolean {
  bool content;
  StreamReference reference;
  bool operator==(Boolean const &other) const;
  // Boolean() = delete;
};
struct List {
  std::vector<ASTObject> contents;
  StreamReference reference;
  bool operator==(List const &other) const;
  // List() = delete;
};
struct Replace {
  std::shared_ptr<ASTObject> input;
  std::shared_ptr<ASTObject> filter;
  std::shared_ptr<ASTObject> product;
  StreamReference reference;
  bool operator==(Replace const &other) const;
  // Replace() = delete;
};

// config: fields, tasks, ast
struct Field {
  Identifier identifier;
  ASTObject expression;
  StreamReference reference;
  bool operator==(Field const &other) const;
  // Field() = delete;
};
struct Task {
  ASTObject identifier;
  Identifier iterator;
  std::map<std::string, Field> fields;
  // std::vector<Field> fields;
  StreamReference reference;
  bool operator==(Task const &other) const;
  // Task() = delete;
};
struct AST {
  std::map<std::string, Field> fields;
  // tasks need to be precomputed before being stored in a tree.
  std::vector<Task> tasks;
  std::optional<Task> topmost_task;
  // make the copy constructor explicit to emphasize performance.
  explicit AST(AST const &) = default;
  AST() = default;
};

#endif
