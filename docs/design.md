# System Design – Phase 1

The compiler is divided into several main components. The source code is first passed to the Lexer, which scans the input and generates a sequence of tokens. These tokens are then processed by the Parser to build the Abstract Syntax Tree (AST). Finally, the Highlighter uses both token information and the AST structure to highlight different parts of the source code.

```text
source ─► Lexer ─► tokens ─► Parser ─► AST ─► Highlighter ─► ANSI / HTML
              │                  │                  ▲
              └──── diagnostics ─┴──────────────────┘
```

If an error is detected during lexical or syntax analysis, the corresponding token is marked in red in the highlighting output.

| File | Responsibility |
|---|---|
| `token.h` | Defines the `Token`, `SrcLoc`, and `Diagnostic` structures. |
| `lexer.*` | Converts source text into tokens, detects invalid tokens, and handles lexical errors. |
| `ast.*` | Defines AST node structures and implements the `dumpAst` function. |
| `parser.*` | Performs syntax analysis using recursive descent and builds the AST. |
| `highlighter.*` | Highlights source code based on token types and AST information, and generates ANSI and HTML output. |
| `main.cpp` | Handles command-line options such as `--ansi`, `--html`, `--tokens`, `--ast`, and `--check`. |

## Parser Design

The Parser is implemented using the Recursive Descent approach with an LL(1) parsing strategy. Each grammar rule is handled by a separate function.

To avoid left recursion, the grammar rules that would normally contain left-recursive structures have been rewritten using repetition. As a result, left-associative operators are handled using the `{ OP operand }` pattern.

Operator precedence is managed through a table named `kLevels`, which stores five levels of binary operators.

**Operator Precedence and Associativity**

The operators are ordered from lowest to highest precedence as follows:

`=` (right-associative) ← `||` ← `&&` ← comparison operators ← `+ -` ← `* / %` ← `^` (right-associative) ← unary `- !` ← postfix `[] () ++ --`

Some important details about this ordering are:

- The `&&` operator has higher precedence than `||`, following the same convention used in C.
- All comparison operators belong to the same precedence level. Therefore, an expression such as `a < b < c` is syntactically valid, but it can be rejected during semantic analysis because of a type mismatch.
- Unary operators have higher precedence than `^`. For example, `-a^2` is interpreted as `(-a)^2`.
- The `^` operator is right-associative, so `2^3^2` is interpreted as `2^(3^2)`.

## Grammar Conflicts and Their Solutions

During the implementation of the Parser, several grammar rules were found to overlap. The following solutions were used to handle these cases.

| Case | Problem | Solution |
|---|---|---|
| Declaration vs. expression statement | Both can start with an `IDENT` token. | The next token is checked. If it is `:`, the parser treats the input as a declaration. This is the only place where an additional lookahead (LL(2)) is needed. |
| Dangling `else` | An `else` may be associated with more than one `if`. | The `parseIf` function associates `else` with the nearest unmatched `if`. |
| Function vs. global variable declaration | Both start with `IDENT ':'`. | The token following `:` is checked. The `function` keyword identifies a function declaration. |
| Array initializer vs. block | Both start with `{`. | An opening brace is treated as an array initializer only in an initializer context. Otherwise, it represents a block. |
| Function calls | The postfix rule allows `()` after an expression. | The parser checks that the callee is an `IDENT`; otherwise, it reports an error. |

The main FIRST and FOLLOW sets used by the Parser are:

- `FIRST(type_spec) = {integer, boolean, char, string, array}`
- `FIRST(expr) = {IDENT, INT, CHAR, STRING, true, false, (, -, !}`
- `FIRST(statement) = FIRST(expr) ∪ { { , if, for, return, print }`
- `FOLLOW(statement) = FIRST(statement) ∪ { }, else }`

## AST Structure

The main AST structures have been implemented in this phase. The nodes are divided into two general groups.

**Leaf nodes:**

`IntLiteral`, `BoolLiteral`, `CharLiteral`, `StringLiteral`, and `Identifier`.

**Internal nodes:**

`Program`, `FuncDecl`, `Prototype`, `Param`, `VarDecl`, `TypeSpec`, `BlockStmt`, `IfStmt`, `ForStmt`, `ReturnStmt`, `PrintStmt`, `ExprStmt`, `BinaryExpr`, `UnaryExpr`, `PostfixExpr`, `AssignExpr`, `CallExpr`, `IndexExpr`, and `ArrayInit`.

An additional node named `ErrorNode` is used to represent syntax errors while keeping as much of the AST structure as possible.

Every AST node contains two common fields:

- `loc`: Stores the location of the first token belonging to the node, including its line and column.
- `type`: Defined as `std::optional<std::string>`. It is currently left as `null` and will be used to store type information in later phases.

Parentheses are not represented directly in the AST because the tree structure itself is sufficient to preserve the grouping and evaluation order of expressions.

## Error Handling and Parser Recovery

One important part of the Parser implementation is how it handles invalid input. Instead of stopping the entire parsing process after the first error, the Parser attempts to recover and continue analyzing the remaining source code.

Two recovery approaches are used.

**1. Hard Errors**

For errors such as a missing expression, the `fail` function creates a `SyntaxError` exception. The exception is propagated to the nearest recovery point, where the Parser uses `syncStatement` or `syncTopLevel` to find a suitable position to continue parsing.

At the statement level, tokens are skipped until one of the following is found:

- `;`, which is consumed after recovery.
- `}`, which is preserved to maintain the block structure.
- One of the keywords `if`, `for`, `return`, or `print`.
- The beginning of a new declaration in the form `name :`.

If an opening brace `{` is encountered during recovery, the entire nested block is skipped while keeping track of brace depth. This prevents the parser from losing track of the block structure.

At the top level, `syncTopLevel` uses brace-depth counting to find a suitable recovery position.

**2. Soft Errors**

For missing delimiters such as `)`, `]`, `;`, and `}`, the `expectDelimSoft` function is used.

In this case, the error is recorded, but no exception is thrown. Parsing continues as if the missing delimiter had been inserted.

For example, consider the following input:

```text
if (y > 0 {
```

Since the closing `)` is missing, the Parser reports a single error and continues parsing the remaining parts of the file.

**Additional measures**

A few additional checks are included to prevent problems during error recovery:

- If the Parser does not advance after recovery, one token is consumed to prevent an infinite loop.
- The same token is not reported as an error more than once.
- Syntax errors caused only by an `INVALID` token produced by the Lexer are not reported again by the Parser.

## Highlighter Implementation

Code highlighting is based on AST information rather than relying only on token types. The process consists of three steps.

First, a default highlighting role is assigned to each token according to its type. Then, the AST is traversed to determine more specific roles for identifiers.

For example:

- Function names in `FuncDecl` and identifiers used as `CallExpr.callee` are highlighted in yellow (Function).
- Identifiers belonging to `Identifier`, `Param`, and `VarDecl` are highlighted in white (Variable).
- Type keywords inside `TypeSpec` are highlighted in cyan (Type).

In the final step, tokens associated with errors are marked in red and underlined. In HTML output, the corresponding error message is also available through the `title` attribute.

Using the AST is important here because token information alone is not always enough to determine the role of an identifier. For example, in `f(x)`, simply looking at the token sequence does not always tell us whether `f` refers to a function or a variable.

In later phases, this classification can be improved by using a symbol table, especially in cases where a local variable has the same name as a function.

For ANSI output, whitespace and other original characters are preserved so that the source code layout does not change. If a token spans multiple lines, its color is handled separately for each line.

The HTML output is generated inside a `pre` element with the `code` class. Different parts of the source are wrapped in `span` elements with classes such as `kw`. The required CSS is embedded directly in the output, so no JavaScript is needed.

## Testing

A set of 30 Golden tests has been prepared to check the different parts of the implementation. The tests can be executed using:

```bash
make test
```

The test cases cover both valid and invalid source files.

For valid inputs, the tests check token output, AST structure, HTML output, and whether the parser finishes without reporting errors.

For invalid inputs, they also check the exact error messages, the presence of `ErrorNode` in the AST, and the generated ANSI output.

The main error cases covered by the tests include:

- Unknown characters in the source code.
- Unterminated strings, character literals, and comments.
- Missing delimiters such as `;`, `)`, and `}`.
- Nested function declarations.
- Invalid use of the `void` type.
- Invalid assignment targets.
- An `else` without a matching `if`.
- Unexpected end of file.

Overall, the first phase provides the main components required for lexical analysis, syntax analysis, AST construction, error recovery, and source code highlighting. The implemented test cases are used to verify that these components behave as expected for both valid and invalid inputs.



------------------------------------------------------------------------------------------------------------------------------


# طراحی سیستم – فاز اول

در فاز اول، ساختار کلی کامپایلر به چند بخش اصلی تقسیم شده است. ابتدا کد منبع توسط Lexer بررسی می‌شود و توکن‌های آن تولید می‌شوند. سپس Parser با استفاده از این توکن‌ها، درخت نحوی یا AST را می‌سازد. در نهایت Highlighter با بررسی توکن‌ها و ساختار AST، بخش‌های مختلف کد را رنگ‌آمیزی می‌کند.

```text
source ─► Lexer ─► tokens ─► Parser ─► AST ─► Highlighter ─► ANSI / HTML
              │                  │                  ▲
              └──── diagnostics ─┴──────────────────┘
```

در صورتی که هنگام تحلیل کد خطایی تشخیص داده شود، محل مربوط به آن در خروجی Highlighter با رنگ قرمز مشخص می‌شود.

| فایل | وظیفه |
|---|---|
| `token.h` | تعریف ساختارهای `Token`، `SrcLoc` و `Diagnostic` |
| `lexer.*` | تبدیل متن ورودی به توکن، تشخیص توکن‌های نامعتبر و مدیریت خطاهای لغوی |
| `ast.*` | تعریف گره‌های درخت AST و پیاده‌سازی تابع `dumpAst` |
| `parser.*` | تحلیل نحوی کد با روش بازگشتی نزولی و ساخت AST |
| `highlighter.*` | رنگ‌آمیزی کد بر اساس نوع توکن و اطلاعات AST و تولید خروجی ANSI و HTML |
| `main.cpp` | مدیریت ورودی‌های خط فرمان مانند `--ansi`، `--html`، `--tokens`، `--ast` و `--check` |

## راهبرد پیاده‌سازی Parser

برای پیاده‌سازی Parser از روش Recursive Descent با رویکرد LL(1) استفاده شده است. در این روش، برای هر قاعده از گرامر یک تابع جداگانه در نظر گرفته می‌شود.

برای جلوگیری از بازگشت چپ، قواعدی که در حالت معمول به‌صورت بازگشتی نوشته می‌شوند، با استفاده از تکرار بازنویسی شده‌اند. به همین دلیل، عملگرهای چپ‌شرکت‌پذیر با ساختار `{ OP operand }` پردازش می‌شوند.

برای مدیریت سطوح مختلف تقدم عملگرها نیز از جدول `kLevels` استفاده شده که پنج سطح از عملگرهای دودویی را نگهداری می‌کند.

**تقدم و شرکت‌پذیری عملگرها**

ترتیب عملگرها از کمترین به بیشترین تقدم به شکل زیر در نظر گرفته شده است:

`=` (راست‌شرکت‌پذیر) ← `||` ← `&&` ← عملگرهای مقایسه‌ای ← `+ -` ← `* / %` ← `^` (راست‌شرکت‌پذیر) ← عملگرهای یکانی `- !` ← عملگرهای پسوندی `[] () ++ --`

چند نکته در این بخش وجود دارد:

- تقدم `&&` از `||` بیشتر است که مشابه زبان C در نظر گرفته شده است.
- تمام عملگرهای مقایسه‌ای در یک سطح قرار دارند. بنابراین عبارتی مثل `a < b < c` از نظر نحوی پذیرفته می‌شود، اما در مرحله تحلیل معنایی به دلیل ناسازگاری نوع‌ها قابل رد شدن است.
- عملگر یکانی نسبت به `^` تقدم بیشتری دارد. در نتیجه `-a^2` به شکل `(-a)^2` تفسیر می‌شود.
- عملگر `^` راست‌شرکت‌پذیر است؛ بنابراین `2^3^2` معادل `2^(3^2)` خواهد بود.

## تعارض‌های موجود در گرامر و نحوه حل آن‌ها

در زمان پیاده‌سازی Parser چند مورد از قواعد گرامر با یکدیگر هم‌پوشانی داشتند. برای هرکدام راه‌حل جداگانه‌ای در نظر گرفته شده است.

| مورد | مشکل | راه‌حل |
|---|---|---|
| شروع اعلان و دستور عبارتی | هر دو می‌توانند با `IDENT` شروع شوند. | با نگاه کردن به توکن بعدی مشخص می‌شود که در صورت وجود `:`، اعلان در حال پردازش است. این مورد تنها به یک نگاه اضافه (LL(2)) نیاز دارد. |
| `else` معلق | مشخص نیست `else` متعلق به کدام `if` است. | در تابع `parseIf`، نزدیک‌ترین `if` بدون `else` به‌عنوان مقصد انتخاب می‌شود. |
| اعلان تابع و متغیر سراسری | هر دو با ساختار `IDENT ':'` شروع می‌شوند. | بعد از `:`، وجود کلمه کلیدی `function` نوع اعلان را مشخص می‌کند. |
| آرایه و بلوک `{...}` | هر دو با `{` شروع می‌شوند. | `{` فقط در محل مقدار اولیه به‌عنوان آرایه در نظر گرفته می‌شود و در سایر موارد مربوط به بلوک است. |
| فراخوانی تابع | قواعد postfix اجازه استفاده از `()` را برای عبارت‌ها می‌دهند. | برای فراخوانی تابع، بررسی می‌شود که callee از نوع `IDENT` باشد. |

مجموعه‌های FIRST و FOLLOW مورد استفاده در Parser نیز به شکل زیر هستند:

- `FIRST(type_spec) = {integer, boolean, char, string, array}`
- `FIRST(expr) = {IDENT, INT, CHAR, STRING, true, false, (, -, !}`
- `FIRST(statement) = FIRST(expr) ∪ { { , if, for, return, print }`
- `FOLLOW(statement) = FIRST(statement) ∪ { }, else }`

## ساختار AST

در این فاز، ساختار اصلی درخت AST پیاده‌سازی شده است. گره‌ها به دو گروه کلی تقسیم می‌شوند:

**گره‌های برگ:**

`IntLiteral`، `BoolLiteral`، `CharLiteral`، `StringLiteral` و `Identifier`

**گره‌های داخلی:**

`Program`، `FuncDecl`، `Prototype`، `Param`، `VarDecl`، `TypeSpec`، `BlockStmt`، `IfStmt`، `ForStmt`، `ReturnStmt`، `PrintStmt`، `ExprStmt`، `BinaryExpr`، `UnaryExpr`، `PostfixExpr`، `AssignExpr`، `CallExpr`، `IndexExpr` و `ArrayInit`

همچنین برای مدیریت خطاهای نحوی، گره `ErrorNode` در نظر گرفته شده است تا در صورت بروز خطا، ساختار AST تا حد امکان حفظ شود.

هر گره دارای دو ویژگی مشترک است:

- `loc`: موقعیت اولین توکن مربوط به گره در فایل (شامل شماره خط و ستون).
- `type`: از نوع `std::optional<std::string>` است و در حال حاضر مقدار آن `null` باقی می‌ماند. این فیلد در مراحل بعدی برای نگهداری اطلاعات نوع استفاده خواهد شد.

پرانتزها به‌صورت مستقیم در AST ذخیره نمی‌شوند، چون ساختار درخت به‌تنهایی ترتیب و نحوه گروه‌بندی عبارت‌ها را مشخص می‌کند.

## مدیریت خطا و بازیابی Parser

یکی از بخش‌های مهم این فاز، نحوه برخورد Parser با ورودی‌های نامعتبر است. هدف این بوده که با مشاهده اولین خطا، کل فرآیند تحلیل متوقف نشود و Parser بتواند تا جای ممکن ادامه دهد.

برای این کار دو روش در نظر گرفته شده است:

**۱. خطاهای سخت (Hard Errors)**

در مواردی مثل نبودن عبارت مورد انتظار، تابع `fail` یک `SyntaxError` ایجاد می‌کند. این خطا تا نزدیک‌ترین نقطه بازیابی منتقل می‌شود و سپس Parser با استفاده از `syncStatement` یا `syncTopLevel` ادامه تحلیل را از یک موقعیت مناسب از سر می‌گیرد.

در بازیابی دستورات، Parser توکن‌ها را تا رسیدن به یکی از موارد زیر رد می‌کند:

- `;` که پس از رسیدن مصرف می‌شود.
- `}` که برای حفظ ساختار بلوک باقی می‌ماند.
- کلمات کلیدی `if`، `for`، `return` و `print`.
- شروع یک اعلان جدید به شکل `نام :`.

اگر هنگام بازیابی به `{` برسیم، بلوک داخلی با در نظر گرفتن عمق آکولادها رد می‌شود تا ترتیب باز و بسته شدن بلوک‌ها به هم نریزد. در سطح بالای برنامه نیز از شمارش عمق آکولاد برای پیدا کردن محل مناسب ادامه تحلیل استفاده می‌شود.

**۲. خطاهای نرم (Soft Errors)**

برای جداکننده‌های گمشده مانند `)`، `]`، `;` و `}` از تابع `expectDelimSoft` استفاده شده است.

در این حالت خطا ثبت می‌شود، اما برخلاف خطاهای سخت، استثنایی پرتاب نمی‌شود و Parser با فرض اینکه جداکننده موردنظر وجود داشته است، به تحلیل ادامه می‌دهد.

برای مثال، در عبارت زیر:

```text
if (y > 0 {
```

به دلیل نبودن `)` فقط یک خطا گزارش می‌شود و امکان ادامه تحلیل بخش‌های بعدی فایل وجود دارد.

**موارد دیگری که برای جلوگیری از مشکلات احتمالی در نظر گرفته شده‌اند:**

- اگر بعد از اجرای عملیات بازیابی، موقعیت Parser تغییر نکرده باشد، یک توکن مصرف می‌شود تا حلقه بی‌نهایت ایجاد نشود.
- برای یک توکن، خطای تکراری گزارش نمی‌شود.
- اگر خطای نحوی صرفاً به دلیل تولید توکن `INVALID` توسط Lexer ایجاد شده باشد، از گزارش مجدد همان خطا در Parser جلوگیری می‌شود.

## پیاده‌سازی Highlighter

رنگ‌آمیزی کد بر اساس اطلاعات AST انجام می‌شود، نه صرفاً نوع توکن‌ها. این کار در سه مرحله صورت می‌گیرد.

ابتدا برای هر توکن بر اساس نوع آن یک رنگ پیش‌فرض تعیین می‌شود. سپس AST پیمایش می‌شود تا نقش دقیق‌تر شناسه‌ها مشخص شود.

برای مثال:

- نام تابع در `FuncDecl` و شناسه موجود در `CallExpr.callee` با رنگ زرد (Function) نمایش داده می‌شود.
- شناسه‌های مربوط به `Identifier`، `Param` و `VarDecl` با رنگ سفید (Variable) مشخص می‌شوند.
- کلمات مربوط به نوع داده در `TypeSpec` با رنگ فیروزه‌ای (Type) نمایش داده می‌شوند.

در مرحله آخر، توکن‌هایی که در تشخیص خطا نقش داشته‌اند، با رنگ قرمز و زیرخط مشخص می‌شوند. در خروجی HTML نیز پیام خطا از طریق ویژگی `title` قابل مشاهده است.

استفاده از AST در این قسمت اهمیت دارد؛ چون با بررسی ساده توکن‌ها نمی‌توان همیشه تشخیص داد که یک شناسه مانند `f` در عبارت `f(x)` نام تابع است یا یک متغیر. در مراحل بعدی و با اضافه شدن جدول نمادها، این تشخیص دقیق‌تر خواهد شد؛ به‌خصوص در مواردی که یک متغیر محلی هم‌نام یک تابع باشد.

برای خروجی ANSI، فاصله‌ها و کاراکترهای اصلی از متن ورودی حفظ می‌شوند تا شکل کد تغییر نکند. همچنین اگر یک توکن چند خط را پوشش دهد، رنگ‌آمیزی هر خط به‌صورت جداگانه مدیریت می‌شود.

خروجی HTML نیز در قالب یک تگ `pre` با کلاس `code` تولید می‌شود. برای رنگ‌آمیزی بخش‌های مختلف از تگ‌های `span` و کلاس‌هایی مانند `kw` استفاده شده و CSS موردنیاز مستقیماً در خروجی قرار می‌گیرد. در این بخش نیازی به JavaScript نیست.

## تست و بررسی عملکرد

برای بررسی بخش‌های مختلف پیاده‌سازی، مجموعه‌ای شامل ۳۰ تست Golden در نظر گرفته شده است که از طریق دستور زیر اجرا می‌شوند:

```bash
make test
```

این تست‌ها هم ورودی‌های معتبر و هم ورودی‌های دارای خطا را پوشش می‌دهند.

برای ورودی‌های معتبر، مواردی مانند خروجی توکن‌ها، ساختار AST، خروجی HTML و نبود خطا بررسی می‌شوند.

در مورد ورودی‌های نامعتبر نیز علاوه بر متن دقیق پیام‌های خطا، ساخته شدن `ErrorNode` در AST و خروجی ANSI بررسی می‌شود.

مهم‌ترین حالت‌هایی که در تست‌ها بررسی شده‌اند عبارت‌اند از:

- وجود کاراکتر ناشناخته در ورودی.
- بسته نشدن رشته، کاراکتر یا کامنت.
- حذف شدن جداکننده‌هایی مانند `;`، `)` و `}`.
- تعریف تابع در داخل تابع دیگر.
- استفاده از نوع نامعتبر `void`.
- استفاده از عبارت نامعتبر به‌عنوان مقصد انتساب.
- وجود `else` بدون `if` متناظر.
- پایان یافتن زودهنگام فایل.

در مجموع، در پایان فاز اول اجزای اصلی تحلیل لغوی، تحلیل نحوی، ساخت AST، مدیریت خطا و رنگ‌آمیزی کد پیاده‌سازی شده‌اند و تست‌های در نظر گرفته‌شده نیز برای بررسی صحت عملکرد این بخش‌ها استفاده می‌شوند.