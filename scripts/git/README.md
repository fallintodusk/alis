# Git Scripts

Public router for ALIS git workflow automation.

## Main Areas

### Public mirror flow

Publish a filtered public snapshot of the repository to a separate remote repository.

- Mirror docs: [mirror/README.md](mirror/README.md)

Typical usage:

```bash
make mirror MIRROR_DRY_RUN=1 MIRROR_REMOTE_URL=git@github.com:owner/repo.git
make mirror MIRROR_REMOTE_URL=git@github.com:owner/repo.git
```

Mirror flow summary:
1. reads committed `HEAD`
2. filters with `scripts/git/mirror/mirror.exclude`
3. sanitizes and validates the filtered tree
4. commits in a disposable temp repo
5. pushes only to the separate mirror remote

### Local git hooks

The full internal tree includes local git hook helpers for Unreal build-cache invalidation after merges and branch switches.

Hook installation docs:
- [../setup/README.md](../setup/README.md)

These hook implementation files are not part of the public mirror payload, but the workflow is still documented here because it is part of the repository's local developer model.

### Squash merge helper

The repository also includes:
- `squash_merge.sh`

Purpose:
- squash temporary work branches into the current branch with a cleaner history

## Notes

- Public mirror publication is the primary public-facing git automation flow in this repository.
- The mirror baseline is the separate remote mirror repository, not a persistent local temp clone.
