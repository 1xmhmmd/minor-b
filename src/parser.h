// parser.h - recursive-descent (LL(1), one LL(2) decision) parser for Minor-B.
#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include "ast.h"
#include "token.h"

class Parser {
 public:
  // `tokens` is the FULL token vector (incl. comments/INVALID, which are skipped here).
  explicit Parser(const std::vector<Token>& tokens);
  std::unique_ptr<Program> parseProgram();
  const std::vector<Diagnostic>& diagnostics() const { return diags_; }

 private:
  struct SyntaxError {};  // thrown to unwind to the nearest recovery point (panic mode)

  const std::vector<Token>& all_;
  std::vector<size_t> sig_;  // indices of significant tokens (what the grammar sees)
  size_t p_ = 0;
  long lastErr_ = -1;
  std::vector<Diagnostic> diags_;

  // --- token helpers
  const Token& cur() const { return all_[sig_[p_]]; }
  const Token& peekTok(size_t n = 1) const { return all_[sig_[std::min(p_ + n, sig_.size() - 1)]]; }
  size_t curIdx() const { return sig_[p_]; }
  bool atEnd() const { return cur().type == TokType::Eof; }
  void advance() { if (!atEnd()) p_++; }
  bool isDelim(const char* s) const { return cur().type == TokType::Delimiter && cur().lexeme == s; }
  bool isOp(const char* s) const { return cur().type == TokType::Operator && cur().lexeme == s; }
  bool isKw(const char* s) const { return cur().type == TokType::Keyword && cur().lexeme == s; }
  bool nextIsColon() const { return peekTok().type == TokType::Delimiter && peekTok().lexeme == ":"; }
  std::string describe(const Token& t) const;

  // --- errors
  bool nearInvalid(size_t tokIdx) const;
  void reportAt(size_t tokIdx, const std::string& msg);
  [[noreturn]] void fail(const std::string& msg);
  void expectDelim(const char* s, const std::string& ctx);      // hard: throws
  void expectDelimSoft(const char* s, const std::string& ctx);  // soft: report, pretend it was there
  void syncStatement();
  void syncTopLevel(size_t start);
  void skipBraces();

  // --- grammar (one function per non-terminal; see docs/grammar.ebnf)
  NodePtr parseGlobalDecl();
  NodePtr parseFunctionRest(size_t nameTok, const std::string& name, SrcLoc loc);
  NodePtr parseVarRest(size_t nameTok, const std::string& name, SrcLoc loc);
  std::unique_ptr<Param> parseParam();
  std::unique_ptr<TypeSpec> parseTypeSpec(bool allowVoid);
  NodePtr parseInitializer();
  std::unique_ptr<BlockStmt> parseBlock();
  NodePtr parseStatement();
  NodePtr parseIf();
  NodePtr parseFor();
  NodePtr parseReturn();
  NodePtr parsePrint();
  NodePtr parseExpr();
  NodePtr parseAssignment();
  NodePtr parseBinary(size_t level);
  NodePtr parsePower();
  NodePtr parseUnary();
  NodePtr parsePostfix();
  NodePtr parsePrimary();
};
