"""Building the whole site with `blog`."""

def _blog_site_impl(ctx):
    out = ctx.actions.declare_directory(ctx.label.name)

    label = ctx.attr.content.label
    root = label.workspace_root
    if label.package:
        root = (root + "/" if root else "") + label.package
    if not root:
        root = "."

    prefix = "" if root == "." else root + "/"
    for file in ctx.files.content:
        if not file.is_source or not file.path.startswith(prefix) or \
           not file.path[len(prefix):].startswith(("posts/", "pages/", "static/")):
            fail("{}: {} is not in posts/, pages/ or static/ of {}, the package of `content`".format(
                ctx.label,
                file.path,
                root,
            ))

    ctx.actions.run(
        executable = ctx.executable._blog,
        arguments = [
            "--content",
            root,
            "--css",
            ctx.file.css.path,
            "-o",
            out.path,
        ],
        inputs = depset(ctx.files.content + [ctx.file.css]),
        outputs = [out],
        mnemonic = "BlogSite",
        progress_message = "Building the site %{label}",
        use_default_shell_env = False,
    )
    return [DefaultInfo(files = depset([out]))]

blog_site = rule(
    implementation = _blog_site_impl,
    doc = """Builds a site with `//:blog` into a directory called `name`.

The output is `<name>/` in the package's directory under the bin directory (`out/bin/` here), the
whole site: pages, `style/main.css` and the static files. It is the same for the same inputs.""",
    attrs = {
        "content": attr.label(
            mandatory = True,
            allow_files = True,
            doc = "The source files of the content directory (a filegroup of `posts/*.jimd`, " +
                  "`pages/*.jimd` and `static/**`). The package of this label is the content " +
                  "directory, so every file has to be in `posts/`, `pages/` or `static/` " +
                  "directly in that package, which the rule checks.",
        ),
        "css": attr.label(
            mandatory = True,
            allow_single_file = True,
            doc = "The stylesheet, which is written to `/style/main.css`.",
        ),
        "_blog": attr.label(
            default = "//:blog",
            executable = True,
            cfg = "exec",
        ),
    },
)
