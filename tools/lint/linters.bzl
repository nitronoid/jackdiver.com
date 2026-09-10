"""Defines linter aspects for the workspace."""

load("@aspect_rules_lint//lint:clang_tidy.bzl", "lint_clang_tidy_aspect")

# buildifier: disable=canonical-repository
clang_tidy = lint_clang_tidy_aspect(
    binary = "@@//tools/lint:clang_tidy",
    configs = [
        "@@//:.clang-tidy",
        "@@//:.clang-format",
        "@@//tests:.clang-tidy",
    ],
    lint_target_headers = False,
    angle_includes_are_system = True,
    verbose = False,
    rule_kinds = ["cc_binary", "cc_library", "cc_test"],
)
