# ProjectCore

Foundation plugin for cross-component contracts, service discovery, experience
registration, and small engine-facing helpers.

| Concern | Owner |
|---|---|
| Durable component guidance | [Documentation](docs/README.md) |
| Public cross-component interfaces | [Interfaces](Source/ProjectCore/Public/Interfaces/README.md) |

Runtime providers register interfaces with `FProjectServiceLocator`; consumers
resolve the interface they require at a stable lifecycle boundary.
