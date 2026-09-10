# jackdiver.com

A blog written in **jimd**, a small Markdown dialect, and built by this repository. The language is
described in [docs/jimd.md](docs/jimd.md).

## Setup

Use [bazelisk](https://github.com/bazelbuild/bazelisk): `.bazelversion` pins Bazel. After cloning:

```bash
git config core.hooksPath .githooks
```

## Commands

- Build the site, into `out/bin/www/`: `bazel build //:www`
- Serve it at <http://127.0.0.1:8000/>: `bazel run //:serve [-- PORT]`
- Test: `bazel test //...`
- Sanitisers: `bazel test //... --config=asan`
- Update the goldens, then review the diff: `bazel run //tests/golden:update` and `bazel run //tests/blog:update`
- Format: `bazel run //tools/format:clang-format` and `bazel run //tools/format:buildifier`
- Lint: `./tools/lint/lint.sh`
- Deploy: a push to `main` tests, builds and publishes the site to GitHub Pages
  ([.github/workflows/pages.yml](.github/workflows/pages.yml))

## Writing

- A post is `content/posts/YYYY-MM-DD-slug.jimd`, published at `/YYYY/MM/DD/slug/`. It has `[%title]`,
  and optionally `[%series]` and `[%description]`. Its headings start at `##`.
- A page is `content/pages/slug.jimd`. Static files go under `content/static/`.
- The rules are in [docs/jimd.md](docs/jimd.md#the-blog).

## Layout

- `include/jimd`, `src/jimd`: the jimd parser and HTML renderer
- `src/js`, `tools/js`: `js::engine`, which runs scripts compiled to QuickJS bytecode by `js_bytecode`
- `src/math`: TeX to MathML, with Temml on `js::engine`
- `src/highlight`: code to HTML, with highlight.js on `js::engine`
- `src/blog`: the `blog` tool, which builds the whole site from `content/`
- `content/`: the posts, pages and static files
- `style/`: the stylesheet
- `deps/thirdparty/`: vendored libraries
