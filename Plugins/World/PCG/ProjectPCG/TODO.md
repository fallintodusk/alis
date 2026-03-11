# ProjectPCG TODO

## DIP Pattern Implementation

### 1. Define IPCGRuntimeService interface in ProjectCore
- [ ] Create interface in ProjectCore/Public/Interfaces/IPCGRuntimeService.h
- [ ] Define methods:
  - [ ] Generation triggers (TriggerPCGGeneration, etc.)
  - [ ] Status queries (IsGenerationInProgress, GetGenerationProgress, etc.)
  - [ ] Parameter configs (SetPCGParameters, GetPCGParameters, etc.)
  - [ ] Runtime generation control
  - [ ] Query/status APIs

### 2. Create UProjectPCGSubsystem
- [ ] Decide subsystem type:
  - [ ] GameInstance subsystem (persistent across levels)
  - [ ] World subsystem (per-world generation)
- [ ] Implement IPCGRuntimeService interface
- [ ] Provide runtime PCG generation services
- [ ] Integration with UE PCG system
- [ ] Handle generation requests from features

### 3. Service registration
- [ ] Register with FProjectServiceLocator in Initialize()
  - [ ] FProjectServiceLocator::Register<IPCGRuntimeService>(this)
- [ ] Unregister in Deinitialize()
  - [ ] FProjectServiceLocator::Unregister<IPCGRuntimeService>()
- [ ] Ensure registration happens before features resolve

### 4. Feature integration
- [ ] Feature plugins resolve via ServiceLocator:
  - [ ] ProjectForestBiomesPack
  - [ ] ProjectUrbanRuinsPCGRecipe
  - [ ] Other procedural content features
- [ ] Features call IPCGRuntimeService methods
- [ ] NO compile-time dependencies on ProjectPCG

### 5. Testing
- [ ] Test service registration at startup
- [ ] Test feature can resolve IPCGRuntimeService
- [ ] Test PCG generation triggers work
- [ ] Test status queries return correct data
- [ ] Test parameter configuration works
- [ ] Test DIP pattern (features don't compile-depend on ProjectPCG)

## Notes
- Follows DIP pattern (Systems register, Features resolve)
- ProjectPCG REGISTERS IPCGRuntimeService
- Features RESOLVE IPCGRuntimeService (NO compile-time dependency)
- See ProjectLoadingSubsystem for reference implementation pattern
