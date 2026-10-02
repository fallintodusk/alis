# Continuous Integration

Checked-in GitHub workflows verify contributor rights declarations and run
public-source-complete checks that do not require Unreal Engine or private
payloads. Local build, runtime, and release gates remain owned by repository
scripts.

| Concern | Owner |
|---|---|
| GitHub rights-affirmation workflow | [Workflow](../../.github/workflows/rights-affirmation.yml) |
| Public source checks | [Workflow](../../.github/workflows/source-checks.yml) |
| Branch and publication protection | [Push protection](push_protection.md) |
| Build entry points | [Build](../build/README.md) |
| Test selection and execution | [Testing](../testing/README.md) |
| Release preparation and verification | [Packaging](../build/packaging_guide.md) |

Do not describe local automation as hosted CI until a checked-in workflow
actually executes it.
