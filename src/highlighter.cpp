#include "highlighter.h"
#include <map>
#include "lexer.h"

Highlighter::Highlighter(const std::string& src, const std::vector<Token>& tokens, const Node* ast,
                         const std::vector<Diagnostic>& diags)
    : src_(src), toks_(tokens), diags_(diags), roles_(tokens.size(), Role::Default) {
  // 1) token-level defaults
  for (const Token& t : toks_) {
    Role r = Role::Default;
    switch (t.type) {
      case TokType::Keyword: r = isTypeKeyword(t.lexeme) ? Role::Type : Role::Keyword; break;
      case TokType::BoolLit:
      case TokType::IntLit: r = Role::Literal; break;
      case TokType::StringLit:
      case TokType::CharLit: r = Role::String; break;
      case TokType::Operator: r = Role::Operator; break;
      case TokType::LineComment:
      case TokType::BlockComment: r = Role::Comment; break;
      case TokType::Invalid: r = Role::Error; break;
      case TokType::Ident: r = Role::Variable; break;
      default: break;
    }
    roles_[t.index] = r;
  }
  // 2) context-sensitive roles from the AST
  if (ast) walk(ast);
  // 3) diagnostics
  for (const Diagnostic& d : diags_)
    if (d.tokIdx >= 0 && toks_[d.tokIdx].type != TokType::Eof) roles_[d.tokIdx] = Role::Error;
}

void Highlighter::walk(const Node* n) {
  switch (n->kind) {
    case NodeKind::FuncDecl:   roles_[static_cast<const FuncDecl*>(n)->nameTok] = Role::Function; break;
    case NodeKind::VarDecl:    roles_[static_cast<const VarDecl*>(n)->nameTok] = Role::Variable; break;
    case NodeKind::Param:      roles_[static_cast<const Param*>(n)->nameTok] = Role::Variable; break;
    case NodeKind::TypeSpec:   roles_[static_cast<const TypeSpec*>(n)->tok] = Role::Type; break;
    case NodeKind::Identifier: roles_[static_cast<const Identifier*>(n)->tok] = Role::Variable; break;
    case NodeKind::CallExpr:   roles_[static_cast<const CallExpr*>(n)->calleeTok] = Role::Function; break;
    default: break;
  }
  Children ch;
  n->children(ch);
  for (auto& [role, c] : ch) walk(c);
}

namespace {
const char* ansiCode(Role r) {
  switch (r) {
    case Role::Keyword:  return "\x1b[1;34m";      // bold blue
    case Role::Type:     return "\x1b[36m";        // teal / cyan
    case Role::Variable: return "\x1b[97m";        // white
    case Role::Function: return "\x1b[33m";        // yellow
    case Role::Literal:  return "\x1b[38;5;208m";  // orange
    case Role::String:   return "\x1b[32m";        // green
    case Role::Operator: return "\x1b[37m";        // light gray
    case Role::Comment:  return "\x1b[3;90m";      // dim gray, italic
    case Role::Error:    return "\x1b[4;31m";      // red underline
    default:             return "";
  }
}
const char* cssClass(Role r) {
  switch (r) {
    case Role::Keyword:  return "kw";
    case Role::Type:     return "ty";
    case Role::Variable: return "var";
    case Role::Function: return "fn";
    case Role::Literal:  return "lit";
    case Role::String:   return "str";
    case Role::Operator: return "op";
    case Role::Comment:  return "cmt";
    case Role::Error:    return "err";
    default:             return "";
  }
}
std::string htmlEscape(const std::string& s) {
  std::string o;
  for (char c : s) {
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      default: o += c;
    }
  }
  return o;
}
}  // namespace

std::string Highlighter::ansi() const {
  std::string out;
  size_t prev = 0;
  for (const Token& t : toks_) {
    if (t.type == TokType::Eof) break;
    out += src_.substr(prev, t.offset - prev);  // whitespace between tokens, verbatim
    prev = t.offset + t.length;
    const char* code = ansiCode(roles_[t.index]);
    if (!*code) { out += t.lexeme; continue; }
    // colour each line separately so multi-line comments survive terminals/pagers
    size_t i = 0;
    while (i < t.lexeme.size()) {
      size_t nl = t.lexeme.find('\n', i);
      size_t end = nl == std::string::npos ? t.lexeme.size() : nl;
      if (end > i) out += code + t.lexeme.substr(i, end - i) + "\x1b[0m";
      if (nl == std::string::npos) break;
      out += '\n';
      i = nl + 1;
    }
  }
  out += src_.substr(prev);
  return out;
}

std::string Highlighter::html(const std::string& title) const {
  std::map<size_t, std::string> tips;
  for (const Diagnostic& d : diags_)
    if (d.tokIdx >= 0) tips[d.tokIdx] += (tips.count(d.tokIdx) ? "; " : "") + d.message;

  std::string body;
  size_t prev = 0;
  for (const Token& t : toks_) {
    if (t.type == TokType::Eof) break;
    body += htmlEscape(src_.substr(prev, t.offset - prev));
    prev = t.offset + t.length;
    const char* cls = cssClass(roles_[t.index]);
    if (!*cls) { body += htmlEscape(t.lexeme); continue; }
    body += std::string("<span class=\"") + cls + "\"";
    if (tips.count(t.index)) body += " title=\"" + htmlEscape(tips[t.index]) + "\"";
    body += ">" + htmlEscape(t.lexeme) + "</span>";
  }
  body += htmlEscape(src_.substr(prev));

  std::string diag;
  for (const Diagnostic& d : diags_)
    diag += "<li><b>" + htmlEscape(d.loc.str()) + "</b> [" + d.phase + "] " + htmlEscape(d.message) + "</li>\n";

  std::string h;
  h += "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n<title>" + htmlEscape(title) +
       "</title>\n<style>\n"
       "body{background:#1e1e1e;color:#d4d4d4;font-family:system-ui,sans-serif;margin:2rem}\n"
       "h1{font-size:1.1rem;font-weight:600}\n"
       "pre.code{background:#252526;padding:1rem;border-radius:6px;overflow:auto;"
       "font-family:Consolas,'DejaVu Sans Mono',monospace;font-size:14px;line-height:1.45;tab-size:4}\n"
       ".kw{color:#569cd6;font-weight:bold}\n"
       ".ty{color:#4ec9b0}\n"
       ".var{color:#f0f0f0}\n"
       ".fn{color:#e5c07b}\n"
       ".lit{color:#e08a3c}\n"
       ".str{color:#98c379}\n"
       ".op{color:#b0b0b0}\n"
       ".cmt{color:#7f848e;font-style:italic}\n"
       ".err{color:#ff5555;text-decoration:underline;text-decoration-color:#ff5555}\n"
       "ul.diag{color:#ff8888;font-family:monospace;font-size:13px}\n"
       "</style>\n</head>\n<body>\n<h1>" + htmlEscape(title) + "</h1>\n<pre class=\"code\">" + body + "</pre>\n";
  if (!diag.empty()) h += "<h1>Diagnostics</h1>\n<ul class=\"diag\">\n" + diag + "</ul>\n";
  h += "</body>\n</html>\n";
  return h;
}
