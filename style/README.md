# Stylesheet

`main.scss` is the only file the build compiles; everything else is a partial
pulled in from it with `@use`. The order of the `@use` lines in `main.scss` is
the order of the output, so it is also the cascade.

| file | what is in it |
|---|---|
| `_scale.scss` | rhythm unit, `fluid()`, type scale, breakpoints, font stacks |
| `_palette.scss` | four colour anchors, two ramps, semantic tokens |
| `_tokens.scss` | `@forward`s the two above, emits `:root`, holds shared mixins |
| `_fonts.scss` | `@font-face` for the four self-hosted families |
| `_typography.scss` | reset, document face, headings, prose blocks, links |
| `_layout.scss` | container, article grid, navigation chrome, archive table |
| `_prose.scss` | quotes, figures, callouts, Markdown tables |
| `_code.scss` | inline code, listings, highlight.js theme |
| `_math.scss` | MathML |
| `_temml.scss` | Temml's own rules, minus what clashes with `_math.scss` and the fonts |
| `_asides.scss` | the margin rail: notes and heading permalinks |
| `_print.scss` | paper |

---

## The rhythm

Everything vertical is a multiple of one unit, `--rhythm`, which is fluid from
**27px to 30px** across the same viewport range as the type. Body copy sits on
exactly one unit of leading.

```scss
r(1)     // var(--rhythm)
r(0.75)  // calc(var(--rhythm) * 0.75)
```

The invariant: **every line-height, margin and padding in this stylesheet is a
multiple of a quarter rhythm unit**, written with `r()`. Nothing is in raw
`rem` or `px` on the block axis. Change `$rhythm` and the whole document
re-pitches; the print sheet does exactly that, to `15pt`.

Content-sized boxes — images, MathML, a table row that wraps — are the honest
exception. The grid resumes underneath them.

### Vertical margins

**A block owns only its bottom margin.** The gap above anything is whatever the
block before it left behind. Headings are the exception: they add a top margin,
because opening a section is exactly when you want more air above than below.

This is not just tidiness. `article` is a grid container, and margins between
grid items do not collapse — two facing margins would *add*. With only one of
them ever set, they cannot.

### Seeing it

```js
document.documentElement.toggleAttribute('data-grid')
```

Draws the rhythm over `main`: a strong line per unit, faint lines on the
quarters.

---

## The type scale

The leading is chosen first and the size second, which is the opposite of a
ratio scale and rather more disciplined: pick a line-height that is a clean
multiple of the rhythm, then find the size that sits well inside it at *both*
ends of the fluid range.

| role | size (360px → 1280px) | leading | ratio at each end |
|---|---|---|---|
| body | 17 → 19px | 1 unit | 1.59 → 1.58 |
| h1 | 28 → 38px | 1.5 units | 1.45 → 1.18 |
| h2 | 22 → 26px | 1.25 units | 1.53 → 1.44 |
| h3 | 19 → 21px | 1 unit | 1.42 → 1.43 |
| note (captions, margin notes) | 14 → 15px | 0.75 units | 1.45 → 1.50 |
| micro (nav, dates, labels) | 13 → 14px | 0.75 units | 1.56 → 1.61 |
| code | 13.5 → 15px | 0.75 units | 1.50 → 1.50 |

`fluid($min, $max)` builds the `clamp()`. Every fluid value in the stylesheet
interpolates across the same viewport range — **22.5rem to 80rem** — so no two
of them ever cross over partway down the page. The middle term of each `clamp()`
keeps a `rem` component, so browser zoom and a raised default font size still
move the type; a pure `vw` expression would freeze it.

The root font size is never set. `rem` means whatever the reader chose.

### Typefaces

Four families, all OFL, all self-hosted from `../font/` with no third-party
request. Attribution and licence: `../font/OFL.txt`.

- **Source Serif 4** — prose and headings. The roman build carries the optical
  size axis (`opsz` 8–60) as well as weight, because it runs from 15px captions
  to a 38px title; the italic is weight-only, since italics only ever appear at
  prose size.
- **Work Sans** — chrome only: navigation, dates, tags, captions, margin notes,
  table headers. Serif for what you read, sans for what you navigate.
- **JetBrains Mono** — code, with the italic for comments and metasyntax.
- **STIX Two Math** — maths, see below.

### Matching x-heights

Measured from each font's `OS/2` table, per 1000 em:

| face | x-height | cap-height |
|---|---|---|
| Source Serif 4 | 475 | 670 |
| STIX Two Math | 473 | 657 |
| Work Sans | 500 | 660 |
| JetBrains Mono | 550 | 730 |

JetBrains Mono's x-height is 16% larger than the body serif's, which is the
whole reason inline code always looks a size too big. Rather than hardcode a
percentage, inline `code` and `math` get:

```scss
font-size-adjust: ex-height 0.475;   // $x-height
```

The browser scales whatever face it actually ended up with until its x-height
is 0.475 of the computed size. That fixes the fallback fonts too, which a magic
percentage cannot — and there are three of them in each stack. Chrome set in
the sans targets `0.5` instead (`$x-height-sans`).

---

## The palette

**Catppuccin Latte** (light) and **Mocha** (dark), from `@catppuccin/palette`
v1.8.0. Both flavour maps are in `_palette.scss` verbatim.

Nothing outside `_palette.scss` and `_code.scss` names a flavour colour. The
rest of the stylesheet uses semantic tokens only:

| token | latte / mocha | contrast on the page |
|---|---|---|
| `$c-text` | `text` / `text` | 7.1 : 11.3 |
| `$c-text-dim` | `subtext1` / `subtext0` | 5.5 : 7.4 |
| `$c-text-faint` | `subtext0` / `overlay1` | 4.4 : 4.4 — decorative only |
| `$c-rule` | `surface1` | hairlines |
| `$c-rule-strong` | `surface2` | rules meant to be seen |
| `$c-surface` | `mantle` / `surface0` | code blocks, callouts |
| `$c-surface-raised` | `crust` / `surface1` | things on a surface |
| `$c-link` | `blue` | 5.0 : 7.8 |
| `$c-link-hot` | `mauve` | hover |
| `$c-link-line` | `overlay0` | rest-state underline |

Latte steps *down* from `base` towards `crust` for surfaces; Mocha steps *up*
into the surface ramp. Both move away from the page ground.

### Latte is refitted for contrast

Catppuccin's Latte accents are tuned to look right, not to be read at body
size. Measured against the code surface, the published values are:

```
yellow 2.15:1   pink 2.17:1   sky 2.30:1   peach 2.45:1   green 2.75:1
```

Well under the 4.5:1 small text wants. Mocha has no such problem — its accents
run 5.4–9.9:1, and only `overlay2` needed a nudge.

`$latte-fitted` holds the same colours with lightness lowered in oklch until
each clears **4.6:1** (the extra 0.1 is so rounding to 8-bit hex can't drop it
back under 4.5). Hue is preserved exactly and chroma pulled in only as far as
sRGB requires, so they still read as Latte. Each entry carries the published
value it came from in a comment.

Every code token now measures 4.59:1 or better in both flavours.

To use both flavours exactly as published and accept the lower contrast:

```scss
@use 'palette' with ($fit-latte-contrast: false);
```

### Choosing a theme

`light-dark()` resolves against the element's used `color-scheme`, so the whole
theme mechanism is that one property on `:root`:

- **nothing saved** — the stylesheet's `color-scheme: light dark`, following the
  OS, until the first click
- **light** / **dark** — `document.documentElement.style.colorScheme`, set by
  `theme.js`

The toggle has two states, light and dark, with no "auto". A click flips
whichever scheme is showing to the other and saves it to `localStorage` under
`theme`; a saved choice is never cleared. A blocking inline script in the page's
`<head>` restores the saved value before first paint — it has to be inline, or
the page paints in the wrong scheme first. The button ships `hidden` and is
revealed by JS, so it never appears as an inert control.

### `wash()`

```scss
background: wash(7%);   // color-mix(in oklab, currentColor 7%, transparent)
```

A wash of the *current text colour*. This is how nested chrome tints itself
correctly — inline code in a callout, in a margin note, in a link, in either
scheme — without a hand-authored token per combination. It replaces about
thirty static variables.

---

## Breakpoints

There are two.

```scss
$breakpoint-wide:   1088px;   // @include wide     — the margin rail opens
$breakpoint-narrow: 40rem;    // @include narrow   — two-up blocks stack
```

**1088px is where the rail opens.** Nothing outside the stylesheet reads it —
margin notes used to duplicate it in JavaScript as `WIDE`, and no longer do, so
there is only one copy to change.

`@include wide` is scoped to `screen and`, because a print viewport can be
wider than 1088px and a margin rail is meaningless on paper. `@include not-wide`
is its complement.

### The layout

```
--measure   min(66ch, 40rem)   text column, ~66 characters
--rail      3rem               gutter between column and rail
--note      15rem              the rail itself
--pad       20px → 32px        page inline padding
--page      measure + 2·pad,  or  measure + rail + note + 2·pad when wide
```

`header`, `main` and `footer` share `--page`, so above the breakpoint the
chrome runs the full width while the prose stays at its measure. `article` is a
two-track grid; below the breakpoint the second track simply does not exist.

Source Serif 4's digit advance is 0.505em, so 66ch is 33.3em — 633px at the
maximum body size, measured in the browser. The 40rem cap therefore does not
bind at default settings; it is there for the reader who has raised their
default font size, and for the fallback faces, whose digits are wider. The
`ch` measure governs the rest of the time, which is the point: the line holds
about 66 characters at every width.

#### `--measure` is a registered property

```scss
@property --measure { syntax: '<length>'; inherits: true; initial-value: 640px; }
```

`ch` resolves against whichever element *reads* the property. A margin note is
set at 15px and the prose at 19px, so an unregistered `--measure` would give
the note a narrower measure than the column it is meant to sit beside, and land
it on top of the text. Registering it as a `<length>` makes the browser resolve
`ch` once, at the element that declares it (`body`, at prose size), and inherit
the result as an absolute length.

Known gap: Firefox 120–127 supports `light-dark()` but not `@property`, and in
that band margin notes will sit too far left. Firefox 128 shipped in July 2024.

### The margin rail

Two things live in it, and both are in `_asides.scss` so their geometry stays
together.

**Margin notes.** `[^label]` in the text becomes `<sup class="note-ref">` holding
a linked number; the `[^label]:` definition becomes `<aside data-note="N">`. Both
are produced by the renderer.

Above the breakpoint there are **two layouts**, and no JavaScript in either.

Where anchor positioning is supported, each note is `position: absolute` with
`inset-block-start: anchor(start)`, pinning it level with the marker that
introduced it. The renderer writes the pairing into the markup as inline
`anchor-name` / `position-anchor` declarations, because the property needs a
dashed-ident on both halves and CSS cannot build one from an attribute — the
build already knows which note goes with which marker, so it says so. `article`
is `position: relative` inside that `@supports` block, so the inline-axis offset
is measured from the start of the measure. This is what the old `aside.js` did
with `offsetTop`, minus the JavaScript.

The fallback is a right float, pushed past the measure by a negative inline-end
margin of `--rail + --note`, with `clear: inline-end`. A float finds its own
vertical place, so it lands a note beside the block it *follows* rather than level
with its marker — measured at 58–268px lower on the current post. In exchange it
can never overlap two notes, which the anchored version can if a long note runs
past the next marker. `article` is `display: flow-root`, which both establishes
the formatting context the floats share — in separate contexts `clear` could not
see them — and contains them, so a note on the last paragraph cannot bleed into
the footer.

`aside::before` prints the note's own number out in the gutter. Cheap
reassurance, and it earns its keep in the fallback, where a note is not level
with its marker.

The marker's size and baseline offset come from the `sup` rule in
`_typography.scss`, not from here. That rule is what keeps a superscript out of
the leading; verified by removing every marker from a rendered page and finding
zero block heights changed.

Below the breakpoint the aside is a normal block with a left rule, and carries no
number — it follows its own paragraph immediately, so adjacency already says
which is which. That is also what it looks like in print.

**Heading permalinks.** `<h2><a href="#x">Text<span class="anchor">#x</span></a></h2>`.
Above the breakpoint the heading's own link becomes a two-track grid — text,
then rail — with `align-items: baseline`. That is what puts the `#slug` on the
heading's baseline with no hand-tuned offset to drift when the scale changes;
the heading text becomes an anonymous grid item, which is exactly what it wants
to be. Below the breakpoint there is nowhere to put it, so it goes away and the
heading grows a hover underline instead.

---

## Links

Two treatments, and neither of them moves anything on hover.

**Prose links** keep a permanent quiet underline and warm to `$hot`, using
`text-decoration-color`. A decoration is right here because it follows line
wraps and skips descenders, and because decorations take no part in layout at
all.

**Chrome links** — navigation, tags, footer, archive titles — use
`quiet-underline()`: `border-bottom: 1px solid transparent` at rest, opaque on
hover. A border is right *there* because it spans the whole box including its
padding, which a decoration will not, and the transparent border reserves its
own space up front so the box is identical in both states.

---

## Code

`<pre class="highlight"><code class="language-NAME">`, with `hljs-*` spans from
highlight.js, which `blog` runs at build time. Colour is only ever set on the
spans, so a block with no language, or `text`, stays on the ink colour.

The theme styles exactly the scopes the site emits, and each colour is a
`duo()` pair from the palette, so both schemes work with nothing extra. A scope
nothing emits has no rule: when a post brings a new one (C's `hljs-meta` for
`#include`, shell's `hljs-variable`) it shows up uncoloured, and the rule
goes in `_code.scss` with it.

| scope | colour (latte / mocha) | |
|---|---|---|
| `hljs-keyword` | `mauve` | |
| `hljs-type`, `hljs-title.class_` | `yellow` | |
| `hljs-built_in` | `blue` | what the language provides |
| `hljs-title` | `teal` | function names |
| `hljs-string` | `green` | |
| `hljs-number`, `hljs-literal` | `peach` | `true` and `nullptr` are constants, like numbers |
| `hljs-comment` | `subtext1` / `subtext0`, italic | prose in a code block |

`hljs-function` and `hljs-params` only wrap other spans, and have no rule. highlight.js's
class names are not stable between versions; `deps/thirdparty/README.md` says which
one is vendored.

Anything structural stays on the neutral ink, so a listing reads as text first
and colour second.

### Wide listings

Above the breakpoint a top-level `pre` (`article > pre`) is as wide as its longest
line, no narrower than the column and no wider than column, rail and note (border
box), and scrolls past that. It ends where a note ends, so it can't widen the page:
`article` takes its width from `main`, not from its children. Nested listings, and
everything below the breakpoint or on paper, keep the column.

| viewport | column | max | code size | columns in column | columns in max |
|---|---|---|---|---|---|
| 1088px | 619px | 907px | 14.7px | 65 | 97 |
| 1280px and up | 633px | 921px | 15px | 65 | 97 |

So a listing can use the notes margin, and a note placed beside a wide one is drawn
over the right-hand end of the code. The fix is in the content: move the note or the
block.

The rule only applies where notes are anchored (`@supports (anchor-name: --note)`).
The fallback notes are floats, and a scrolling `pre` is a block formatting context,
which may not overlap a float and could be pushed below it, so there the listing
keeps the column.

---

## Maths

The build renders LaTeX to MathML with Temml, so there is no JS and no maths
CSS framework — just type, plus `_temml.scss` (below).

STIX Two Math is self-hosted because it is the only face in the stack with an
OpenType `MATH` table, which is what the browser reads to size fraction bars,
stretch brackets and place limits. Without it, display equations come out
*wrong*, not merely plain. The stack ends in the CSS Fonts 4 `math` generic
keyword, not `serif`, for the same reason.

It is 400KB, but only pages containing a `<math>` element fetch it: a font is
downloaded when something on the page needs a glyph from it.

Sizing. STIX's x-height is 473/1000 against the body serif's 475 — within half
a percent, so `font-size-adjust` is very nearly a no-op for the real font and
does its real work on the fallbacks. The nudge on top is `1.02em` inline and
`1.08em` for display, and it is small on purpose: matched x-heights are not
matched *presence*, since STIX's stems are lighter and a lone italic variable
in a run of roman reads smaller than it measures.

`math[display="block"]` is `width: fit-content` with auto margins — browsers do
not centre display MathML for you and there is no wrapper element in the
generated markup to centre it with — plus `overflow-x: auto`, so a long
equation on a phone scrolls rather than bursting the layout.

Colour is inherited, so it comes through the same `light-dark()` token as body
text and dark mode is free.

`_temml.scss` is Temml's own stylesheet, `Temml-STIX2.css`, with the lines that
clash with the above taken out; the comment at the top of the file lists each
and says why. What is left is what MathML Core cannot do on its own: the classes
Temml puts on accents, array cells, enclosures and equation numbers, and the
Firefox and WebKit workarounds. To update it, compare it with the new
`Temml-STIX2.css` (`deps/thirdparty/README.md`).

`.math-error` is unused: a formula that fails to render fails the build, so no
page is written. The rule is kept, like `.site-footer`.

---

## Print

`_print.scss`. Undoing dark mode on paper is one declaration:

```scss
:root { color-scheme: only light; }
```

Every colour in the stylesheet is a `light-dark()` pair, so forcing the used
scheme resolves all of them at once. Re-pitching the document for paper is one
more (`--rhythm: 15pt`), because every vertical measurement is a multiple of it.

Navigation is hidden, prose links gain their destination in `::after`, solid
fills become hairlines, code wraps instead of scrolling, and the usual
`break-inside`/`orphans` guards are applied. Margin notes need nothing: the
rail rules are scoped to `screen`, so notes arrive on paper already in flow.
