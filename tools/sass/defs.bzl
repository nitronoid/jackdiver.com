"""Compiling stylesheets with dart-sass."""

def _sass_css_impl(ctx):
    name = ctx.attr.src.label.name
    if not name.endswith(".scss"):
        fail("{}: src must end in .scss, got {}".format(ctx.label, name))
    out = ctx.actions.declare_file(name[:-len(".scss")] + ".css")

    package = ctx.label.workspace_root
    if ctx.label.package:
        package = (package + "/" if package else "") + ctx.label.package
    if not package:
        package = "."

    ctx.actions.run(
        executable = ctx.file._dart,
        arguments = [
            ctx.file._snapshot.path,
            "--no-source-map",
            "--style=expanded",
            "--load-path=" + package,
            ctx.file.src.path,
            out.path,
        ],
        inputs = depset([ctx.file.src] + ctx.files.srcs),
        tools = [ctx.file._snapshot],
        outputs = [out],
        mnemonic = "Sass",
        progress_message = "Compiling %{input}",
        use_default_shell_env = False,
    )
    return [DefaultInfo(files = depset([out]))]

sass_css = rule(
    implementation = _sass_css_impl,
    doc = """Compiles `src` to CSS with the pinned dart-sass.

The output is `src` with `.scss` replaced by `.css`, in the same directory.

Runs the `dart` binary on the snapshot directly. The `sass` script beside them finds them with
`readlink -f "$0"`, which only works by following its symlink out of the sandbox.""",
    attrs = {
        "src": attr.label(
            mandatory = True,
            allow_single_file = True,
            doc = "The stylesheet to compile.",
        ),
        "srcs": attr.label_list(
            allow_files = True,
            doc = "The files it loads (partials and the like), which are the action's other " +
                  "inputs beside `src`, so `src` is not listed again, and a partial that a " +
                  "partial loads is listed too.",
        ),
        "_dart": attr.label(
            default = "//tools/sass:dart",
            allow_single_file = True,
            cfg = "exec",
        ),
        "_snapshot": attr.label(
            default = "//tools/sass:snapshot",
            allow_single_file = True,
            cfg = "exec",
        ),
    },
)
