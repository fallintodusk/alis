"""Machine-local path literals: the executable form of the Machine-Local Values
rule in docs/architecture/principles.md.

Each rule pairs a pattern with the portable spelling documentation writes
instead. validate_engine_env.py applies the patterns to every tracked file; the
public mirror (scripts/git/mirror/sanitize_public_text.py) applies them to every
published file and writes the spellings into published documentation.

Generic environment variables carry no machine identity and never match. No
pattern crosses a line break, so a whole-file substitution stays inside the
line that carries the path. A literal separator next to a literal folder name
is spelled as a one-character class, so this file's own source never matches
and needs no exemption.
"""
from __future__ import annotations

import re

# One path segment naming a user: no separator, whitespace, quote, or
# placeholder/variable marker, so "<user>" and "%USERNAME%" stay portable.
_USER = r"[^\\/\s\"'`<>%$]+"
# A Unix home path starts a token; "example.com/home/page" is a URL.
_UNIX_HOME = r"(?<![\w.])/home/" + _USER
_WSL_HOME = r"\\\\wsl\.localhost\\[^\\\s]+\\home\\" + _USER
_WIN_USER = r"[A-Za-z]:\\Users\\" + _USER
_WIN_USER_FWD = r"[A-Za-z]:/Users/" + _USER
_X86 = r"(?: \(x86\))?"

RULES = [
    (re.compile(p), portable)
    for p, portable in (
        # Maintainer repository checkouts
        (r"[A-Za-z]:[\\]+Repos_Alis[\\]+site", "<site-repo>"),
        (r"[A-Za-z]:[/]Repos_Alis[/]site", "<site-repo>"),
        (r"[A-Za-z]:[\\]+Repos_Alis[\\]+Alis", "<repo>"),
        (r"[A-Za-z]:[/]Repos_Alis[/]Alis", "<repo>"),
        (r"/mnt/[A-Za-z]/Repos_Alis[/]site", "<site-repo>"),
        (r"/mnt/[A-Za-z]/Repos_Alis[/]Alis", "<repo>"),
        (_WSL_HOME + r"\\repos_alis\\cdn", "<cdn-repo>"),
        (_UNIX_HOME + r"/repos_alis/cdn", "<cdn-repo>"),
        (r"~[/]repos_alis[/]cdn", "<cdn-repo>"),
        (r"~[/]repos_alis[/]site", "<site-repo>"),
        (r"~[/]repos_alis[/]Alis", "<repo>"),
        (r"~[/]repos_alis[/]", "$HOME/repos_alis/"),
        # Engine and tool installs
        (r"[A-Za-z]:\\UnrealEngine(?:-[0-9.]+|\\UE_[0-9.]+)", "<ue-path>"),
        (r"[A-Za-z]:/UnrealEngine(?:-[0-9.]+|/UE_[0-9.]+)", "<ue-path>"),
        (r"[A-Za-z]:\\Program Files" + _X86 + r"\\Epic Games\\UE_[0-9.]+", "<ue-path>"),
        (r"[A-Za-z]:/Program Files" + _X86 + r"/Epic Games/UE_[0-9.]+", "<ue-path>"),
        (r"[A-Za-z]:\\Program Files\\Python[0-9]+\\python\.exe", "python"),
        (r"[A-Za-z]:/Program Files/Python[0-9]+/python\.exe", "python"),
        (r"[A-Za-z]:\\Program Files" + _X86 + r"\\Windows Kits\\10\\Debuggers\\x64\\cdb\.exe",
         "<debugger-path>"),
        (r"[A-Za-z]:/Program Files" + _X86 + r"/Windows Kits/10/Debuggers/x64/cdb\.exe",
         "<debugger-path>"),
        (r"[A-Za-z]:\\Symbols", "<symbols-dir>"),
        (r"[A-Za-z]:/Symbols", "<symbols-dir>"),
        (r"[A-Za-z]:\\Builds\\[A-Za-z0-9_.-]+", "<build-dir>"),
        (r"[A-Za-z]:/Builds/[A-Za-z0-9_.-]+", "<build-dir>"),
        (r"[A-Za-z]:\\Games\\Alis", "<install-root>"),
        (r"[A-Za-z]:/Games/Alis", "<install-root>"),
        # User homes; the specific locations precede the home itself
        (_WIN_USER + r"\\AppData\\Local\\Temp\\", "%TEMP%\\"),
        (_WIN_USER_FWD + r"/AppData/Local/Temp/", "%TEMP%/"),
        (_WIN_USER + r"\\AppData\\Local\\", "%LOCALAPPDATA%\\"),
        (_WIN_USER_FWD + r"/AppData/Local/", "%LOCALAPPDATA%/"),
        (_WIN_USER, "%USERPROFILE%"),
        (_WIN_USER_FWD, "%USERPROFILE%"),
        (_WSL_HOME, "%WSL_HOME%"),
        (_UNIX_HOME, "$HOME"),
        # The sibling CDN checkout keeps its machine layout behind a variable
        (r"%WSL_HOME%[\\/]+repos_alis[\\/]+cdn", "<cdn-repo>"),
        (r"\$HOME[\\/]+repos_alis[\\/]+cdn", "<cdn-repo>"),
    )
]


def machine_path_lines(text: str) -> list[int]:
    """1-based numbers of the lines that carry a machine-local path literal."""
    return [
        number
        for number, line in enumerate(text.splitlines(), 1)
        if any(pattern.search(line) for pattern, _ in RULES)
    ]
