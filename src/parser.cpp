#include "parser.h"

namespace {
// Binary operator precedence levels, LOWEST first. Each level is one EBNF rule:
//   logical_or, logical_and, comparison, additive, multiplicative.
// All are left-associative and written with iteration (no left recursion).
const std::vector<std::vector<std::string>> kLevels = {
    {"||"}, {"&&"}, {"<", "<=", ">", ">=", "==", "!="}, {"+", "-"}, {"*", "/", "%"}};

// Spec (B-3): "\n" = newline, "\0" = NUL, backslash + any other char = that char.
std::string unescape(const std::string& body) {
  std::string out;
  for (size_t i = 0; i < body.size(); i++) {
    if (body[i] == '\\' && i + 1 < body.size()) {
      char e = body[++i];
      out += e == 'n' ? '\n' : e == '0' ? '\0' : e;
    } else {
      out += body[i];
    }
  }
  return out;
}
}  // namespace

Parser::Parser(const std::vector<Token>& tokens) : all_(tokens) {
  for (const Token& t : tokens)
    if (t.type != TokType::LineComment && t.type != TokType::BlockComment)
      sig_.push_back(t.index);  // INVALID tokens stay in the stream: the lexer already reported them
}

// ---------------------------------------------------------------- errors
std::string Parser::describe(const Token& t) const {
  return t.type == TokType::Eof ? "end of file" : "'" + t.lexeme + "'";
}

// A syntax error caused by (or right after) a token the lexer already rejected
// would only repeat the same mistake, so it is not reported again.
bool Parser::nearInvalid(size_t tokIdx) const {
  if (all_[tokIdx].type == TokType::Invalid) return true;
  for (size_t i = tokIdx; i-- > 0;) {
    TokType t = all_[i].type;
    if (t == TokType::Invalid) return true;
    if (t != TokType::LineComment && t != TokType::BlockComment) break;
  }
  return false;
}

void Parser::reportAt(size_t tokIdx, const std::string& msg) {
  if ((long)tokIdx == lastErr_) return;  // avoid cascades on the same token
  if (nearInvalid(tokIdx)) { lastErr_ = (long)tokIdx; return; }
  lastErr_ = (long)tokIdx;
  diags_.push_back(Diagnostic{"syntax", msg, all_[tokIdx].loc, (long)tokIdx});
}

void Parser::fail(const std::string& msg) {
  reportAt(curIdx(), msg);
  throw SyntaxError{};
}

void Parser::expectDelim(const char* s, const std::string& ctx) {
  if (isDelim(s)) { advance(); return; }
  fail(std::string("expected '") + s + "' " + ctx + ", got " + describe(cur()));
}

void Parser::expectDelimSoft(const char* s, const std::string& ctx) {
  if (isDelim(s)) { advance(); return; }
  reportAt(curIdx(), std::string("expected '") + s + "' " + ctx + ", got " + describe(cur()));
}

// Panic mode inside a block: skip to a synchronisation point.
//   ';' is consumed, '}' and statement keywords / "name :" are left for the caller.
void Parser::syncStatement() {
  while (!atEnd()) {
    if (isDelim(";")) { advance(); return; }
    if (isDelim("}")) return;
    if (isDelim("{")) { skipBraces(); return; }   // skip a whole nested block
    if (isKw("if") || isKw("for") || isKw("return") || isKw("print")) return;
    if (cur().type == TokType::Ident && nextIsColon()) return;
    advance();
  }
}

void Parser::skipBraces() {  // cur() is '{'
  int depth = 0;
  while (!atEnd()) {
    if (isDelim("{")) depth++;
    else if (isDelim("}")) { depth--; if (depth == 0) { advance(); return; } }
    advance();
  }
}

// Panic mode at top level: stop after a ';' or after the '}' that closes a body,
// or right before the next "name :" declaration.
void Parser::syncTopLevel(size_t start) {
  int depth = 0;
  while (!atEnd()) {
    if (isDelim("{")) { depth++; advance(); continue; }
    if (isDelim("}")) { advance(); if (depth > 0) depth--; if (depth == 0) return; continue; }
    if (depth == 0) {
      if (isDelim(";")) { advance(); return; }
      if (p_ != start && cur().type == TokType::Ident && nextIsColon()) return;
    }
    advance();
  }
}

// ---------------------------------------------------------------- declarations
std::unique_ptr<Program> Parser::parseProgram() {
  auto prog = std::make_unique<Program>(cur().loc);
  while (!atEnd()) {
    size_t start = p_;
    try {
      prog->decls.push_back(parseGlobalDecl());
    } catch (const SyntaxError&) {
      prog->decls.push_back(std::make_unique<ErrorNode>(all_[sig_[start]].loc));
      syncTopLevel(start);
    }
    if (p_ == start && !atEnd()) advance();  // guarantee progress
  }
  return prog;
}

// global_decl ::= IDENT ':' ( function_rest | var_rest )
NodePtr Parser::parseGlobalDecl() {
  if (cur().type != TokType::Ident)
    fail("expected a declaration of the form 'name : type', got " + describe(cur()));
  size_t nameTok = curIdx();
  std::string name = cur().lexeme;
  SrcLoc loc = cur().loc;
  advance();
  expectDelim(":", "after '" + name + "'");
  if (isKw("function")) return parseFunctionRest(nameTok, name, loc);
  return parseVarRest(nameTok, name, loc);
}

// function_rest ::= 'function' ret_type '(' [ param_list ] ')' ( '=' block | ';' )
NodePtr Parser::parseFunctionRest(size_t nameTok, const std::string& name, SrcLoc loc) {
  auto fn = std::make_unique<FuncDecl>(loc);
  fn->name = name;
  fn->nameTok = nameTok;
  advance();  // 'function'
  fn->retType = parseTypeSpec(true);
  expectDelim("(", "after the return type");
  if (!isDelim(")")) {
    fn->params.push_back(parseParam());
    while (isDelim(",")) { advance(); fn->params.push_back(parseParam()); }
  }
  expectDelimSoft(")", "after the parameter list");
  if (isOp("=")) {
    advance();
    fn->body = parseBlock();
  } else if (isDelim(";")) {
    advance();  // prototype
  } else {
    fail("expected '=' or ';' after the function signature, got " + describe(cur()));
  }
  return fn;
}

// param ::= IDENT ':' type_spec
std::unique_ptr<Param> Parser::parseParam() {
  if (cur().type != TokType::Ident) fail("expected a parameter name, got " + describe(cur()));
  auto prm = std::make_unique<Param>(cur().loc);
  prm->name = cur().lexeme;
  prm->nameTok = curIdx();
  advance();
  expectDelim(":", "after parameter '" + prm->name + "'");
  prm->paramType = parseTypeSpec(false);
  return prm;
}

// var_rest ::= type_spec [ '=' initializer ] ';'
NodePtr Parser::parseVarRest(size_t nameTok, const std::string& name, SrcLoc loc) {
  auto v = std::make_unique<VarDecl>(loc);
  v->name = name;
  v->nameTok = nameTok;
  v->declType = parseTypeSpec(false);
  if (isOp("=")) { advance(); v->init = parseInitializer(); }
  expectDelimSoft(";", "after declaration of '" + name + "'");
  return v;
}

// type_spec ::= 'integer' | 'boolean' | 'char' | 'string' | 'array' '[' [expr] ']' type_spec
// ret_type  ::= type_spec | 'void'
std::unique_ptr<TypeSpec> Parser::parseTypeSpec(bool allowVoid) {
  auto t = std::make_unique<TypeSpec>(cur().loc);
  t->tok = curIdx();
  if (cur().type == TokType::Keyword &&
      (cur().lexeme == "integer" || cur().lexeme == "boolean" || cur().lexeme == "char" ||
       cur().lexeme == "string")) {
    t->name = cur().lexeme;
    advance();
  } else if (isKw("void")) {
    if (!allowVoid) fail("'void' is only allowed as a function return type");
    t->name = "void";
    advance();
  } else if (isKw("array")) {
    t->name = "array";
    advance();
    expectDelim("[", "after 'array'");
    if (!isDelim("]")) t->size = parseExpr();
    expectDelimSoft("]", "after the array size");
    t->elem = parseTypeSpec(false);
  } else {
    fail("expected a type name, got " + describe(cur()));
  }
  return t;
}

// initializer ::= expr | '{' [ expr { ',' expr } ] '}'
NodePtr Parser::parseInitializer() {
  if (!isDelim("{")) return parseExpr();
  auto init = std::make_unique<ArrayInit>(cur().loc);
  advance();
  if (!isDelim("}")) {
    init->elems.push_back(parseExpr());
    while (isDelim(",")) { advance(); init->elems.push_back(parseExpr()); }
  }
  expectDelimSoft("}", "to close the array initializer");
  return init;
}

// ---------------------------------------------------------------- statements
// block ::= '{' { statement } '}'      (panic-mode recovery happens per statement)
std::unique_ptr<BlockStmt> Parser::parseBlock() {
  auto blk = std::make_unique<BlockStmt>(cur().loc);
  expectDelim("{", "to start a block");
  while (!isDelim("}") && !atEnd()) {
    size_t start = p_;
    try {
      blk->stmts.push_back(parseStatement());
    } catch (const SyntaxError&) {
      blk->stmts.push_back(std::make_unique<ErrorNode>(all_[sig_[start]].loc));
      syncStatement();
    }
    if (p_ == start && !isDelim("}") && !atEnd()) advance();  // guarantee progress
  }
  expectDelimSoft("}", "to close the block");
  return blk;
}

// statement ::= block | if | for | return | print | local_decl | expr ';'
// The only look-ahead beyond 1 token: "IDENT ':'" starts a declaration, any other
// IDENT starts an expression statement  (LL(2) decision, see docs/grammar.ebnf).
NodePtr Parser::parseStatement() {
  if (isDelim("{")) return parseBlock();
  if (isKw("if")) return parseIf();
  if (isKw("for")) return parseFor();
  if (isKw("return")) return parseReturn();
  if (isKw("print")) return parsePrint();
  if (cur().type == TokType::Ident && nextIsColon()) {
    size_t nameTok = curIdx();
    std::string name = cur().lexeme;
    SrcLoc loc = cur().loc;
    advance(); advance();  // name ':'
    if (isKw("function")) fail("nested functions are not allowed in Minor-B");
    return parseVarRest(nameTok, name, loc);
  }
  auto st = std::make_unique<ExprStmt>(cur().loc);
  st->expr = parseExpr();
  expectDelimSoft(";", "after the expression");
  return st;
}

// if ::= 'if' '(' expr ')' statement [ 'else' statement ]   (else binds to nearest if)
NodePtr Parser::parseIf() {
  auto n = std::make_unique<IfStmt>(cur().loc);
  advance();
  expectDelim("(", "after 'if'");
  n->cond = parseExpr();
  expectDelimSoft(")", "after the condition");
  n->thenBranch = parseStatement();
  if (isKw("else")) { advance(); n->elseBranch = parseStatement(); }
  return n;
}

// for ::= 'for' '(' [expr] ';' [expr] ';' [expr] ')' statement
NodePtr Parser::parseFor() {
  auto n = std::make_unique<ForStmt>(cur().loc);
  advance();
  expectDelim("(", "after 'for'");
  if (!isDelim(";")) n->init = parseExpr();
  expectDelimSoft(";", "in the for header");
  if (!isDelim(";")) n->cond = parseExpr();
  expectDelimSoft(";", "in the for header");
  if (!isDelim(")")) n->step = parseExpr();
  expectDelimSoft(")", "after the for header");
  n->body = parseStatement();
  return n;
}

// return ::= 'return' [expr] ';'
NodePtr Parser::parseReturn() {
  auto n = std::make_unique<ReturnStmt>(cur().loc);
  advance();
  if (!isDelim(";")) n->value = parseExpr();
  expectDelimSoft(";", "after 'return'");
  return n;
}

// print ::= 'print' expr { ',' expr } ';'
NodePtr Parser::parsePrint() {
  auto n = std::make_unique<PrintStmt>(cur().loc);
  advance();
  n->args.push_back(parseExpr());
  while (isDelim(",")) { advance(); n->args.push_back(parseExpr()); }
  expectDelimSoft(";", "after the print list");
  return n;
}

// ---------------------------------------------------------------- expressions
NodePtr Parser::parseExpr() { return parseAssignment(); }

// assignment ::= logical_or [ '=' assignment ]          (right-associative)
NodePtr Parser::parseAssignment() {
  NodePtr lhs = parseBinary(0);
  if (!isOp("=")) return lhs;
  auto n = std::make_unique<AssignExpr>(lhs->loc);
  n->opTok = curIdx();
  if (lhs->kind != NodeKind::Identifier && lhs->kind != NodeKind::IndexExpr)
    reportAt(curIdx(), "invalid assignment target (expected a variable or array element)");
  advance();
  n->target = std::move(lhs);
  n->value = parseAssignment();
  return n;
}

// level ::= next { OP next }     for each row of kLevels
NodePtr Parser::parseBinary(size_t level) {
  if (level == kLevels.size()) return parsePower();
  NodePtr lhs = parseBinary(level + 1);
  while (cur().type == TokType::Operator &&
         std::find(kLevels[level].begin(), kLevels[level].end(), cur().lexeme) != kLevels[level].end()) {
    auto n = std::make_unique<BinaryExpr>(lhs->loc);
    n->op = cur().lexeme;
    n->opTok = curIdx();
    advance();
    n->left = std::move(lhs);
    n->right = parseBinary(level + 1);
    lhs = std::move(n);
  }
  return lhs;
}

// power ::= unary [ '^' power ]      (right-associative; binds tighter than '*')
NodePtr Parser::parsePower() {
  NodePtr base = parseUnary();
  if (!isOp("^")) return base;
  auto n = std::make_unique<BinaryExpr>(base->loc);
  n->op = "^";
  n->opTok = curIdx();
  advance();
  n->left = std::move(base);
  n->right = parsePower();
  return n;
}

// unary ::= ( '-' | '!' ) unary | postfix
NodePtr Parser::parseUnary() {
  if (isOp("-") || isOp("!")) {
    auto n = std::make_unique<UnaryExpr>(cur().loc);
    n->op = cur().lexeme;
    n->opTok = curIdx();
    advance();
    n->operand = parseUnary();
    return n;
  }
  return parsePostfix();
}

// postfix ::= primary { '[' expr ']' | '(' [args] ')' | '++' | '--' }
NodePtr Parser::parsePostfix() {
  NodePtr e = parsePrimary();
  while (true) {
    if (isDelim("[")) {
      auto n = std::make_unique<IndexExpr>(e->loc);
      advance();
      n->base = std::move(e);
      n->index = parseExpr();
      expectDelimSoft("]", "after the array index");
      e = std::move(n);
    } else if (isDelim("(")) {
      if (e->kind != NodeKind::Identifier) fail("only a function name can be called");
      auto* id = static_cast<Identifier*>(e.get());
      auto n = std::make_unique<CallExpr>(id->loc);
      n->callee = id->name;
      n->calleeTok = id->tok;
      advance();
      if (!isDelim(")")) {
        n->args.push_back(parseExpr());
        while (isDelim(",")) { advance(); n->args.push_back(parseExpr()); }
      }
      expectDelimSoft(")", "after the call arguments");
      e = std::move(n);
    } else if (isOp("++") || isOp("--")) {
      auto n = std::make_unique<PostfixExpr>(e->loc);
      n->op = cur().lexeme;
      n->opTok = curIdx();
      advance();
      n->operand = std::move(e);
      e = std::move(n);
    } else {
      return e;
    }
  }
}

// primary ::= INT | CHAR | STRING | 'true' | 'false' | IDENT | '(' expr ')'
NodePtr Parser::parsePrimary() {
  const Token& t = cur();
  switch (t.type) {
    case TokType::IntLit: {
      auto n = std::make_unique<IntLiteral>(t.loc);
      n->tok = curIdx();
      unsigned long long v = 0;
      for (char c : t.lexeme) {
        unsigned d = c - '0';
        if (v > (~0ULL - d) / 10) { v = ~0ULL; break; }  // saturate (lexer already warned)
        v = v * 10 + d;
      }
      n->value = v;
      advance();
      return n;
    }
    case TokType::CharLit: {
      auto n = std::make_unique<CharLiteral>(t.loc);
      n->tok = curIdx();
      std::string s = unescape(t.lexeme.substr(1, t.lexeme.size() - 2));
      n->value = s.empty() ? '\0' : s[0];
      advance();
      return n;
    }
    case TokType::StringLit: {
      auto n = std::make_unique<StringLiteral>(t.loc);
      n->tok = curIdx();
      n->value = unescape(t.lexeme.substr(1, t.lexeme.size() - 2));
      advance();
      return n;
    }
    case TokType::BoolLit: {
      auto n = std::make_unique<BoolLiteral>(t.loc);
      n->tok = curIdx();
      n->value = t.lexeme == "true";
      advance();
      return n;
    }
    case TokType::Ident: {
      auto n = std::make_unique<Identifier>(t.loc);
      n->tok = curIdx();
      n->name = t.lexeme;
      advance();
      return n;
    }
    case TokType::Invalid: {  // already reported by the lexer: swallow it quietly
      auto n = std::make_unique<ErrorNode>(t.loc);
      advance();
      return n;
    }
    default:
      break;
  }
  if (isDelim("(")) {
    advance();
    NodePtr e = parseExpr();
    expectDelimSoft(")", "to close the parenthesised expression");
    return e;
  }
  fail("expected an expression, got " + describe(t));
}
