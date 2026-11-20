#include "lexer.h"
#include <string_view>
#include <unordered_set>

namespace {
const std::unordered_set<std::string> kKeywords = {
    "array", "boolean", "char", "else", "false", "for", "function", "if",
    "integer", "print", "return", "string", "true", "void", "while"};
const std::unordered_set<std::string> kTypeKeywords = {
    "array", "boolean", "char", "integer", "string", "void"};

constexpr size_t kMaxLen = 256;  // max identifier / string length (spec B-2, B-3)

bool isAlpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool isDigit(int c) { return c >= '0' && c <= '9'; }
bool isContinuation(int c) { return c >= 0 && (c & 0xC0) == 0x80; }  // UTF-8 tail byte

std::string describeChar(const std::string& lx) {
  unsigned char c = lx.empty() ? 0 : (unsigned char)lx[0];
  if (c >= 0x20 && c < 0x7F) return "'" + lx + "'";
  if (c >= 0x80) return "'" + lx + "' (non-ASCII)";
  static const char* hex = "0123456789ABCDEF";
  return std::string("0x") + hex[c >> 4] + hex[c & 15];
}
}  // namespace

bool isKeyword(const std::string& s) { return kKeywords.count(s) > 0; }
bool isTypeKeyword(const std::string& s) { return kTypeKeywords.count(s) > 0; }

void Lexer::advance() {
  char c = src_[pos_++];
  if (c == '\n') { line_++; col_ = 1; }
  else if (!isContinuation((unsigned char)c)) col_++;  // count code points, not bytes
}

void Lexer::begin() { start_ = pos_; startLoc_ = SrcLoc{file_, line_, col_}; }

size_t Lexer::emit(TokType t) {
  Token tok;
  tok.type = t;
  tok.lexeme = src_.substr(start_, pos_ - start_);
  tok.loc = startLoc_;
  tok.offset = start_;
  tok.length = pos_ - start_;
  tok.index = tokens_.size();
  tokens_.push_back(tok);
  return tok.index;
}

void Lexer::error(size_t idx, const std::string& msg) {
  diags_.push_back(Diagnostic{"lexical", msg, tokens_[idx].loc, (long)idx});
}

void Lexer::skipWhitespace() {
  while (pos_ < src_.size()) {
    char c = src_[pos_];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') advance();
    else break;
  }
}

std::vector<Token> Lexer::tokenize() {
  tokens_.clear();
  diags_.clear();
  while (true) {
    skipWhitespace();
    begin();
    if (pos_ >= src_.size()) { emit(TokType::Eof); break; }
    int c = peek();
    if (isAlpha(c)) lexIdentOrKeyword();
    else if (isDigit(c)) lexNumber();
    else if (c == '"') lexString();
    else if (c == '\'') lexChar();
    else if (c == '/' && peek(1) == '/') lexLineComment();
    else if (c == '/' && peek(1) == '*') lexBlockComment();
    else lexOperatorOrDelimiter();
  }
  return tokens_;
}

// IDENT = [a-zA-Z_][a-zA-Z0-9_]* ; keywords are looked up AFTER the longest
// identifier-shaped lexeme was matched => "return" is KEYWORD, "returned" is IDENT.
void Lexer::lexIdentOrKeyword() {
  while (isAlpha(peek()) || isDigit(peek())) advance();
  std::string text = src_.substr(start_, pos_ - start_);
  TokType t = TokType::Ident;
  if (text == "true" || text == "false") t = TokType::BoolLit;
  else if (isKeyword(text)) t = TokType::Keyword;
  size_t idx = emit(t);
  if (t == TokType::Ident && text.size() > kMaxLen)
    error(idx, "identifier longer than 256 characters");
}

// INT = [0-9]+   (the sign is a separate unary operator, handled by the parser)
void Lexer::lexNumber() {
  while (isDigit(peek())) advance();
  std::string text = src_.substr(start_, pos_ - start_);
  size_t nz = text.find_first_not_of('0');
  std::string d = nz == std::string::npos ? "0" : text.substr(nz);
  bool tooBig = d.size() > 19 || (d.size() == 19 && d > "9223372036854775808");
  size_t idx = emit(TokType::IntLit);
  if (tooBig) error(idx, "integer literal does not fit in 64 bits");
}

// STRING = '"' ( [^"\\\n] | '\\' [^\n] )* '"'
void Lexer::lexString() {
  advance();  // opening quote
  size_t count = 0;
  bool closed = false;
  while (pos_ < src_.size()) {
    char c = src_[pos_];
    if (c == '\n') break;
    if (c == '"') { advance(); closed = true; break; }
    if (c == '\\') {
      advance();
      if (pos_ < src_.size() && src_[pos_] != '\n') advance();
      count++;
      continue;
    }
    advance();
    count++;
  }
  if (!closed) {
    size_t idx = emit(TokType::Invalid);
    error(idx, "unterminated string literal");
    return;
  }
  size_t idx = emit(TokType::StringLit);
  if (count > kMaxLen) error(idx, "string literal longer than 256 characters");
}

// CHAR = "'" ( [^'\\\n] | '\\' [^\n] ) "'"
void Lexer::lexChar() {
  advance();  // opening quote
  size_t count = 0;
  bool closed = false, nonAscii = false;
  while (pos_ < src_.size()) {
    char c = src_[pos_];
    if (c == '\n') break;
    if (c == '\'') { advance(); closed = true; break; }
    if (c == '\\') {
      advance();
      if (pos_ < src_.size() && src_[pos_] != '\n') advance();
      count++;
      continue;
    }
    if ((unsigned char)c >= 0x80) nonAscii = true;
    advance();
    if (!isContinuation(peek())) count++;
  }
  if (!closed) {
    size_t idx = emit(TokType::Invalid);
    error(idx, "unterminated character literal");
  } else if (count != 1) {
    size_t idx = emit(TokType::Invalid);
    error(idx, count == 0 ? "empty character literal"
                          : "character literal must contain exactly one character");
  } else if (nonAscii) {
    size_t idx = emit(TokType::Invalid);
    error(idx, "character literal must be a single ASCII character");
  } else {
    emit(TokType::CharLit);
  }
}

// LINE_COMMENT = '//' [^\n]*
void Lexer::lexLineComment() {
  while (pos_ < src_.size() && src_[pos_] != '\n') advance();
  emit(TokType::LineComment);
}

// BLOCK_COMMENT = '/*' ... '*/'   (not nested, like C)
void Lexer::lexBlockComment() {
  advance(); advance();
  while (pos_ < src_.size()) {
    if (src_[pos_] == '*' && peek(1) == '/') {
      advance(); advance();
      emit(TokType::BlockComment);
      return;
    }
    advance();
  }
  size_t idx = emit(TokType::Invalid);
  error(idx, "unterminated block comment");
}

void Lexer::lexOperatorOrDelimiter() {
  int c = peek(), n = peek(1);
  // Longest match: try the two-character operators before single characters,
  // so "<=" is ONE token and never '<' '='.
  static const char* two[] = {"++", "--", "&&", "||", "==", "!=", "<=", ">="};
  for (const char* t : two) {
    if (c == t[0] && n == t[1]) { advance(); advance(); emit(TokType::Operator); return; }
  }
  if (std::string_view("+-*/%^!=<>").find((char)c) != std::string_view::npos) {
    advance(); emit(TokType::Operator); return;
  }
  if (std::string_view("()[]{},;:").find((char)c) != std::string_view::npos) {
    advance(); emit(TokType::Delimiter); return;
  }
  // Unknown character: emit INVALID, report, skip it, keep scanning.
  advance();
  while (isContinuation(peek())) advance();  // whole UTF-8 sequence = 1 token
  size_t idx = emit(TokType::Invalid);
  std::string lx = tokens_[idx].lexeme;
  std::string msg = "unexpected character " + describeChar(lx);
  if (lx == "&") msg += " (did you mean '&&'?)";
  if (lx == "|") msg += " (did you mean '||'?)";
  error(idx, msg);
}
