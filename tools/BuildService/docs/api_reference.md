# Build Service CLI

Run commands from `tools/BuildService` through the repository wrapper:

```powershell
.\scripts\build.ps1 run -- <global-options> <command> <command-options>
```

The global options are `--config <path>` and `--verbose`.

## Commands

```text
build [--force] [--plugin <name>] [--dry-run] [--build-id <id>]
      [--channel <name>] [--stream]

publish --build-id <id> (--stage | --promote | --rollback)

watch [--branch <name>] [--interval <seconds>]
```

`build` scans changed units by default. `--force` selects all units, while
`--plugin` selects one named plugin. `--dry-run` performs planning and executor
validation without packaging or manifest mutation.

`publish` requires exactly one operation. Stage and promote call the configured
CDN Control API; promote also requires the local source-release receipt to
verify against the current repository and staged artifacts. Rollback delegates
the channel request to the Control API.

Use `--help` on the built executable as the executable option authority.
