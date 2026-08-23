#!/usr/bin/env python3
"""Check for member functions declared in headers but never defined in .cpp files.

Catches the "declared a bunch of functions but forgot to implement some" case.
Unlike the linker (which only complains when an undefined function is actually
called AND an executable is linked), this scans declarations directly, so it
also finds functions that are never called anywhere.

Usage:
    python3 build_support/check_unimplemented.py [path ...]

Each path is a file or directory; directories are scanned recursively for
.h/.hpp/.hh and .cpp/.cc/.cxx.  Defaults to plugins/Muduo/net.
Exit code is non-zero if any missing definition is found (useful as a
pre-commit hook or CMake custom target).
"""

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

HEADER_SUFFIXES = {".h", ".hpp", ".hh"}
SOURCE_SUFFIXES = {".cpp", ".cc", ".cxx"}

_COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)


def strip_comments(text):
    return _COMMENT_RE.sub("", text)


def find_class_blocks(text):
    """Yield (class_name, body_start, body_end) for each class/struct body.

    Offsets are relative to ``text``.  Nested classes are also found, which is
    fine for a checker (their declarations are looked up with the full class
    name, so a missing nested-class method is still reported).
    """
    for m in re.finditer(r"\b(?:class|struct)\s+(\w+)", text):
        # 跳过前置声明（class Foo;）：在第一个 `{` 之前先出现了 `;`
        next_open = text.find("{", m.end())
        next_semicolon = text.find(";", m.end())
        if next_open == -1 or (next_semicolon != -1
                               and next_semicolon < next_open):
            continue
        brace = next_open
        depth = 0
        i = brace
        while i < len(text):
            c = text[i]
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        yield m.group(1), brace + 1, i


def _is_pure_or_default(stmt):
    # 跳过 = 0(纯虚), = default, = delete —— 这些不需要定义
    return any(tok in stmt for tok in ("= 0", "=0", "= default", "=default",
                                       "= delete", "=delete"))


def _clean_statement(raw):
    """Drop leading access-specifier lines (public:/protected:/private:).

    Access specifiers end with ``:``, not ``;``, so they get merged into the
    head of the following statement; without this they would swallow the first
    member function after them.
    """
    parts = []
    for ln in raw.split("\n"):
        s = ln.strip()
        if not s:
            continue
        if re.fullmatch(r"(?:public|protected|private)\s*:", s):
            continue
        parts.append(s)
    return " ".join(parts)


def declared_functions(body, body_start):
    """Return {name: declared_count} and {name: first_position} for member funcs.

    Declarations are statements ending in ``;`` at the class-body brace depth;
    inline definitions (statements that close a ``{...}``) are skipped.
    """
    counts = Counter()
    first_pos = {}

    depth = 0
    stmt_start = 0
    i = 0
    while i < len(body):
        c = body[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                stmt_start = i + 1  # 内联定义结束，吞掉整条语句
        elif c == ";" and depth == 0:
            stmt = _clean_statement(body[stmt_start:i])
            stmt_start = i + 1
            if not stmt:
                i += 1
                continue
            if stmt.startswith(("class", "struct", "enum", "friend", "using",
                                "typedef")):
                i += 1
                continue
            if _is_pure_or_default(stmt) or "operator" in stmt:
                i += 1
                continue
            m = re.search(r"(~?\w+)\s*\(", stmt)
            if m:
                name = m.group(1)
                counts[name] += 1
                first_pos.setdefault(name, stmt_start)
        i += 1
    return counts, first_pos


def defined_count(class_name, func_name, cpp_texts):
    pattern = re.compile(r"\b" + re.escape(class_name) + r"::"
                         + re.escape(func_name) + r"\s*\(")
    return sum(len(pattern.findall(t)) for t in cpp_texts.values())


def line_at(text, offset):
    return text.count("\n", 0, offset) + 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", default=["plugins/Muduo/net"])
    args = parser.parse_args()

    headers = []
    sources = []
    for p in args.paths:
        path = Path(p)
        if path.is_file():
            (headers if path.suffix in HEADER_SUFFIXES
             else sources if path.suffix in SOURCE_SUFFIXES else []).append(path)
        elif path.is_dir():
            headers.extend(f for f in path.rglob("*") if f.suffix in HEADER_SUFFIXES)
            sources.extend(f for f in path.rglob("*") if f.suffix in SOURCE_SUFFIXES)
        else:
            print(f"skip: not found: {path}", file=sys.stderr)

    headers = sorted(set(headers))
    sources = sorted(set(sources))
    if not sources:
        print("no source files found", file=sys.stderr)
        return 2

    cpp_texts = {p: strip_comments(p.read_text(errors="replace")) for p in sources}
    cpp_source = "".join(cpp_texts.values())

    missing = []
    checked = 0
    for h in headers:
        raw = h.read_text(errors="replace")
        text = strip_comments(raw)
        for class_name, body_start, body_end in find_class_blocks(text):
            body = text[body_start:body_end]
            counts, first_pos = declared_functions(body, body_start)
            for name, declared in counts.items():
                checked += declared
                defined = defined_count(class_name, name, cpp_texts)
                if defined < declared:
                    missing.append((h, class_name, name, declared - defined,
                                    line_at(raw, body_start + first_pos[name])))

    missing.sort(key=lambda x: (str(x[0]), x[4]))
    if missing:
        print(f"发现 {len(missing)} 个声明但未实现的成员函数：\n")
        for header, cls, name, n, line in missing:
            suffix = f"  (缺 {n} 个重载)" if n > 1 else ""
            print(f"  [MISSING] {cls}::{name}{suffix}")
            print(f"            declared: {header}:{line}")
        print("\n共检查 {0} 个声明 / {1} 个头文件 / {2} 个源文件".format(
            checked, len(headers), len(sources)))
        return 1

    print(f"OK: 共检查 {checked} 个成员函数声明，全部已实现 "
          f"({len(headers)} 个头文件 / {len(sources)} 个源文件)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
