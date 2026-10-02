# Git Publication Protection

Git publication is protected at the repository and workflow boundaries, not by
a local branch-name hook.

## Current Controls

- The official GitHub repository requires a pull request and the `verify`
  status check before `main` can advance.
- Force pushes and branch deletion are disabled for `main`.
- The current single-maintainer configuration uses zero required approvals;
  an approval that the only maintainer cannot supply is not a security control.
- Agent publication still requires explicit operator authorization.
  Repository settings never grant that authority.
- `.githooks/pre-push` runs Git LFS only. It is not an autonomous-branch
  blocker and must not be described as one.

Untrusted users may propose pull requests, but they cannot merge or publish an
official release without repository permission. The 2.0.0 release tooling also
validates the official repository identity before official remote mutation.

## Release Flow

```text
validated local public projection
-> release branch
-> pull request
-> required verify check
-> authorized merge
-> final artifacts bound to the actual main revision
-> explicit operator-authorized tag and release publication
```

Local dry runs and user-owned forks remain valid. They do not acquire ALIS
publication authority.
