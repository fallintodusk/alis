# UEEnvSync.Tests.ps1 - machine-local writer unit tests + stale-cache
# recovery E2E. Run: Invoke-Pester scripts/setup/test/UEEnvSync.Tests.ps1

BeforeAll {
    $script:SetupDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
    $script:RepoRoot = (Resolve-Path (Join-Path $script:SetupDir "..\..")).Path
    Import-Module (Join-Path $script:SetupDir "UEEnvSync.psm1") -Force

    $script:NewL = "<ue-path>"
    $script:NewS = "<ue-path>"
    $script:PreviousL = "<ue-path>"
    $script:PreviousS = "<ue-path>"

    function Invoke-Rewrite([string]$Text) {
        Update-EngineRootsInString -Text $Text `
            -NewLauncherRoot $script:NewL -NewSourceRoot $script:NewS `
            -PreviousLauncherRoot $script:PreviousL `
            -PreviousSourceRoot $script:PreviousS
    }

    function Invoke-MachineSync([string]$SettingsPath, [string]$McpPath) {
        Sync-UEMachineLocalJson -NewLauncherRoot $script:NewL `
            -NewSourceRoot $script:NewS `
            -PreviousLauncherRoot $script:PreviousL `
            -PreviousSourceRoot $script:PreviousS `
            -SettingsPath $SettingsPath -McpPath $McpPath
    }
}

Describe "Convert-EngineRootStyle" {
    It "renders forward, backslash, msys and double-slash styles" {
        Convert-EngineRootStyle "<ue-path>" "C:/X" |
            Should -Be "<ue-path>"
        Convert-EngineRootStyle "<ue-path>" "C:\X\Y" |
            Should -Be "<ue-path>"
        Convert-EngineRootStyle "<ue-path>" "/c/x" |
            Should -Be "/c/UnrealEngine/UE_5.8"
        Convert-EngineRootStyle "<ue-path>" "//G/x" |
            Should -Be "//G/UnrealEngine-5.8"
    }
}

Describe "Update-EngineRootsInString" {
    It "rewrites a grant EQUAL to the root (not only descendants)" {
        (Invoke-Rewrite "<ue-path>").Text |
            Should -Be "<ue-path>"
    }

    It "rewrites beneath the root, preserving the suffix" {
        (Invoke-Rewrite "<ue-path>/Engine/Binaries/Win64").Text |
            Should -Be "<ue-path>/Engine/Binaries/Win64"
    }

    It "preserves backslash style" {
        (Invoke-Rewrite "<ue-path>\Engine").Text |
            Should -Be "<ue-path>\Engine"
    }

    It "preserves msys style (PATH entries)" {
        (Invoke-Rewrite "/c/UnrealEngine/UE_5.7/Engine/Binaries/ThirdParty/Python3/Win64").Text |
            Should -Be "/c/UnrealEngine/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64"
    }

    It "rewrites source roots inside permission strings (double-slash style)" {
        (Invoke-Rewrite 'Read(//G/UnrealEngine-5.7/Engine/Source/**)').Text |
            Should -Be 'Read(//G/UnrealEngine-5.8/Engine/Source/**)'
    }

    It "preserves an unrelated recognized launcher root" {
        (Invoke-Rewrite "<ue-path>/Engine").Text |
            Should -Be "<ue-path>/Engine"
    }

    It "never touches a sibling like UE_5.70-other and does not block on it" {
        $r = Invoke-Rewrite "<ue-path>-other/bin"
        $r.Text | Should -Be "<ue-path>-other/bin"
        $r.Blockers.Count | Should -Be 0
    }

    It "preserves an unrelated unrecognized engine path" {
        $r = Invoke-Rewrite "D:/Engines/UE_5.7/foo"
        $r.Text | Should -Match "D:/Engines/UE_5\.7/foo"
        $r.Blockers.Count | Should -Be 0
    }

    It "moves selected 5.7 while retaining an intentional 5.5 grant" {
        $r = Invoke-Rewrite (
            "<ue-path>/Engine;<ue-path>/Engine")
        $r.Text | Should -Be (
            "<ue-path>/Engine;<ue-path>/Engine")
    }

    It "is idempotent on already-new values" {
        $r = Invoke-Rewrite "<ue-path>/Engine"
        $r.Text | Should -Be "<ue-path>/Engine"
        $r.Blockers.Count | Should -Be 0
    }
}

Describe "Sync-UEMachineLocalJson" {
    BeforeEach {
        $script:Tmp = Join-Path $TestDrive ([IO.Path]::GetRandomFileName())
        New-Item -ItemType Directory -Path $script:Tmp | Out-Null
        $script:SettingsPath = Join-Path $script:Tmp "settings.local.json"
        $script:McpPath = Join-Path $script:Tmp "mcp.json"

        Set-Content $script:SettingsPath -Value @'
{
  "env": {
    "UE_INSTALL_LOCATION": "C:\\UnrealEngine\\UE_5.7",
    "PATH": "${PATH}:/c/UnrealEngine/UE_5.7/Engine/Binaries/ThirdParty/Python3/Win64",
    "CUSTOM_KEEP": "untouched"
  },
  "permissions": {
    "allow": ["Read(//G/UnrealEngine-5.7/Engine/Source/**)"],
    "additionalDirectories": ["<project-root>", "<ue-path>"]
  },
  "unknownTopLevel": { "keep": true }
}
'@
        Set-Content $script:McpPath -Value @'
{
  "mcpServers": {
    "blueprint-mcp": {
      "command": "node.exe",
      "env": {
        "UE_PROJECT_DIR": ".",
        "UE_PORT": "9847",
        "UE_EDITOR_CMD": "<ue-path>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
      }
    }
  }
}
'@
    }

    It "rewrites grants, unifies env var name, sets mcp expansion form" {
        $r = Invoke-MachineSync $script:SettingsPath $script:McpPath
        $r.Blockers.Count | Should -Be 0
        $r.Changed.Count | Should -Be 2

        $s = Get-Content $script:SettingsPath -Raw | ConvertFrom-Json
        $s.env.PSObject.Properties["UE_INSTALL_LOCATION"] | Should -BeNullOrEmpty
        $s.env.UE_PATH | Should -Be "<ue-path>"
        $s.env.PATH | Should -Match "/c/UnrealEngine/UE_5.8/"
        $s.env.CUSTOM_KEEP | Should -Be "untouched"
        $s.permissions.allow[0] | Should -Be 'Read(//G/UnrealEngine-5.8/Engine/Source/**)'
        $s.permissions.additionalDirectories -contains "<ue-path>" |
            Should -BeTrue
        $s.unknownTopLevel.keep | Should -BeTrue

        $m = Get-Content $script:McpPath -Raw | ConvertFrom-Json
        $m.mcpServers.'blueprint-mcp'.env.UE_EDITOR_CMD |
            Should -Be '${UE_PATH}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
        $m.mcpServers.'blueprint-mcp'.env.UE_PROJECT_DIR |
            Should -Be ($script:Tmp -replace '\\', '/')
        $m.mcpServers.'blueprint-mcp'.env.UE_PORT | Should -Be "9847"
    }

    It "is idempotent (second run changes nothing)" {
        Invoke-MachineSync $script:SettingsPath $script:McpPath | Out-Null
        $r2 = Invoke-MachineSync $script:SettingsPath $script:McpPath
        $r2.Changed.Count | Should -Be 0 `
            -Because ($r2 | ConvertTo-Json -Depth 8 -Compress)
    }

    It "creates a one-time backup" {
        Invoke-MachineSync $script:SettingsPath $script:McpPath | Out-Null
        Test-Path "$($script:SettingsPath).engine-sync.bak" | Should -BeTrue
        (Get-Content "$($script:SettingsPath).engine-sync.bak" -Raw) |
            Should -Match "UE_INSTALL_LOCATION"
    }

    It "does NOT partially write when one file is invalid JSON" {
        Set-Content $script:McpPath -Value "{ this is not json"
        $before = Get-Content $script:SettingsPath -Raw
        $r = Invoke-MachineSync $script:SettingsPath $script:McpPath
        $r.Blockers.Count | Should -BeGreaterThan 0
        (Get-Content $script:SettingsPath -Raw) | Should -Be $before
    }

    It "preserves an unrelated unrecognized grant" {
        $s = Get-Content $script:SettingsPath -Raw | ConvertFrom-Json
        $s.permissions.additionalDirectories += "D:/WeirdPlace/UE_5.7"
        Set-Content $script:SettingsPath ($s | ConvertTo-Json -Depth 16)
        $r = Invoke-MachineSync $script:SettingsPath $script:McpPath
        $r.Blockers.Count | Should -Be 0
        $result = Get-Content $script:SettingsPath -Raw | ConvertFrom-Json
        $result.permissions.additionalDirectories |
            Should -Contain "D:/WeirdPlace/UE_5.7"
    }
}

Describe "combined machine-local transaction" {
    It "preflights VS Code before changing JSON or environment" {
        $root = Join-Path $TestDrive "combined"
        $configDir = Join-Path $root "scripts\config"
        $vsCodeDir = Join-Path $root ".vscode"
        New-Item -ItemType Directory -Path $configDir, $vsCodeDir -Force |
            Out-Null
        Copy-Item (Join-Path $script:RepoRoot `
            "scripts\config\Resolve-UEConfig.ps1") $configDir
        Set-Content (Join-Path $configDir "ue_path.conf") `
            "UE_PATH=$script:NewL"
        $settings = Join-Path $root "settings.local.json"
        $mcp = Join-Path $root "mcp.json"
        $launch = Join-Path $vsCodeDir "launch.json"
        Set-Content $settings '{"env":{"UE_PATH":"<ue-path>"}}'
        Set-Content $mcp '{"mcpServers":{}}'
        Set-Content $launch `
            '{"configurations":[{"program":"<ue-path>/Editor.exe"}]}'
        $settingsBefore = Get-Content $settings -Raw
        (Get-Item $launch).IsReadOnly = $true
        $previousErrorAction = $ErrorActionPreference
        try {
            # This child is expected to emit stderr and exit 1. Capture that
            # evidence even when the aggregate runner uses Stop globally.
            $ErrorActionPreference = "Continue"
            $out = & powershell.exe -NoProfile -ExecutionPolicy Bypass `
                -File (Join-Path $script:SetupDir "setup_ue_env.ps1") `
                -EnvScope Process -ConfigDir $configDir -ProjectRoot $root `
                -SettingsPath $settings -McpPath $mcp `
                -PreviousLauncherRoot $script:PreviousL 2>&1
            $exitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousErrorAction
            (Get-Item $launch).IsReadOnly = $false
        }

        $exitCode | Should -Be 1
        ($out -join "`n") | Should -Match "read-only"
        (Get-Content $settings -Raw) | Should -Be $settingsBefore
    }
}

Describe "stale-cache recovery E2E (resolver fails -> setup repairs -> resolver passes)" {
    It "full cycle in a child powershell against the real repo conf" {
        $envPs1 = Join-Path $script:RepoRoot "scripts\config\env.ps1"
        $setup = Join-Path $script:SetupDir "setup_ue_env.ps1"
        $tmp = Join-Path $TestDrive "e2e"
        New-Item -ItemType Directory -Path $tmp | Out-Null
        Set-Content (Join-Path $tmp "settings.local.json") '{"env":{}}'
        Set-Content (Join-Path $tmp "mcp.json") '{"mcpServers":{}}'

        $scriptBlock = @"
`$Env:UE_PATH = 'C:\Stale\UE_9.9'
`$r1 = powershell.exe -NoProfile -ExecutionPolicy Bypass -File '$envPs1' 2>&1
`$rc1 = `$LASTEXITCODE
try {
    & '$setup' -EnvScope Process -SettingsPath '$tmp\settings.local.json' -McpPath '$tmp\mcp.json' -SkipVsCode | Out-Null
    `$rc2 = 0
} catch { `$rc2 = 1 }
`$r3 = powershell.exe -NoProfile -ExecutionPolicy Bypass -File '$envPs1' 2>&1
`$rc3 = `$LASTEXITCODE
Write-Output "RC1=`$rc1 RC2=`$rc2 RC3=`$rc3"
"@
        $out = powershell.exe -NoProfile -ExecutionPolicy Bypass -Command $scriptBlock
        $joined = ($out -join " ")
        $joined | Should -Match "RC1=1"  # stale env -> resolver hard fail
        $joined | Should -Match "RC2=0"  # setup repairs despite stale env
        $joined | Should -Match "RC3=0"  # resolver passes afterwards
    }
}

Describe "Sync-UEUserEnv derives UE_EDITOR_CMD (C10 engine parity)" {
    # Regression for the drift that let Codex drive a UE 5.7 editor against a
    # 5.8 project: UE_EDITOR_CMD must be DERIVED from the same root as
    # UE_PATH, never configured independently. Process scope only - these
    # tests must not mutate the operator's User environment.

    BeforeEach {
        $script:SavedPath = $Env:UE_PATH
        $script:SavedCmd  = $Env:UE_EDITOR_CMD
    }
    AfterEach {
        $Env:UE_PATH = $script:SavedPath
        $Env:UE_EDITOR_CMD = $script:SavedCmd
    }

    It "derives UE_EDITOR_CMD from the launcher root" {
        Sync-UEUserEnv -NewLauncherRoot $script:NewL -Scope Process | Out-Null
        $Env:UE_EDITOR_CMD | Should -Be `
            "<ue-path>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    }

    It "keeps UE_EDITOR_CMD under the same root as UE_PATH" {
        Sync-UEUserEnv -NewLauncherRoot $script:NewL -Scope Process | Out-Null
        $Env:UE_EDITOR_CMD | Should -BeLike "$($Env:UE_PATH)\*"
    }

    It "follows the root when the configured engine changes" {
        Sync-UEUserEnv -NewLauncherRoot $script:PreviousL -Scope Process | Out-Null
        $Env:UE_EDITOR_CMD | Should -BeLike "*UE_5.7*"
        Sync-UEUserEnv -NewLauncherRoot $script:NewL -Scope Process | Out-Null
        $Env:UE_EDITOR_CMD | Should -BeLike "*UE_5.8*"
        $Env:UE_EDITOR_CMD | Should -Not -BeLike "*UE_5.7*"
    }
}
