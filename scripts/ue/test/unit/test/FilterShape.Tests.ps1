# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Dev Loop Contract enforcement surface (docs/agents/canonical.md section 7):
# exactly one full test name is accepted in dev mode; prefixes, wildcards,
# unions, and tag expressions are refused. Automation tests live BOTH in the
# shared Plugins/Test plugin and inside each owning plugin's own Tests
# directory, so recognition must cover both - otherwise plugin-owned exact
# IDs get pushed onto -Mode Gate, which ALSO unlocks broad filters and
# inverts the guardrail.

BeforeAll {
    . (Join-Path $PSScriptRoot '..\Test-FilterShape.ps1')
}

Describe 'Dev-loop exact filter recognition' {
    It 'accepts an exact test id owned by a plugin outside Plugins/Test' {
        $shape = Test-ExactFilter -Filter 'Project.World.Realization.Runtime.RouteCollision'
        $shape.IsExact | Should -BeTrue
    }

    It 'accepts an exact test id under a test module Private/Unit folder' {
        $shape = Test-ExactFilter -Filter 'ProjectLoading.Unit.TravelURL.Provenance'
        $shape.IsExact | Should -BeTrue
    }

    It 'accepts an exact test id in the shared test plugin' {
        $shape = Test-ExactFilter -Filter 'ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.BuilderWrapsEveryCell'
        $shape.IsExact | Should -BeTrue
    }

    It 'still refuses a prefix of a plugin-owned test id' {
        # Same leading segments as the accepted world test, but not a test.
        $shape = Test-ExactFilter -Filter 'Project.World.Realization.Runtime'
        $shape.IsExact | Should -BeFalse
    }

    It 'still refuses broad shapes' {
        foreach ($filter in @(
            'ProjectIntegrationTests.UI',
            'ProjectIntegrationTests.UI.*',
            'Project.World.Realization.Runtime.RouteCollision;Project.World.Realization.Runtime.ProfileContract',
            'Group:World')) {
            (Test-ExactFilter -Filter $filter).IsExact | Should -BeFalse
        }
    }
}
