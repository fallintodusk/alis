"""Owner-declared World producer descriptors, read for proof routing.

A producer is keyed by generator_id plus generator_version. Its owning plugin declares
it in Data/Producers/<generator_id>.json and keeps its verify baseline in
Data/TestFixtures/Verify/<generator_id>.verify.json; the shared realization pipeline
uses the stem realization_pipeline. The descriptor schema owns validation. This reader
takes only the fields routing consumes and fails closed on anything it cannot read, so
nothing is silently skipped.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any


PIPELINE_ID = "realization_pipeline"
PIPELINE_DESCRIPTOR = f"Plugins/World/ProjectWorld/Data/Producers/{PIPELINE_ID}.json"
VERIFY_PREFIX = "Project.World.Realization.Verify."


class ProducerDescriptorError(RuntimeError):
    pass


@dataclass(frozen=True)
class ProducerDescriptor:
    path: str
    generator_id: str
    # None for the pipeline descriptor, which has no generator version.
    generator_version: int | None
    modules: tuple[str, ...]
    data_inputs: tuple[str, ...]

    @property
    def is_pipeline(self) -> bool:
        return self.generator_version is None

    @property
    def key(self) -> tuple[str, int | None]:
        return (self.generator_id, self.generator_version)

    @property
    def label(self) -> str:
        """The manifest producer id <generator_id>:v<generator_version>."""
        return PIPELINE_ID if self.is_pipeline else f"{self.generator_id}:v{self.generator_version}"

    @property
    def baseline_path(self) -> str:
        plugin_root = PurePosixPath(self.path).parents[2]
        return f"{plugin_root.as_posix()}/Data/TestFixtures/Verify/{self.generator_id}.verify.json"


def _read_object(path: Path, label: str) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ProducerDescriptorError(f"{label} is unreadable: {path.as_posix()}") from error
    if not isinstance(value, dict):
        raise ProducerDescriptorError(f"{label} root is not an object: {path.as_posix()}")
    return value


def _strings(value: object, field: str, relative: str) -> tuple[str, ...]:
    if not isinstance(value, list) or not all(isinstance(item, str) and item for item in value):
        raise ProducerDescriptorError(f"Producer descriptor field {field} is not a list of names: {relative}")
    return tuple(value)


def _data_inputs(repo_root: Path, value: object, relative: str) -> tuple[str, ...]:
    data_inputs = _strings(value, "data_inputs", relative)
    for item in data_inputs:
        parts = PurePosixPath(item).parts
        if parts[:1] != ("Plugins",) or ".." in parts or "\\" in item:
            raise ProducerDescriptorError(f"Declared data input escapes Plugins/: {item} in {relative}")
        if not (repo_root / item).is_file():
            raise ProducerDescriptorError(f"Declared data input is missing: {item} in {relative}")
    return data_inputs


def _descriptor(repo_root: Path, path: Path) -> ProducerDescriptor:
    relative = path.relative_to(repo_root).as_posix()
    document = _read_object(path, "Producer descriptor")
    modules = _strings(document.get("modules"), "modules", relative)
    kind = document.get("kind")
    if kind == "pipeline":
        if relative != PIPELINE_DESCRIPTOR or not modules:
            raise ProducerDescriptorError(
                f"The pipeline descriptor must be {PIPELINE_DESCRIPTOR} and name its modules: {relative}"
            )
        return ProducerDescriptor(relative, PIPELINE_ID, None, modules, ())
    generator_id = document.get("generator_id")
    version = document.get("generator_version")
    if kind != "producer" or generator_id != path.stem or generator_id == PIPELINE_ID:
        raise ProducerDescriptorError(f"Producer descriptor kind or generator_id does not match its file: {relative}")
    if type(version) is not int or version < 1:
        raise ProducerDescriptorError(f"Producer descriptor generator_version is invalid: {relative}")
    data_inputs = _data_inputs(repo_root, document.get("data_inputs"), relative)
    return ProducerDescriptor(relative, generator_id, version, modules, data_inputs)


def load_producer_descriptors(repo_root: Path) -> tuple[ProducerDescriptor, ...]:
    descriptors = []
    for path in sorted((repo_root / "Plugins").glob("*/*/Data/Producers/*")):
        if not path.is_file() or path.suffix != ".json":
            raise ProducerDescriptorError(
                f"Producers directory holds a non-descriptor entry: {path.relative_to(repo_root).as_posix()}"
            )
        descriptors.append(_descriptor(repo_root, path))
    keys = [descriptor.key for descriptor in descriptors]
    duplicated = sorted({descriptor.label for descriptor in descriptors if keys.count(descriptor.key) > 1})
    if duplicated:
        raise ProducerDescriptorError(f"Producers are declared more than once for one id and version: {duplicated}")
    if (PIPELINE_ID, None) not in keys:
        raise ProducerDescriptorError(f"The pipeline descriptor is missing: {PIPELINE_DESCRIPTOR}")
    return tuple(descriptors)


def recorded_verify(repo_root: Path, descriptor: ProducerDescriptor) -> str | None:
    """The verify test named by the producer's baseline, or None before one is recorded."""
    path = repo_root / descriptor.baseline_path
    if not path.is_file():
        return None
    verify = _read_object(path, "Verify baseline").get("verify")
    if not isinstance(verify, str) or not verify.startswith(VERIFY_PREFIX) or verify == VERIFY_PREFIX:
        raise ProducerDescriptorError(
            f"Verify baseline names no {VERIFY_PREFIX}<Producer> test: {descriptor.baseline_path}"
        )
    return verify
