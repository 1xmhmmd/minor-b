// highlighter.h - AST-driven syntax highlighter (ANSI terminal + standalone HTML/CSS).
//
// Step 1 gives every token a default role from its token type only.
// Step 2 walks the AST and OVERRIDES roles that need syntactic context, e.g.
//   - function name in FuncDecl / CallExpr.callee  -> Function
//   - Identifier / VarDecl / Param names           -> Variable
//   - type keywords inside TypeSpec                -> Type
// Step 3 marks every token that has a lexical/syntax diagnostic as Error.
#pragma once
#include <string>
#include <vector>
#include "ast.h"
#include "token.h"

enum class Role { Default, Keyword, Type, Variable, Function, Literal, String, Operator, Comment, Error };

class Highlighter {
 public:
  Highlighter(const std::string& src, const std::vector<Token>& tokens, const Node* ast,
              const std::vector<Diagnostic>& diags);
  std::string ansi() const;
  std::string html(const std::string& title) const;
  const std::vector<Role>& roles() const { return roles_; }

 private:
  const std::string& src_;
  const std::vector<Token>& toks_;
  const std::vector<Diagnostic>& diags_;
  std::vector<Role> roles_;
  void walk(const Node* n);
};
