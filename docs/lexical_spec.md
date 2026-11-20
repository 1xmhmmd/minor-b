# Minor-B Lexical Specification

This section describes the lexical rules of the Minor-B language. The specification is based on Appendix B of the course slides and the implementation details from Phase 1 of the project.

## 1. Token Categories and Lexical Patterns

The Lexer divides the source code into a sequence of tokens. Each token is identified according to a specific lexical pattern.

| Token Type (`TokType`) | Regular Expression / Pattern | Description |
|---|---|---|
| Whitespace | `[ \t\r\n]+` | Whitespace characters are skipped. They only update the current line and column. |
| LineComment | `//[^\n]*` | A single-line comment extending to the end of the line. |
| BlockComment | `/\*([^\*]|\*+[^*/])*\*+/` | A non-nested block comment, similar to C. An unterminated comment produces an INVALID token. |
| Keyword | `array boolean char else false for function if integer print return string true void while` | Contains 15 reserved words. `while` is reserved, although no corresponding statement is defined in the grammar. |
| BoolLit | `true \| false` | Boolean literals, which are also included in the keyword set. |
| Ident | `[a-zA-Z_][a-zA-Z0-9_]*` | Identifiers must start with a letter or underscore and can contain up to 256 characters. |
| IntLit | `[0-9]+` | Unsigned decimal integer literals. |
| StringLit | `"([^"\\\n]|\\[^\n])*"` | Double-quoted strings with a maximum length of 256 characters after escape processing. |
| CharLit | `'([^'\\\n]|\\[^\n])'` | A character literal containing exactly one ASCII character or one escape sequence. |
| Operator | `++ -- && \|\| == != <= >= + - * / % ^ ! = < >` | Single-character and two-character operators. |
| Delimiter | `( ) [ ] { } , ; :` | Separators and structural symbols used in the language. |
| Invalid | Any unsupported character or unterminated lexical construct | An error is reported along with the invalid token. |

**Escape Sequences**

According to slide B-3, escape sequences are interpreted as follows:

- `\n` represents a newline character.
- `\0` represents the NUL character.
- A backslash followed by any other character represents that character itself.

For example, `\t` is interpreted as the character `t`, `\"` represents a double quote, and `\\` represents a single backslash.

## 2. Token Selection When Multiple Rules Match

In some cases, an input sequence may match more than one lexical rule. Two rules are used to resolve these situations.

**2.1. Longest Match (Maximal Munch)**

The Lexer always tries to consume the longest valid sequence of characters that matches a token pattern.

In the `lexOperatorOrDelimiter` function, two-character operators such as `<=`, `==`, `++`, and similar operators are checked first. If none of them matches, single-character operators are considered.

For example, the expression `a<=b` is split into three tokens:

`IDENT(a)`, `<=`, `IDENT(b)`

Similarly, before recognizing `/` as the division operator, the Lexer checks whether the next character forms `//` or `/*`, so comments can be recognized correctly.

Identifiers are also consumed as long as the following characters remain valid. Therefore, `returned` is recognized as one complete identifier rather than being split into smaller tokens.

**2.2. Keyword Priority Over Identifiers**

After reading a sequence that matches the identifier pattern, the Lexer checks whether it belongs to the set of reserved keywords.

For example:

- `return` is recognized as a `KEYWORD`.
- `returned` is recognized as an `IDENT`.

This approach ensures that reserved words are correctly distinguished from identifiers that simply begin with the same characters.

## 3. Design Decision: Unsigned Integer Literals

The initial Phase 1 description refers to Decimal Signed Integers. However, since `-` is also defined as a unary operator in the operator table, integer literals are treated as unsigned during lexical analysis.

The integer literal pattern is therefore:

`IntLit = [0-9]+`

The negative sign is not included as part of the integer token. Instead, it is handled by the Parser and represented as a `UnaryExpr('-')` node.

For example, the expression `-5` is tokenized into two separate tokens:

`-` and `5`

This also avoids ambiguity when distinguishing a negative number from the subtraction operator.

The implementation checks the 64-bit integer range. To support the minimum negative value, the positive magnitude corresponding to `2^63` is also considered during numeric processing, allowing `-2^63` to be represented.

## 4. Finite State Machine (DFA)

The Lexer can be viewed as a finite state machine. Each token category is recognized through a set of states and transitions.

The main states include:

- `START`: The initial state, used to determine the type of the next token.
- `IDENT`: Reads identifiers.
- `INT`: Reads integer literals.
- `STR` and `STR_ESC`: Handle string literals and their escape sequences.
- `CHR` and `CHR_ESC`: Handle character literals.
- `SLASH`: Checks whether `/` represents the division operator or the beginning of a comment.
- `LINECMT`: Reads single-line comments.
- `BLOCKCMT` and `BLK_STAR`: Process block comments and detect their closing sequence.
- `OPCHAR`: Handles operators. For example, `<` can be extended to `<=` if followed by `=`.
- `INVALID`: Represents invalid input.

The Lexer is implemented manually in `src/lexer.cpp`. Each function named `lexXxx` is responsible for recognizing a particular token category.

In most cases, the appropriate function is selected by examining the current character. For `/`, an additional lookahead is required because it may start either a comment or the division operator.

From a theoretical perspective, this implementation corresponds to the DFA obtained by combining the regular expressions. Such an automaton can be constructed using Thompson's construction followed by subset construction.

## 5. Lexical Error Handling

The Lexer does not stop when it encounters invalid input. Instead, it records the error, produces the corresponding token, and continues scanning the remaining source code whenever possible.

The behavior for different error cases is summarized below.

| Invalid Input | Lexer Behavior |
|---|---|
| Unknown characters such as `@`, `#`, `&`, `\|`, or `é` | Produces an `INVALID` token, records an error, and continues from the next character. For `&` and `\|`, the expected operators `&&` and `\|\|` are suggested. |
| Unterminated string | Produces an `INVALID` token up to the end of the line and resumes scanning from the next line. |
| Empty, multi-character, or unterminated character literal | Produces an `INVALID` token and records the corresponding error. |
| Unterminated block comment | Produces an `INVALID` token extending to the end of the file. |
| Identifier or string longer than 256 characters | The token remains valid, but a length-limit error is recorded. |
| Integer outside the 64-bit range | The numeric token is preserved, but an out-of-range error is reported. |

After processing the entire source file, the Lexer always returns an `EOF` token. This allows lexical analysis to reach the end of the input even when multiple errors are present.

## Summary

This section defines the token categories, lexical patterns, token-selection rules, integer and string handling, and lexical error recovery for Minor-B.

The Lexer is implemented manually and uses the longest-match rule together with character-based state transitions to recognize tokens. Invalid input is reported without immediately terminating the scanning process, allowing the remaining source code to be analyzed as well.

---------------------------------------------------------------------------------------------------------------------

# مشخصات لغوی زبان Minor-B (Lexical Specification)

این بخش مشخصات لغوی زبان Minor-B را توضیح می‌دهد. مبنای این مشخصات، پیوست B اسلایدهای درس و جزئیات پیاده‌سازی انجام‌شده در فاز اول پروژه است.

## ۱. دسته‌بندی توکن‌ها و الگوهای لغوی

در Lexer، متن ورودی به مجموعه‌ای از توکن‌ها تقسیم می‌شود. هر توکن بر اساس الگوی لغوی مشخصی تشخیص داده می‌شود.

| نوع توکن (`TokType`) | عبارت منظم / الگو | توضیحات |
|---|---|---|
| Whitespace | `[ \t\r\n]+` | فاصله‌ها و کاراکترهای سفید؛ حذف می‌شوند و فقط موقعیت خط و ستون را تغییر می‌دهند. |
| LineComment | `//[^\n]*` | کامنت تک‌خطی که تا انتهای خط ادامه دارد. |
| BlockComment | `/\*([^\*]|\*+[^*/])*\*+/` | کامنت چندخطی و غیرتودرتو، مشابه C. در صورت بسته نشدن، INVALID تولید می‌شود. |
| Keyword | `array boolean char else false for function if integer print return string true void while` | شامل ۱۵ کلمه رزروشده است. کلمه `while` رزرو شده، اما دستور مربوط به آن در گرامر وجود ندارد. |
| BoolLit | `true \| false` | مقادیر بولی که از نظر لغوی زیرمجموعه کلمات کلیدی هستند. |
| Ident | `[a-zA-Z_][a-zA-Z0-9_]*` | شناسه‌ها با حرف یا `_` شروع می‌شوند و حداکثر ۲۵۶ کاراکتر دارند. |
| IntLit | `[0-9]+` | اعداد صحیح ده‌دهی بدون علامت. |
| StringLit | `"([^"\\\n]|\\[^\n])*"` | رشته‌های محصور در دابل‌کوتیشن با حداکثر ۲۵۶ کاراکتر پس از اعمال escape. |
| CharLit | `'([^'\\\n]|\\[^\n])'` | مقدار کاراکتری شامل دقیقاً یک کاراکتر ASCII یا یک escape. |
| Operator | `++ -- && \|\| == != <= >= + - * / % ^ ! = < >` | عملگرهای یک‌کاراکتری و دوکاراکتری زبان. |
| Delimiter | `( ) [ ] { } , ; :` | جداکننده‌ها و علائم ساختاری زبان. |
| Invalid | سایر کاراکترهای نامعتبر یا ساختارهای لغوی بسته‌نشده | همراه با ثبت پیام خطا. |

**قواعد Escape**

مطابق اسلاید B-3، کاراکترهای escape به شکل زیر تفسیر می‌شوند:

- `\n` به معنی خط جدید است.
- `\0` به معنی کاراکتر NUL است.
- بک‌اسلش به همراه هر کاراکتر دیگری، همان کاراکتر را نمایش می‌دهد.

برای مثال، `\t` به کاراکتر `t`، عبارت `\"` به دابل‌کوتیشن و `\\` به یک بک‌اسلش تبدیل می‌شود.

## ۲. نحوه انتخاب توکن در صورت وجود چند تطبیق

در بعضی موقعیت‌ها ممکن است یک رشته با بیش از یک الگوی لغوی مطابقت داشته باشد. برای جلوگیری از ابهام، دو قاعده در Lexer استفاده می‌شود.

**۲-۱. قانون طولانی‌ترین تطبیق (Maximal Munch)**

در این روش، Lexer تا جای ممکن طولانی‌ترین رشته معتبر را به‌عنوان یک توکن در نظر می‌گیرد.

در تابع `lexOperatorOrDelimiter` ابتدا عملگرهای دوکاراکتری مانند `<=`، `==`، `++` و موارد مشابه بررسی می‌شوند و در صورت عدم تطبیق، عملگرهای تک‌کاراکتری بررسی خواهند شد.

به همین دلیل، عبارت `a<=b` به سه توکن تقسیم می‌شود:

`IDENT(a)`، `<=`، `IDENT(b)`

همچنین قبل از شناسایی `/` به‌عنوان عملگر تقسیم، ابتدا وجود `//` و `/*` بررسی می‌شود تا کامنت‌ها به‌درستی تشخیص داده شوند.

برای شناسه‌ها نیز تمام کاراکترهای مجاز تا جایی که ممکن باشد خوانده می‌شوند. بنابراین `returned` یک شناسه کامل است و به دو بخش تقسیم نمی‌شود.

**۲-۲. اولویت کلمات کلیدی نسبت به شناسه‌ها**

پس از خواندن یک رشته با الگوی شناسه، Lexer آن را با مجموعه کلمات رزروشده مقایسه می‌کند.

در نتیجه:

- `return` به‌عنوان `KEYWORD` شناخته می‌شود.
- `returned` به‌عنوان `IDENT` شناخته می‌شود.

این روش باعث می‌شود کلمات کلیدی از شناسه‌هایی که فقط با آن‌ها شروع می‌شوند، به‌درستی تفکیک شوند.

## ۳. تصمیم طراحی برای اعداد صحیح

در توضیحات اولیه فاز اول از عبارت Decimal Signed Integers استفاده شده است؛ اما با توجه به اینکه علامت `-` در جدول عملگرها به‌عنوان یک عملگر یکانی نیز تعریف شده، در پیاده‌سازی Lexer اعداد صحیح بدون علامت در نظر گرفته شده‌اند.

بنابراین الگوی عدد صحیح به شکل زیر است:

`IntLit = [0-9]+`

علامت منفی به‌صورت بخشی از توکن عددی در نظر گرفته نمی‌شود و در مرحله Parsing توسط `UnaryExpr('-')` ساخته خواهد شد.

برای مثال، عبارت `-5` به دو توکن تقسیم می‌شود:

`-` و `5`

این تصمیم از ایجاد ابهام در تشخیص عملگر تفریق نیز جلوگیری می‌کند.

محدوده اعداد صحیح ۶۴ بیتی بررسی می‌شود. برای پشتیبانی از مقدار منفی حداقل، مقدار مثبت متناظر با `2^63` نیز در مرحله پردازش عددی در نظر گرفته می‌شود تا مقدار `-2^63` قابل بیان باشد.

## ۴. ماشین حالت متناهی (DFA)

ساختار Lexer را می‌توان به‌صورت یک ماشین حالت متناهی در نظر گرفت. در پیاده‌سازی، هر نوع توکن با مجموعه‌ای از حالت‌ها و انتقال‌ها تشخیص داده می‌شود.

حالت‌های اصلی عبارت‌اند از:

- `START`: حالت اولیه که نوع توکن بعدی را مشخص می‌کند.
- `IDENT`: خواندن شناسه‌ها.
- `INT`: خواندن اعداد صحیح.
- `STR` و `STR_ESC`: پردازش رشته و کاراکترهای escape آن.
- `CHR` و `CHR_ESC`: پردازش مقادیر کاراکتری.
- `SLASH`: بررسی کاراکتر `/` برای تشخیص عملگر تقسیم یا شروع کامنت.
- `LINECMT`: خواندن کامنت تک‌خطی.
- `BLOCKCMT` و `BLK_STAR`: پردازش کامنت چندخطی و تشخیص پایان آن.
- `OPCHAR`: بررسی عملگرها، برای مثال تبدیل `<` به `<=` در صورت مشاهده `=`.
- `INVALID`: وضعیت مربوط به ورودی نامعتبر.

پیاده‌سازی Lexer به‌صورت دستی و در فایل `src/lexer.cpp` انجام شده است. هر تابع با نام `lexXxx` بخشی از فرآیند تشخیص یک نوع توکن را بر عهده دارد.

در حالت معمول، انتخاب تابع مناسب با بررسی یک کاراکتر انجام می‌شود. برای کاراکتر `/` به دلیل احتمال شروع کامنت یا عملگر تقسیم، یک نگاه اضافه به کاراکتر بعدی نیاز است.

از نظر نظری، این ساختار معادل DFA حاصل از ترکیب عبارات منظم است که می‌توان آن را با استفاده از روش ساخت تامپسون و تبدیل زیرمجموعه‌ها به دست آورد.

## ۵. مدیریت خطاهای لغوی

Lexer در برخورد با ورودی نامعتبر متوقف نمی‌شود. در عوض، خطا را ثبت می‌کند، توکن مربوطه را مشخص می‌کند و تا حد امکان به خواندن ادامه می‌دهد.

رفتار آن در حالت‌های مختلف به شکل زیر است:

| نوع ورودی نامعتبر | رفتار Lexer |
|---|---|
| کاراکتر ناشناخته مانند `@`، `#`، `&`، `\|` یا `é` | تولید توکن `INVALID`، ثبت پیام خطا و ادامه از کاراکتر بعدی. برای `&` و `\|` وجود عملگرهای `&&` و `\|\|` پیشنهاد می‌شود. |
| رشته بسته‌نشده | تولید `INVALID` تا انتهای خط و ادامه تحلیل از خط بعد. |
| کاراکتر خالی، چندکاراکتری یا بسته‌نشده | تولید `INVALID` و ثبت خطای مربوطه. |
| کامنت بلوکی بسته‌نشده | تولید `INVALID` تا انتهای فایل. |
| شناسه یا رشته بیش از ۲۵۶ کاراکتر | توکن معتبر باقی می‌ماند، اما خطای مربوط به محدودیت طول ثبت می‌شود. |
| عدد خارج از محدوده ۶۴ بیتی | توکن عددی حفظ می‌شود، اما خطای مربوط به محدوده مقدار ثبت خواهد شد. |

در نهایت، Lexer پس از پردازش تمام ورودی، توکن `EOF` را برمی‌گرداند. بنابراین حتی در صورت وجود خطاهای متعدد، فرآیند تحلیل لغوی تا رسیدن به انتهای فایل ادامه پیدا می‌کند.

## جمع‌بندی

در این بخش، قواعد تشخیص توکن‌ها، نحوه اولویت‌بندی الگوهای لغوی، پردازش اعداد و رشته‌ها و همچنین روش مدیریت خطاهای لغوی مشخص شد. پیاده‌سازی به‌صورت دستی انجام شده و با استفاده از قانون طولانی‌ترین تطبیق و بررسی حالت‌های مختلف، تلاش شده است توکن‌ها به‌درستی شناسایی شوند و در صورت وجود ورودی نامعتبر، Lexer بتواند بدون متوقف شدن به کار خود ادامه دهد.








