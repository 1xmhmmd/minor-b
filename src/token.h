// token.h - token / location / diagnostic definitions shared by all phases.
#pragma once
#include <cstddef>
#include <string>

enum class TokType {
  Keyword, BoolLit, Ident, IntLit, StringLit, CharLit,
  Operator, Delimiter, LineComment, BlockComment, Invalid, Eof
};

inline const char* tokTypeName(TokType t) {
  switch (t) {
    case TokType::Keyword:      return "KEYWORD";
    case TokType::BoolLit:      return "BOOL";
    case TokType::Ident:        return "IDENT";
    case TokType::IntLit:       return "INT";
    case TokType::StringLit:    return "STRING";
    case TokType::CharLit:      return "CHAR";
    case TokType::Operator:     return "OP";
    case TokType::Delimiter:    return "DELIM";
    case TokType::LineComment:  return "LINE_COMMENT";
    case TokType::BlockComment: return "BLOCK_COMMENT";
    case TokType::Invalid:      return "INVALID";
    case TokType::Eof:          return "EOF";
  }
  return "?";
}

// Source location: file name + 1-based line and column.
struct SrcLoc {
  const std::string* file = nullptr;  // owned by main(), outlives all tokens
  int line = 0, col = 0;
  std::string pos() const { return std::to_string(line) + ":" + std::to_string(col); }
  std::string str() const { return (file ? *file : std::string("<input>")) + ":" + pos(); }
};

struct Token {
  TokType type = TokType::Eof;
  std::string lexeme;    // exact substring of the source
  SrcLoc loc;            // location of the first character
  size_t offset = 0;     // byte offset in source (used by the highlighter)
  size_t length = 0;
  size_t index = 0;      // position in the full token vector
};

struct Diagnostic {
  std::string phase;     // "lexical" or "syntax"
  std::string message;
  SrcLoc loc;
  long tokIdx = -1;      // offending token (index into full token vector)
};
