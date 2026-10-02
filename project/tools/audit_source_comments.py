"""自作C++の型・関数と説明コメントを構文解析して確認する。

Tree-sitterはチェック専用。ゲームのビルドには不要。
使い方はdocs/source-review-unit3/README.mdを参照する。
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "generated/source-review-unit3-tooling"))
try:
    from tree_sitter import Language, Parser
    import tree_sitter_cpp
except ImportError as error:
    raise SystemExit("監査用ライブラリを読み込めません。docs/source-review-unit3/README.mdの導入手順を確認してください。") from error

PARSER = Parser(Language(tree_sitter_cpp.language()))


def walk(node):
    """名前付きノードを深さ優先で列挙する。前方宣言は後段で除外する。"""
    yield node
    for child in node.named_children:
        yield from walk(child)


def text(node, source):
    return source[node.start_byte:node.end_byte].decode("utf-8") if node else ""


def scope(node, source):
    """入れ子の型と名前空間をたどり、同名のStateやConfigを区別する。"""
    parts = []
    parent = node.parent
    while parent:
        if parent.type in ("class_specifier", "struct_specifier", "namespace_definition"):
            name = parent.child_by_field_name("name")
            if name:
                parts.append(text(name, source))
        parent = parent.parent
    return "::".join(reversed(parts))


def leading_comments(node, source):
    """宣言直前のコメントをまとめる。型の説明を関数の説明として数えない。"""
    comments = []
    current = node
    if current.parent and current.parent.type in ("template_declaration", "declaration", "field_declaration"):
        current = current.parent
    previous = current.prev_named_sibling
    while previous and previous.type == "comment":
        comments.append(text(previous, source))
        previous = previous.prev_named_sibling
    if not comments:
        # STDMETHODCALLTYPEなどの修飾マクロでASTの開始位置がずれても、直前の行を確認する。
        line_start = source.rfind(b"\n", 0, current.start_byte) + 1
        for line in reversed(source[:line_start].decode("utf-8").splitlines()):
            if not line.lstrip().startswith("//"):
                break
            comments.append(line.strip())
    return "\n".join(reversed(comments))


def parameter_names(function, source):
    """型名や既定値を含めず、関数の引数の宣言名だけを取り出す。"""
    parameters = function.child_by_field_name("parameters")
    names = []
    if not parameters:
        return names
    for parameter in parameters.named_children:
        declarator = parameter.child_by_field_name("declarator")
        while declarator and declarator.type not in ("identifier", "field_identifier"):
            nested = declarator.child_by_field_name("declarator")
            declarator = nested or next(iter(declarator.named_children), None)
        if declarator:
            names.append(text(declarator, source))
    return names


def inventory(path):
    """コメントと文字列をコードと混同せず、説明の対象となる宣言を取得する。"""
    source = path.read_bytes().removeprefix(b"\xef\xbb\xbf")
    tree = PARSER.parse(source)
    entries = []
    seen = set()
    for node in walk(tree.root_node):
        kind = node.type
        parameters = []
        if kind in ("class_specifier", "struct_specifier"):
            if not node.child_by_field_name("body"):
                continue
            name = text(node.child_by_field_name("name"), source) or "(anonymous)"
            declaration = node
            category = "type"
        elif kind == "function_declarator":
            declaration = node.parent
            while declaration and declaration.type not in ("declaration", "field_declaration", "function_definition"):
                if declaration.type in ("parameter_declaration", "type_definition", "alias_declaration"):
                    declaration = None
                    break
                declaration = declaration.parent
            if declaration is None:
                continue
            if declaration.type != "function_definition":
                ancestor = declaration.parent
                while ancestor and ancestor.type not in ("class_specifier", "struct_specifier", "function_definition", "lambda_expression"):
                    ancestor = ancestor.parent
                if ancestor and ancestor.type in ("function_definition", "lambda_expression"):
                    continue  # 関数内の値の直接初期化を、関数宣言と数えない。
            declarator = node.child_by_field_name("declarator")
            if declarator and declarator.type in ("parenthesized_declarator", "pointer_declarator"):
                continue  # コールバックの型は、関数実装の説明対象と区別する。
            name = text(declarator, source)
            name = re.sub(r"^(?:STDMETHODCALLTYPE|WINAPI|APIENTRY|CALLBACK|__stdcall|__cdecl)\s+", "", name)
            if not name or "= delete" in text(declaration, source):
                continue
            category = "function"
            parameters = parameter_names(node, source)
        else:
            continue
        key = (category, declaration.start_byte)
        if key in seen:
            continue
        seen.add(key)
        owner = scope(declaration, source)
        qualified = "::".join(part for part in (owner, name) if part)
        body = declaration.child_by_field_name("body")
        signature = source[declaration.start_byte:body.start_byte if body else declaration.end_byte].decode("utf-8")
        comments = leading_comments(declaration, source)
        entries.append({
            "file": path.relative_to(ROOT).as_posix(), "kind": category,
            "name": name, "qualified": qualified, "line": declaration.start_point.row + 1,
            "start": declaration.start_byte, "end": declaration.end_byte,
            "documentation_start": declaration.parent.start_byte
                if declaration.parent and declaration.parent.type == "template_declaration"
                else declaration.start_byte,
            "signature": signature.strip(), "comment": comments,
            "documented": bool(re.search(r"[ぁ-んァ-ン一-龯]", comments)),
            "parameters": parameters,
            "unknown_parameters": [name for name in re.findall(r"@param(?:\[[^\]]+\])?\s+(\w+)", comments)
                if category == "function" and name not in parameters],
        })
    return source, tree, entries


def source_paths():
    """外部ライブラリと生成物を除き、ゲーム・エンジン・入口のC++を列挙する。"""
    return sorted(p for base in (ROOT / "project/DirectX/engine", ROOT / "project/game")
                  for p in base.rglob("*") if p.suffix in (".h", ".cpp")) + [ROOT / "project/main.cpp"]


def main():
    """宣言側の説明を実装側へ対応付け、実装に同じ説明を重複要求しない。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "generated/source-review-unit3-audit.json")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    entries = [entry for path in source_paths() for entry in inventory(path)[2]]
    documented_headers = {entry["qualified"] for entry in entries
                          if entry["kind"] == "function" and entry["file"].endswith(".h") and entry["documented"]}
    for entry in entries:
        if entry["kind"] == "function" and entry["qualified"] in documented_headers:
            entry["documented"] = True
    missing = [entry for entry in entries if not entry["documented"]]
    summary = {"files": len(source_paths()), "types": sum(e["kind"] == "type" for e in entries),
               "functions": sum(e["kind"] == "function" for e in entries),
               "missing_types": sum(e["kind"] == "type" for e in missing),
               "missing_functions": sum(e["kind"] == "function" for e in missing),
               "unknown_parameters": sum(len(e["unknown_parameters"]) for e in entries)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({"summary": summary, "entries": entries}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False))
    return int(args.check and (bool(missing) or summary["unknown_parameters"] > 0))


if __name__ == "__main__":
    raise SystemExit(main())
