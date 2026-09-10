# jimd

jimd is the markup language of this repository: a small Markdown dialect, parsed by the `jimd` library
and written to HTML by `blog`. This is its reference, and the reference of how `blog` builds the site
from it. It says what the code does, in the order a document is read.

Every example below that is followed by its output was run through `blog`'s renderer, and the output
is what it wrote (the HTML fragment, with no page around it). The examples with no output only have to
parse and render without an error. Maths is not shown as MathML, which is long and is Temml's, and
code is shown without the spans `blog` adds to highlight it.

- [A document](#a-document)
- [Metadata](#metadata)
- [Blocks](#blocks): [paragraphs](#paragraphs), [headings](#headings), [quotes](#quotes),
  [code blocks](#code-blocks), [lists](#lists), [display maths](#display-maths-), [aligned maths](#aligned-maths-),
  [definitions](#definitions)
- [Inline content](#inline-content): [emphasis](#emphasis-and-strong), [code](#code-spans),
  [links](#links), [images](#images), [notes](#margin-notes), [maths](#inline-maths), [escapes](#escapes)
- [Maths](#maths)
- [Figures](#figures)
- [Typography](#typography)
- [The blog](#the-blog)
- [Errors](#errors)
- [Limits](#limits)

## A document

A document is UTF-8 text. A line ends at `\n` or `\r\n`, so a CRLF file reads like the same file with
LF (line and column in an error are the same too). A `\r` that is not before a `\n` is an ordinary
character. A byte order mark is not stripped: it is the first character of the first line, which
means the metadata of a file that starts with one is not metadata.

A document is, in order:

1. [Metadata](#metadata): `[%key]: value` lines, if any.
2. Blocks, separated by blank lines.

A blank line is a line of spaces and tabs only. The only things a document is made of are the blocks
below, and nothing in it is HTML: `<`, `>` and `&` in text are escaped, and there is no way to write a
tag.

The language is strict. Where Markdown guesses, jimd mostly reports an error with a line and a
column: an emphasis that is not closed, an escape of a character that does not need one, a link
with no definition. Columns count bytes, from 1, and the line is where the offending thing is, which for
an `unclosed_*` error is where it should have been closed (the end of the line, or of the input).
[Errors](#errors) lists all of them.

## Metadata

Metadata is a run of `[%key]: value` lines at the very top of the file. Blank lines may come before
and between them. The first line that is not metadata starts the document, and it can be the very next
line.

```jimd
[%title]: A page

[%description]: With "quotes" & <angle brackets>
Text starts here.
```

```html
<p>Text starts here.</p>
```

- The key is everything between `[%` and `]`: at least one character, none of them `]`. It is
  case-sensitive and taken as written, blanks included, so `[%title ]` is the key `title ` and sets
  no title.
- The value is the rest of the line, with the blanks around it removed. It is plain text: nothing in
  it is markup, so `[%title]: A *page*` is the title `A *page*`. The language does not smarten it
  (see [typography](#typography)). `blog` escapes it where it writes it, and smartens a title.
- A key can be given once: a second one is `duplicate_metadata`, at its key, even after a blank line.
- A line that is not complete metadata (`[%k]:` with no value, `[%k] v` with no colon, an indented
  line) is not metadata and not an error. Neither is a `[%key]: value` line after the first block, or
  in a quote. It is an ordinary paragraph line, and its markup is parsed like any other text, so
  `[%title] 2 * 3` is an `unclosed_emphasis`.
- Any key is accepted by the language. What a key does belongs to what reads it. `blog` allows
  `title`, `series` and `description` in a post, and `title` and `description` in a page, and rejects
  other keys (see [the blog](#the-blog)).

```jimd
[%title]: A
[%title]: B
```

```text
example.jimd:2:3: duplicate metadata
```

## Blocks

A block starts at the start of a line. These start one, and can interrupt a paragraph without a blank
line before them: a heading, `>` or `<` on a line of its own, a code fence, a `$$` or `$$$` fence, a list
item and a definition. Everything else is a paragraph.

### Paragraphs

A paragraph is lines of [inline content](#inline-content), up to a blank line or the start of another
block. A line end inside it is a soft break and is written as a line end in the HTML, so the browser
reflows it.

```jimd
First line,
second line.

A new paragraph.
```

```html
<p>First line,
second line.</p>
<p>A new paragraph.</p>
```

A line may start with blanks, and they are kept as text. `>` and `<` cannot be escaped (see
[quotes](#quotes)), so a paragraph line that is only one of them starts with a blank.

### Headings

One or more `#`, at least one blank (space or tab), and inline content on the same line. The
level is the number of `#`, and a level above six is written as `<h6>`. A `#` anywhere but the start of a
line is text, and so is a `#` line with nothing after its blanks, or with no blank after the `#`s
(`#tag`, `#12`).

A heading is its own permalink. Its id is its plain text with its markup removed, lower-cased,
with each run of characters other than ASCII letters and digits turned into one `-`, and `-` at the
ends dropped. `'` and `’` are dropped without ending a word, so `Let's` is `lets`. A heading with
nothing left is `section`. An id that is taken gets `-2`, `-3`, and so on, and the ids of notes
(`note-1`) come from the same set. The slug is taken from the source, so typography never changes it.

```jimd
## The boiler

## Let's *crank* it...

## The boiler

## ...
```

```html
<h2 id="the-boiler"><a href="#the-boiler">The boiler</a></h2>
<h2 id="lets-crank-it"><a href="#lets-crank-it">Let’s <strong>crank</strong> it…</a></h2>
<h2 id="the-boiler-2"><a href="#the-boiler-2">The boiler</a></h2>
<h2 id="section"><a href="#section">…</a></h2>
```

A heading that holds a link or a note reference, anywhere in it (inside emphasis too), is written
without the permalink, because an `<a>` cannot hold another:

```jimd
## See [@this](https://example.org)
```

```html
<h2 id="see-this">See <a href="https://example.org">this</a></h2>
```

Maths in a heading is allowed and its TeX is part of the slug (`## Energy $E=mc^2$` is `energy-e-mc-2`).

### Quotes

A line of `>` on its own (blanks may follow) opens a quote, and a line of `<` on its own closes it. A
quote holds blocks, other quotes too.

```jimd
>
Quoted text.

>
Nested.
<
Back in the first.
<
```

```html
<blockquote>
<p>Quoted text.</p>
<blockquote>
<p>Nested.</p>
</blockquote>
<p>Back in the first.</p>
</blockquote>
```

The markers are only recognised alone on a line: `> text` is a paragraph (with an escaped `&gt;`),
as is a line that starts with a blank before the marker. They cannot be escaped, so a paragraph that is
only `>` or `<` has to start with a blank. A `<` with no quote open is a `syntax` error, and a quote
that is still open at the end of the input is `unclosed_quote`, at the end of the input. In a quote, a
line that fits no block is `syntax`, as it is outside one.

```jimd
>
Never closed.
```

```text
example.jimd:3:1: unclosed quote
```

### Code blocks

A line of three backticks, an optional info string, and a line end opens a block. The block is every
line up to the next line that is three backticks and blanks only, and that line closes it.

````jimd
```cpp title=x
int x = 1 < 2;
```
````

This is the library's output with no code table. `blog` also highlights the block (see the last item
below).

```html
<pre><code class="language-cpp">int x = 1 &lt; 2;
</code></pre>
```

- The info string is the text after the fence, without backticks, with the blanks at its ends removed.
  The first word is the class (`language-cpp`); a block with no info string has no class.
- The body is verbatim, line ends included (CRLF becomes LF in the HTML): no markup, no escapes. Only a
  bare fence closes the block, so a fence with an info string, an indented fence and one of four or
  more backticks are body.
- A fence is only a fence when it starts a line and holds no backtick in the info string: ```` ```a`b ````
  is a `syntax` error, and so is a line of four or more backticks outside a block, so a bare fence
  cannot be shown inside a block.
- A block that is not closed before the end of the input is `unclosed_code_block`, reported at the end
  of the input.
- The library can take highlighted HTML for a block, by its language and body (`code_requests()` lists
  the blocks that have a language). A block that has an entry is written as
  `<pre class="highlight"><code class="language-cpp">` and the entry as it is, unescaped, and any other
  block as above. `blog` passes one (see [the blog](#the-blog)).

### Lists

A list item is `-` in the first column, one or more blanks, and [inline content](#inline-content) to
the end of the line. Items on consecutive lines are one list, and any other line ends it, a blank
line included, so two lists with a blank line between them are two lists. Items are one line, unordered
and cannot nest: a line after an item that is not an item is a paragraph.

```jimd
A paragraph line, and the list starts right after it:
- one
- _two_ and *three*

- a second list
```

```html
<p>A paragraph line, and the list starts right after it:</p>
<ul>
<li>one</li>
<li><em>two</em> and <strong>three</strong></li>
</ul>
<ul>
<li>a second list</li>
</ul>
```

A `-` that is not followed by a blank (`-5`, `--`), a `-` with nothing but blanks after it, and a `-`
that is not in the first column are text. `\-` is a literal dash, for a paragraph line that has to start
with `- `.

```jimd
-5 degrees
-
\- not a list
```

```html
<p>-5 degrees
-
- not a list</p>
```

### Display maths (`$$`)

A line of `$$` (blanks may follow) opens a block and the next such line closes it. The body is the
TeX of one display equation, verbatim and line ends included. A `$$$` line is not a `$$` fence, and
inside a `$$` body it is part of the TeX. See [maths](#maths) for what the TeX may be, and
[inline maths](#inline-maths) for `$x$`.

```jimd
An equation comes next:
$$
\int_0^1 x^2 \, dx = \frac{1}{3}
$$
and the text goes on.
```

A fence can follow a paragraph line, and any block, a definition included, can follow the closing
fence. A body with nothing but blanks and blank lines is `empty_maths`, at the opening fence, and a
block that is still open at the end of the input is `unclosed_math_block`.

```jimd
$$

$$
```

```text
example.jimd:1:1: empty maths
```

### Aligned maths (`$$$`)

The same, with `$$$` fences, for several rows aligned on `&`. Every line of the body is a row, and
the rows become an `align*` environment (the `\begin{align*}` and `\end{align*}` and the `\\` between
rows are added, and not written).

```jimd
$$$
f(0) &= \frac{dT}{dt} \cdot \frac{\tau}{K}
     &= f_{steady} \cdot \frac{\tau}{\tau_{fast}}
$$$
```

- A row is its line as written, blanks at either end included. Blank lines before the first row and
  after the last are not rows. A **blank line between rows is an empty row**, the spacing a
  derivation sometimes wants.
- A row cannot end in `\\`, as the join adds one and a second would make an empty row: that is
  `math_row_break`, at the start of the row. It looks at the TeX before a `%` comment, trailing blanks
  dropped, and a run of two or more backslashes counts. A `\\` inside a row is fine and makes another row.
- A row cannot open or close an environment the next row finishes: a row whose count of `\begin{` and
  `\end{` differ is `math_environment_spans_rows`, at the start of the row. A multi-line `aligned` has to
  be written as rows with `&`, as a nested one renders wrong without an error.
- A `%` starts a TeX comment that runs to the end of the row, and it cannot swallow the row break
  that follows: the break is on a line of its own.
- **A blank row between rows that have `&` is written as `&` alone.** A blank row has no `&`, so it has
  one cell where the others have two, and the vendored Temml 0.14.0 writes a junk `style` attribute
  (`level:0;color:undefined;…`) on the cell it pads. It is harmless, but it is a Temml bug and a lone `&`
  gives both cells and no junk. In a block with no `&` at all, a blank line is fine.

```jimd
$$$
a &= b + c
&
d &= e
$$$
```

```jimd
$$$
x = 1

y = 2
$$$
```

An empty body is `empty_maths` and an unclosed block is `unclosed_math_block`, as for `$$`.

```jimd
$$$
x = 1 \\
$$$
```

```text
example.jimd:2:1: maths row ends in a row break
```

```jimd
$$$
\begin{aligned}
x &= 1
\end{aligned}
$$$
```

```text
example.jimd:2:1: maths environment spans rows
```

### Definitions

A definition is a line that starts in the first column with a key in brackets and a colon, then
content to the end of the line. It names something that [links](#links), [images](#images) and
[notes](#margin-notes) refer to by key. It can follow a paragraph line directly, does not need to come
before its use, and is not written to the page where it stands.

| Definition | Content | Used by |
|---|---|---|
| `[@key]: url` | A target, as written, to the end of the line | `[@key]` |
| `[!key]: url` or `[!key]: url 75%` | A target and an optional [width](#figures) | `[!key]` |
| `[^key]: text` | [Inline content](#inline-content), one line | `[^key]` |

The keys of the three kinds are separate. A key is compared as written (case and blanks count), a
definition needs something after its colon (`[@k]:` alone is a paragraph), and a key defined twice is
`duplicate_definition`, at the second key. A key has at least one character that is not a blank: an empty
one, in a definition or a bare `[@key]`, `[!key]` or `[^key]`, is `empty_key`, at the `]`. A bare
reference is to a definition only, never to an inline resource: a key that is used and never defined
is `undefined_reference`, at the key, whatever inline resources have the same text. A note that is
defined and never used is not written, but its maths is still checked.

```jimd
A [@link] and a [@link] again, and an [@inline](https://o.org).

[@link]: https://example.org/page
```

```html
<p>A <a href="https://example.org/page">link</a> and a <a href="https://example.org/page">link</a> again, and an <a href="https://o.org">inline</a>.</p>
```

```jimd
[@missing]
```

```text
example.jimd:1:3: undefined reference
```

```jimd
[@]: https://example.org
```

```text
example.jimd:1:3: empty key
```

The target of a link or image definition is the text from its first character that is not a blank to
the end of the line, so a trailing blank is part of it (an image width may be followed by blanks, and
they are dropped): write the target with none.

## Inline content

Inline content is what paragraphs, headings, list items and notes hold: text, emphasis, code, links,
images, note references, maths and escapes. Every `_`, `*`, `` ` ``, `\`, `$` and `[` followed by `@`, `!`
or `^` starts something, and everything else is text. A `[` that is not followed by one of the three,
and a `]`, are text.

### Emphasis and strong

`_text_` is emphasis (`<em>`) and `*text*` is strong (`<strong>`). Each closes on its own line, cannot
be empty, and holds text, escapes, code, links, images, notes and maths. They nest in each other, and
never in themselves: inside a `_…_` the next `_` is the end.

```jimd
_italic_, *bold*, and _a *bold _inner_ part* of it_.
```

```html
<p><em>italic</em>, <strong>bold</strong>, and <em>a <strong>bold <em>inner</em> part</strong> of it</em>.</p>
```

- An emphasis that is not closed by the end of its line is `unclosed_emphasis`, at the end of the line.
- `__`, `**` and the like are `empty_emphasis`, at the first marker (`2 ** 3` is one: write `2 \*\* 3`).
  `_ _` is not empty.
- A `_` between two ASCII letters or digits is part of the word: `snake_case` and `10_000` are text.
  It is `_` only: a `*` is always a marker, and a letter such as `é` is not a word character, so
  `café\_au\_lait` needs its escapes. For the same reason `_foo_bar` is an `unclosed_emphasis`: the
  closing `_` is inside a word.
- Crossing pairs (`_a *b_ c*`) are `unclosed_emphasis`.

```jimd
snake_case and 10_000 are text, but snake _case is not
```

```text
example.jimd:1:55: unclosed emphasis
```

### Code spans

Backticks around text: `` `x < y` `` is `<code>`. The text is verbatim, with no escapes, emphasis or maths in it, and
it ends at the next backtick on the line. There is no way to put a backtick in one. A backtick that
is not closed on its line is a `syntax` error.

```jimd
Use `a * b` and `$x$` and `\q` as they are.
```

```html
<p>Use <code>a * b</code> and <code>$x$</code> and <code>\q</code> as they are.</p>
```

### Links

`[@text](url)` is a link whose text is `text`, and `[@key]` is a link to the definition of `key`,
whose text is the key. The two are separate: an inline link carries its own target and defines nothing,
so equal texts may have different targets, and `[@text]` is never a link back to one (it is the
[definition](#definitions) of `text`, or `undefined_reference`). The text is the key, and is raw:
`[@a *b*](u)` shows `a *b*` and has no markup in it. (Typography still applies.)

- The target is everything up to the closing `)` on the same line, as written: no title syntax and no
  escapes, blanks included, and `[@a]()` is a link to nothing.
- One level of balanced parentheses is part of the target (`[@a](Foo_(bar))` has the target
  `Foo_(bar)`). Two levels are not: `[@a](x(y(z)))` is the target `x(y(z)` and a `)` of text. A single
  `(` has to be written `%28`, and there is no escape for a target.
- A `[@` with no `]`, and a `(` target with no `)`, on the line, are `unclosed_resource`, at the end of
  the line. A space between the `]` and the `(` means there is no target.
- The text may not be empty: `[@](u)`, or one with nothing but blanks, is `empty_link`, at the `]`.

```jimd
[@Foo](https://en.wikipedia.org/wiki/Foo_(bar)) and [@Foo](/other) and [@Foo] and [@a](x(y(z)))

[@Foo]: https://example.org/foo
```

```html
<p><a href="https://en.wikipedia.org/wiki/Foo_(bar)">Foo</a> and <a href="/other">Foo</a> and <a href="https://example.org/foo">Foo</a> and <a href="x(y(z)">a</a>)</p>
```

```jimd
[@a](u
```

```text
example.jimd:1:7: unclosed resource
```

### Images

`[!alt](url)` is an image, and `[!key]` uses a [definition](#definitions) and the alt text is the key.
The target may end in a width: see [figures](#figures). The alt text is raw, like the text of a link.

An inline image carries its own target and defines nothing, so equal alt texts may have different
targets, and `[!alt]` is never a reference to one. An inline image may have no alt text: `[!](x.png)` is
`alt=""`, a decorative image. A definition or a bare reference may not: `[!]: x.png` and `[!]` are
`empty_key`, at the `]`.

```jimd
[!A diagram](/picture.png 60%) and [!A diagram](/other.png) and [!A diagram]

[!A diagram]: /defined.png
```

```html
<p><img src="/picture.png" alt="A diagram" style="width: 60%"> and <img src="/other.png" alt="A diagram"> and <img src="/defined.png" alt="A diagram"></p>
```

### Margin notes

`[^key]` refers to a note defined with `[^key]: text`, and `[^key](text)` writes the note where it is
used. The text is one line of inline content and ends, for the inline form, at the first `)` that is
not escaped (`\)`), not in a code span, and not inside a link, image, emphasis or maths of its own, so
it can hold those. A note cannot hold another note: that is `nested_footnote`, at the key of the inner
one. An inline note keeps its key to help find it in a long file, and nothing else is done with it: it is
not written, not unique and not referable. Each inline note is a note of its own, whatever its key
(`[^](text)` has an empty one), and `[^key]` is only ever a reference to the definition of `key`. A
definition or a bare reference with an empty key, `[^]: text` or `[^]`, is `empty_key`, at the `]`.

```jimd
A claim[^a], a second[^b](a note written *here*), and the first again[^a].

Next paragraph.

[^a]: The first note.
```

```html
<p>A claim<sup class="note-ref" style="anchor-name: --note-1"><a href="#note-1" id="noteref-1">1</a></sup>, a second<sup class="note-ref" style="anchor-name: --note-2"><a href="#note-2" id="noteref-2">2</a></sup>, and the first again<sup class="note-ref"><a href="#note-1">1</a></sup>.</p>
<aside id="note-1" data-note="1" style="position-anchor: --note-1">
<p>The first note.</p>
</aside>
<aside id="note-2" data-note="2" style="position-anchor: --note-2">
<p>a note written <strong>here</strong></p>
</aside>
<p>Next paragraph.</p>
```

- Notes are numbered by their first use, in the document. A later use of a defined note links to the
  first and has no id. An inline note is always a new note, with the next number.
- A note is an `<aside>` written after the top-level block that holds its first reference, so after the
  whole quote, list or heading if the reference is in one. The stylesheet puts it in the margin,
  level with its marker. The number is drawn from `data-note`.
- The ids are `note-N` and `noteref-N`, from the same set as the headings' ids, so a heading
  `note 1` written before the first note makes it `note-1-2`. The marker and its aside are paired by the number in
  `anchor-name` and `position-anchor`, which does not change.

### Inline maths

`$tex$`. A `$` always starts maths, so a dollar sign in text is `\$`. The TeX is every character up to
the next `$` on the same line, verbatim: no escapes, emphasis or links inside, and blanks around it are
kept. See [maths](#maths) for the rest.

```jimd
It costs \$5, and $x^2$ is the square.
```

- Maths with nothing but blanks in it, `$$` or `$ $`, is `empty_maths`, at the first `$`. (A line that
  is only `$$` is the fence of a [display block](#display-maths-).)
- Maths that is not closed by the end of the line is `unclosed_math`, at the end of the line.
- It is allowed wherever inline content is: in a heading, a list item, a note, emphasis (`_a $x_1$ b_`,
  where the `_` in the TeX does not end the emphasis) and an inline note (`[^n](see $f(x)$)`, where the
  `)` does not end the note). It is not in the text of a link or image, which is raw.
- A `$` in a code span, a link target, a definition or metadata is not maths.

```jimd
A $x
```

```text
example.jimd:1:5: unclosed maths
```

```jimd
Between $$ nothing
```

```text
example.jimd:1:9: empty maths
```

### Escapes

A backslash and one of ``\ * _ ` [ # ) - $`` is that character as text. Any other character after a
backslash, a backslash at the end of a line, and a non-ASCII character are `invalid_escape`, at the
backslash. `]`, `(`, `<`, `>`, `.`, `'` and `"` need no escape or have none. Escapes are not read in a
code span, a code block, maths, link or image text, a target, a definition target or metadata. (They are in
emphasis, headings, items and notes.)

```jimd
\* \_ \` \[ \# \) \- \$ \\
```

```html
<p>* _ ` [ # ) - $ \</p>
```

```jimd
C:\dir
```

```text
example.jimd:1:3: invalid escape
```

## Maths

Maths is rendered to MathML by [Temml](https://temml.org) (version 0.14.0), which is built into the
program: no Node and no JavaScript on the page. The TeX goes to Temml as it is written (inline and
`$$`), or as an `align*` with the rows (`$$$`). TeX that Temml rejects is `invalid_math`
(`example.jimd:1:2: invalid maths: Unsupported function name: \foo`), at the place Temml gives up,
and the first such error is reported. Which commands exist is Temml's:
[its function support list](https://temml.org/docs/en/supported) is the reference.

What to know in jimd:

- **A dollar sign.** A `$` ends inline maths, so `\$` cannot be written there. Temml supports
  `\textdollar` in text mode only: write `\text{\textdollar}5` (or `\char"24`). In a `$$` or `$$$`
  body, which has no closing `$` to confuse, a plain `\$` works.
- **A percent sign.** `%` starts a TeX comment that runs to the end of the TeX (for inline maths,
  the closing `$`), so `$50% of x$` is `50`. Write `\%`.
- **Positions.** An error is at the place Temml gives, mapped back to the file: a character of the
  inline TeX, of the `$$` body, or of a `$$$` row. When Temml gives no place, it is at the start of the TeX
  (the first row of a `$$$` block). An unclosed `{` in the last row is reported at the end of that row.
  In an earlier row Temml reads on into the next one, and gives up there, at its `&`. When a token
  comes from a macro (`\def`), Temml's place is relative to the macro, and may point at the wrong place
  in your TeX.
- **Blank rows and `&`** in `$$$`: see [aligned maths](#aligned-maths-).
- Equal TeX is rendered once per document. `$$$` is an `align*`, so its rows are not numbered.

```jimd
$\foo$
```

```text
example.jimd:1:2: invalid maths: Unsupported function name: \foo
```

```jimd
$\text{\textdollar}5$ and $x = \char"24$ and 50% in text is $50\%$ in maths.
```

```jimd
$$
x = \$5
$$
```

```jimd
$50% of x$
```

## Figures

A paragraph that is only one image (blanks around it are fine) is a **figure**: it is not wrapped in
a `<p>`. What it is drawn as depends on the target.

A **width** ends an image target, inline or defined: a last word that is an integer from 1 to 100
and `%`, after a blank, with something before it: `[!alt](/x.svg 75%)`, `[!alt]: /x.svg 75%`. It
is the width of the image, kept as written (`007%` is `007%`). `0%`, `101%` and longer are
`invalid_width`, at the first digit. `7.5%`, `75 %`, `+75%` and a word with nothing before it (`[!a](75%)`)
are part of the target. A defined image has its definition's width, and an inline one its own. A
width goes with an image and nothing else.

A figure whose target ends in `.svg` (exactly, in lower case: `.SVG` and `x.svg?v=1` are not) is drawn
as a mask, sized by the `viewBox` of the file:

```jimd
[!A diagram](/image/diagram.svg 75%)
```

```html
<div class="ink-mask" role="img" aria-label="A diagram" style="--src: url('/image/diagram.svg'); --ratio: 300/200; --width: 75%"></div>
```

An empty alt writes `aria-hidden="true"` in place of `role` and `aria-label`, and no width writes no
`--width`. Any other image, a PNG figure included, and an SVG image that shares its paragraph with text
or other images, is an `<img>` in a `<p>`. A list item or a note is not a paragraph, so an image alone in
one is an `<img>`. A figure in a quote is a figure.

How an SVG figure is read:

- **The target is a path under the static root**, not a URL, and has to be the same thing as both. It
  starts with a single `/` and has no empty, `.` or `..` segment and no `?`, `#`, `%`, `\` or control
  character. Anything else is `image_not_local`, at the image, with the reason (`does not start with /`,
  `starts with //`, `has an empty segment`, `has a . or .. segment`, `has a ?, # or %`, `has a \ or a
  control character`). The file is `<static root>/<target without its first />`, and the static
  root is `content/static`. A symbolic link there is followed.
- **The ratio** is taken from the file's `viewBox`: four plain decimals (no exponent), the third and
  fourth positive, in the first `<svg>` tag outside a comment. It is written `W/H` as read, without
  signs, leading zeros or trailing zeros after a point; blanks, a comma or both may separate the
  four. A file without a usable one is `image_size`.
  A file that is not there is `file_not_found`, and one that cannot be read is `read_failed`. All are
  at the image: the detail of the first is the target and the reason (`/image/x.svg: no viewBox in the
  <svg> tag`), and of the other two the target.
- **`'`, `(`, `)` and a space** in a figure target are `invalid_target`, at the first of them, once
  the file has been read: it is the check on what goes in `url('…')`. (The library refuses `\` and
  control characters there too, but `blog` has refused them already, as `image_not_local`.)
- A figure that is not an SVG figure is not looked for: it needs no file.

```jimd
[!x](x.svg)
```

```text
example.jimd:1:3: figure target is not a local path: x.svg: does not start with /
```

```jimd
[!x](/image/diagram.svg 0%)
```

```text
example.jimd:1:25: invalid image width
```

```jimd
[!x](/image/missing.svg)
```

```text
example.jimd:1:3: file not found: /image/missing.svg
```

```jimd
[!x](/image/it's.svg)
```

```text
example.jimd:1:15: invalid figure target
```

## Typography

Always on, for the text of paragraphs, headings, list items, notes and link text, and nowhere else:
code spans, code blocks, maths, an image's alt text, targets and ids are untouched.

- `...` becomes `…` and `--` becomes `—`, from left to right: `....` is `….` and `---` is `—-`.
- `'` and `"` become curly. One opens (`‘` `“`) when the character before it is the start of the
  block, item, heading or note, a blank, a line end, or one of `( [ { / - ‘ “ — –`, and closes
  (`’` `”`) otherwise. The character it goes by is the last one written, so `--"a"` opens and
  `..."a"` closes. Emphasis and strong do not count as characters (`_"What's up?"_` opens), and a code
  span, image, maths or note reference counts as a letter (so `$x$"` closes).
- Each piece of text is smartened on its own, so what an escape or a `<em>` splits is not joined: `-\-`
  stays `--`.
- The output is UTF-8 characters, not entities.

```jimd
"Hello," she said. It's 'quoted' -- really... ok --- hm.
```

```html
<p>“Hello,” she said. It’s ‘quoted’ — really… ok —- hm.</p>
```

Limitations, all by design and pinned by tests:

- `'90s` becomes `‘90s`, and `rock 'n' roll` becomes `rock ‘n’ roll`, since a `'` after a blank opens.
- `''` and `""` are not a pair: the second goes by the first, so after a blank both open (`‘‘`) and
  after a word both close (`””`).
- A quote after `=` closes, so `a="b"` is `a=”b”`, and `2"` is `2”`.
- There is no escape for a straight `'` or `"` (`\'` is `invalid_escape`) and none for `.` or `-`
  except `\-`: a code span shows them as they are.
- A slug is made from the source text: `Let's` is `lets` whether or not the curly quote is typed.

## The blog

`blog` builds a whole site from a content directory, which is `content/` here:

```text
content/
├── posts/YYYY-MM-DD-slug.jimd
├── pages/slug.jimd
└── static/…
```

Each of the three can be missing. In `posts/` and `pages/` a name that starts with `.` is not an
entry, and any other entry that is not a `.jimd` file (`notes.txt`, a directory) is a bad name.

**Posts.** `content/posts/YYYY-MM-DD-slug.jimd` is published at `/YYYY/MM/DD/slug/`, with no other
change.

- The name is a real date (`2026-02-30` is not one), then a slug of lower-case letters and digits
  joined by single hyphens, then `.jimd`.
- The [metadata](#metadata) has `title` (required), `series` and `description`. The date comes from the
  name, and the site's title (Jack Diver) is added to the page's, so neither is written.
- The title is the page's `h1`, so the body starts at `##`. `blog` does not check this.
- Posts are listed newest first. On one day the later slug comes first.
- Posts that share a `series` form a series, oldest first. Once a series has two posts, each of them
  shows the list of its parts and links to the previous and next part. A series of one shows nothing.
  Every post also links to the previous and next post by date.

**Pages.** `content/pages/slug.jimd` is published at `/slug/`, with `title` (required) and
`description`. Pages are linked from the header, sorted by slug. A page may not be called `archive`
or four digits, which are paths the site writes (`page collides with a generated path`).

**Static files.** Everything under `content/static/` (but a `.DS_Store`) is copied as it is, to the same
path under the site: fonts, `favicon.svg`, `theme.js` and the images. An SVG figure is read from there
for its `viewBox` (see [figures](#figures)).

**Code.** A code block with a language is highlighted by highlight.js at build time: it is written as
`<pre class="highlight"><code class="language-cpp">` and the code with `<span class="hljs-…">` around
its tokens. The language is a name or alias of one of highlight.js's
[languages](../deps/thirdparty/README.md#highlightjs), in any case, and is never guessed. One it doesn't
know is an error (`cannot highlight code: unknown language`), so a typo can't lose the highlighting
quietly. `text` is accepted: its code is only escaped, though the block still has `class="highlight"`.
A block with no info string is not highlighted.

**What is written.** Each post and page at its URL, `/index.html` (the newest post in full, and a link
to the archive), `/archive/index.html` (every post by year), `/404.html` (the page a host such as
GitHub Pages shows for a missing URL, so every link in a page starts at the root), `/style/main.css`,
and the static files.
Nothing in them depends on the time or on the machine: building twice gives the same files.

**The command line:**

```text
blog --content <dir> --css <file> -o <dir>
```

Each option is given once, in any order. `--content` is the content directory, `--css` the stylesheet,
which is written to `/style/main.css`, and `-o` the output directory, which has to be missing or empty.
Any other command line prints the usage above and exits with 1. `bazel build //:www` runs it over
`content/` and `//style:main_css`.

## Errors

An error stops the parse: the first one is reported. Positions are `line:column`, 1-based, columns in
bytes. A message is `path:line:col: description`, with `: detail` after it for the errors that have
one, and `path: description` for an error about a file as a whole. `blog` prints it on
standard error and exit with 1.

### Errors of the language (`jimd::errc`)

| Message | When | Position |
|---|---|---|
| `syntax error` | Text that is not any construct: a backtick that is not closed on its line, a fence line with a backtick in its info string, a line of four or more backticks, a `<` with no quote open, a line that fits no block inside a quote | The first character that cannot be read |
| `unclosed quote` | The input ends inside a quote | The end of the input |
| `unclosed code block` | The input ends inside a code block | The end of the input |
| `unclosed emphasis` | `_` or `*` that is not closed on its line, or crossing pairs | The end of the line |
| `empty emphasis` | `__`, `**` and the like | The first marker |
| `unclosed resource` | `[@`, `[!` or `[^` with no `]`, or a `(` target or note text with no `)`, on the line | The end of the line |
| `invalid escape` | A backslash before a character that is not escapable, or at the end of a line | The backslash |
| `duplicate definition` | A key defined twice, in definitions of the same kind | The key of the second |
| `undefined reference` | A `[@key]`, `[!key]` or `[^key]` with no definition (an inline resource is not one) | The key |
| `empty key` | A definition or a bare `[@key]`, `[!key]` or `[^key]` whose key is nothing or only blanks | The `]` |
| `empty link` | An inline `[@text](url)` whose text is nothing or only blanks | The `]` |
| `nested footnote` | A note that holds a note reference | The key of the inner one |
| `duplicate metadata` | A metadata key given twice | The key of the second |
| `unclosed maths` | `$` with no closing `$` on its line | The end of the line |
| `unclosed maths block` | The input ends inside a `$$` or `$$$` block | The end of the input |
| `empty maths` | `$$` or `$ $` inside a line, or a `$$` or `$$$` block with only blanks | The first `$`, or the opening fence |
| `maths row ends in a row break` | A `$$$` row that ends in `\\` | The start of the row |
| `maths environment spans rows` | A `$$$` row with unequal `\begin{` and `\end{` | The start of the row |
| `invalid image width` | A width of `0%`, or over `100%` | The first digit of the width |
| `invalid figure target` | An SVG figure target with a space, `'`, `(` or `)`, after its file has been read (the library also refuses `\` and control characters, which `blog` has already refused) | The first such character |
| `unrendered maths` | `jimd::html::render` was called without the MathML of some maths | The TeX |
| `unknown image size` | `render` was called without the ratio of an SVG figure | The image |

The last two cannot come from `blog`, which fills both tables first. They are for a program that
calls the library.

### Errors of `blog` (`blog::errc`)

| Message | When | Place |
|---|---|---|
| `file not found` | A file that is not there (an SVG, a `--content` or `--css`) | The file; for an SVG, the image |
| `read failed` | A file or directory that cannot be read, a directory where a file is wanted, a `posts/`, `pages/` or `static/` that is not a directory, a symlink to a directory in `static/` (it is not followed) | As above |
| `write failed` | A file that cannot be written | The file |
| `no usable image size` | An SVG figure whose file has no usable `viewBox`, with the target and the reason | The image |
| `invalid maths` | TeX that Temml rejects, with its message | Where Temml gives up |
| `cannot highlight code` | A code block whose language highlight.js does not know (`unknown language`), or that it fails on, with its message | The language |
| `figure target is not a local path` | A figure target that is not a path under the static root, with the target and the reason | The image (its alt text, so column 3 for an image at the start of a line) |
| `bad post file name` | A post whose file is not `YYYY-MM-DD-slug.jimd`, or is not a real date, or is not a file (`not a file`), with the reason | The file |
| `bad page file name` | A page whose file is not `slug.jimd`, or is not a file, with the reason | The file |
| `missing title` | A post or page with no `[%title]` | The file |
| `unknown metadata key` | A key a post or page does not have (the first in the file), with the key and what it has | The key |
| `page collides with a generated path` | A page called `archive`, or four digits, whose URL the site writes itself | The file |
| `usage: blog --content <dir> --css <file> -o <dir>` | A bad command line | None |
| `output directory is not empty` | `-o` is a directory with anything in it, or a file | The directory |
| `static file collides with a file the site writes` | A file of `static/` at the path of a file the site writes, a directory of one, or inside one | The static file |

`blog` reads and renders every post and page, and reports every fault of the content one per line,
sorted by path, as `path:line:col: description` (or `path: description`). If there is any error it
writes nothing, not even the output directory, and exits with 1. An `output directory is not empty`,
or a `--content` that is not a directory (`file not found`), is the only error then: nothing else is
looked at.

`file not found` and `read failed` have no detail except for a figure, where it is the target. A
`write failed` can leave the files written before it. A `static file collides…` is the one fault
that is not always reported: it is looked for only when no post or page has a bad name, lacks a
title, has an unknown key or has a reserved slug, because the files the site writes are not known
before. A file has at most one error, so a `.jimd` file that does not parse is reported for that,
not for its name; a file that is not a `.jimd` file is a bad name and is not read.

```jimd
$\foo$
```

```text
example.jimd:1:2: invalid maths: Unsupported function name: \foo
```

## Limits

- **Nesting has no limit.** Each level of emphasis, quote or note is a recursive call, so thousands of
  levels (about 8,000 of `_*_*…` on one line, or 5,000 to 10,000 nested quotes) overflow the stack.
- **Nothing is HTML.** There is no raw HTML, no tables, no ordered or nested lists, no hard line
  break, no title on a link, and no way to write `<` or `>` alone on a line in a paragraph without a
  blank before it.
- **Positions are bytes**, not characters, and a line's number counts `\n`.
