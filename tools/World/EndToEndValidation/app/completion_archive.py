"""Owned copies of frozen execution evidence; original receipt bytes stay intact."""
from pathlib import Path,PurePosixPath,PureWindowsPath
import json
import re

from .contracts import ValidationFailure, file_hash, read_json


def require(condition, message):
    if not condition:
        raise ValidationFailure('completion_evidence_unproven', message)


def digest(value):
    require(isinstance(value,str) and re.fullmatch('[a-f0-9]{64}',value), 'Invalid evidence digest')
    return value


def owned(repo, value):
    require(isinstance(value,str) and value, 'Missing evidence path')
    path = Path(value)
    path = (path if path.is_absolute() else repo/path).resolve()
    require(path.is_relative_to(repo.resolve()) and path.is_file(), 'Missing or foreign evidence path')
    return path


class EvidenceArchive:
    """Hash-addressed evidence inside one existing acceptance evidence directory."""

    def __init__(self, repo, root, entries=None, execution_root=None):
        self.repo, self.root = repo.resolve(), root.resolve()
        require(self.root.is_relative_to(self.repo/'Saved/Validation/WorldAuthority'),
                'Completion evidence belongs to WorldAuthority')
        self.entries = dict(entries or {})
        self.execution_root = execution_root or str(self.repo)

    def key(self, value):
        require(isinstance(value,str) and value,'Missing frozen path label')
        raw = value.replace('\\','/')
        path = PureWindowsPath(raw) if PureWindowsPath(raw).is_absolute() else PurePosixPath(raw)
        if path.is_absolute():
            origin = type(path)(self.execution_root)
            require(path.is_relative_to(origin),'Frozen path label escapes its execution root')
            path = path.relative_to(origin)
        require(path.parts and '..' not in path.parts and not path.is_absolute(),'Invalid frozen path label')
        return path.as_posix()

    def add(self, path, sha256):
        value, expected = path, digest(sha256)
        source = owned(self.repo,value)
        key = self.key(value)
        require(key not in self.entries or self.entries[key] == expected,
                'One original path has conflicting frozen identities')
        require(file_hash(source) == expected, 'Original evidence changed before retention')
        target = self.root/'payloads'/expected
        target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists():
            require(file_hash(target) == expected, 'Retained immutable evidence was corrupted')
        else:
            staging = target.with_suffix('.tmp')
            require(not staging.exists(), 'Unresolved evidence staging exists')
            staging.write_bytes(source.read_bytes())
            require(file_hash(staging) == expected and file_hash(source) == expected,
                    'Evidence changed while being retained')
            staging.replace(target)
        self.entries[key] = expected
        return self.path(value,expected)

    def add_json(self, path, sha256):
        self.add(path,sha256)
        return self.read(path,sha256)

    def path(self, value, expected):
        key, expected = self.key(value), digest(expected)
        require(self.entries.get(key) == expected, 'Receipt has no exact retained evidence binding')
        path = self.root/'payloads'/expected
        require(path.is_file() and file_hash(path) == expected, 'Retained evidence is missing or changed')
        return path

    def read(self, value, expected):
        return read_json(self.path(value,expected))

    def retain_pins(self, pins):
        require(isinstance(pins,dict), 'Receipt pin inventory is missing')
        return {name:self.add(pin['path'],pin['sha256']) for name,pin in pins.items()}

    def write(self, value):
        path = self.root/'completion.json'
        require(not path.exists(), 'A frozen completion record cannot be replaced')
        value = {**value, 'retained_evidence':self.entries,'execution_root':self.execution_root}
        path.write_text(json.dumps(value,indent=2,ensure_ascii=True)+'\n',encoding='utf-8')
        return path
