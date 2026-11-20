# Minor-B Lexer · Parser · Syntax Highlighter  Mohammad Alinejad 40113308(Phase 1)

```
make            # builds ./minorb   (g++ -std=c++17)
make test       # golden-file tests
./minorb file.b                 # ANSI colours in the terminal
./minorb --html file.b -o out.html
./minorb --tokens file.b        # token stream
./minorb --ast file.b           # abstract syntax tree
./minorb --check file.b         # diagnostics only
```
Docs: `docs/lexical_spec.md` (regexes, longest-match/keyword priority), `docs/grammar.ebnf`, `docs/design.md`.


