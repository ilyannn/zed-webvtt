# Zed extension for WebVTT and SRT subtitles. `just` lists recipes.

default:
    @just --list

# Regenerate the parser from grammar/grammar.js
[working-directory('grammar')]
generate:
    tree-sitter generate

# Every static check: justfile format, parser in sync with grammar.js, queries load
lint:
    just --fmt --check --unstable
    just generate
    git diff --exit-code -- grammar/src
    just _query highlights
    just _query injections

[working-directory('grammar')]
_query name:
    tree-sitter query --quiet ../languages/webvtt/{{ name }}.scm ../examples/*

# Offline tests: grammar corpus, and the examples parse without errors
[working-directory('grammar')]
test:
    tree-sitter test
    tree-sitter parse --quiet ../examples/*

# Lint + test: what the pre-push hook runs
check: lint test

# Rewrite files with the formatters lint checks
fmt:
    just --fmt --unstable

# Use the versioned git hooks in .githooks/ (pre-push runs `just check`)
hooks:
    git config core.hooksPath .githooks
