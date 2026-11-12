# Building assembly editor module

Owns the portable layout implementation and its native editor realization.

| Surface | Implementation |
|---|---|
| Portable input, kit, plan and refusal | [Plan contract](Public/ProjectBuildingAssemblyPlan.h) and [layout](Private/ProjectBuildingAssemblyPlan.cpp) |
| Native kit reads, realized-output validation and replacement | [Native contract](Public/ProjectBuildingAssemblyRealization.h), [kit admission](Private/ProjectBuildingAssemblyKit.cpp) and [ISM realization](Private/ProjectBuildingAssemblyRealization.cpp) |

Persistent callers supply shared module meshes and stable actor identity. Updates
retain the external actor and its GUID inside the caller's World transaction;
the caller owns package saves, manifests and rollback. Isolated callers can
instead use actor-local modules and staged replacement.

The planner uses standard C++20 so host checks execute the production algorithm
without loading Unreal. Native MSVC compilation and the isolated scene test
verify the engine adapter separately. No host compiler is runtime authority.
