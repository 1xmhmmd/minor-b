// minorb - Minor-B lexer, parser and syntax highlighter (phase 1).
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include "ast.h"
#include "highlighter.h"
#include "lexer.h"
#include "parser.h"

static void usage() {
  std::cerr << "Usage: minorb [mode] [-o out] <file.b>\n"
               "  --ansi     colourised output for terminals (default)\n"
               "  --html     standalone HTML/CSS page\n"
               "  --tokens   print the token stream\n"
               "  --ast      print the abstract syntax tree\n"
               "  --check    print diagnostics only (to stdout)\n"
               "Exit status: 0 = no errors, 1 = lexical/syntax errors, 2 = usage / I/O error\n";
}

int main(int argc, char** argv) {
  std::string mode = "--ansi", inPath, outPath;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--ansi" || a == "--html" || a == "--tokens" || a == "--ast" || a == "--check") mode = a;
    else if (a == "-o" && i + 1 < argc) outPath = argv[++i];
    else if (!a.empty() && a[0] == '-') { usage(); return 2; }
    else inPath = a;
  }
  if (inPath.empty()) { usage(); return 2; }

  std::ifstream in(inPath, std::ios::binary);
  if (!in) { std::cerr << "minorb: cannot open '" << inPath << "'\n"; return 2; }
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string src = ss.str();
  const std::string file = inPath;

  // Pipeline: source -> Lexer -> tokens -> Parser -> AST -> Highlighter
  Lexer lexer(src, &file);
  std::vector<Token> tokens = lexer.tokenize();
  Parser parser(tokens);
  std::unique_ptr<Program> ast = parser.parseProgram();

  std::vector<Diagnostic> diags = lexer.diagnostics();
  diags.insert(diags.end(), parser.diagnostics().begin(), parser.diagnostics().end());
  std::stable_sort(diags.begin(), diags.end(), [](const Diagnostic& a, const Diagnostic& b) {
    return a.loc.line != b.loc.line ? a.loc.line < b.loc.line : a.loc.col < b.loc.col;
  });

  std::ostringstream out;
  if (mode == "--tokens") {
    for (const Token& t : tokens) {
      std::string lx;  // show whitespace control chars readably, keep everything else raw
      for (char c : t.lexeme) lx += c == '\n' ? "\\n" : c == '\r' ? "\\r" : c == '\t' ? "\\t" : std::string(1, c);
      out << t.loc.pos() << "\t" << tokTypeName(t.type) << "\t" << lx << "\n";
    }
  } else if (mode == "--ast") {
    dumpAst(ast.get(), out);
  } else if (mode == "--html") {
    out << Highlighter(src, tokens, ast.get(), diags).html(file);
  } else if (mode == "--ansi") {
    out << Highlighter(src, tokens, ast.get(), diags).ansi();
  }

  std::ostream* dest = &std::cout;
  std::ofstream outFile;
  if (!outPath.empty()) {
    outFile.open(outPath, std::ios::binary);
    if (!outFile) { std::cerr << "minorb: cannot write '" << outPath << "'\n"; return 2; }
    dest = &outFile;
  }
  if (mode != "--check") *dest << out.str();

  std::ostream& dstream = (mode == "--check") ? std::cout : std::cerr;
  for (const Diagnostic& d : diags)
    dstream << d.loc.str() << ": error [" << d.phase << "]: " << d.message << "\n";
  return diags.empty() ? 0 : 1;
}
