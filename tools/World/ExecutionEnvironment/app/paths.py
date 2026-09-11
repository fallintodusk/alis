from pathlib import Path


def environment_data_root(repo_root: Path) -> Path:
    return repo_root / "tmp" / "world" / "execution_environment"
