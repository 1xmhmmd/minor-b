// lexer.h - hand-written DFA-style scanner for Minor-B.
#pragma once
#include <string>
#include <vector>
#include "token.h"

bool isKeyword(const std::string& s);
bool isTypeKeyword(const std::string& s);

class Lexer {
 public:
  Lexer(const std::string& src, const std::string* file) : src_(src), file_(file) {}
  // Never throws / never stops: invalid input yields INVALID tokens + diagnostics.
  std::vector<Token> tokenize();
  const std::vector<Diagnostic>& diagnostics() const { return diags_; }

 private:
  const std::string& src_;
  const std::string* file_;
  size_t pos_ = 0, start_ = 0;
  int line_ = 1, col_ = 1;
  SrcLoc startLoc_;
  std::vector<Token> tokens_;
  std::vector<Diagnostic> diags_;

  int peek(size_t n = 0) const { return pos_ + n < src_.size() ? (unsigned char)src_[pos_ + n] : -1; }
  void advance();
  void begin();
  size_t emit(TokType t);
  void error(size_t tokIdx, const std::string& msg);
  void skipWhitespace();
  void lexIdentOrKeyword();
  void lexNumber();
  void lexString();
  void lexChar();
  void lexLineComment();
  void lexBlockComment();
  void lexOperatorOrDelimiter();
};
