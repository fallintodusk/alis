[SystemSettings]
r.Streaming.PoolSize=8000

[URL]
GameName=Alis

[/Script/HardwareTargeting.HardwareTargetingSettings]
TargetedHardwareClass=Desktop
AppliedTargetedHardwareClass=Desktop
DefaultGraphicsPerformance=Maximum
AppliedDefaultGraphicsPerformance=Maximum

[/Script/EngineSettings.GameMapsSettings]
EditorStartupMap=/City17/Maps/City17_Persistent_WP.City17_Persistent_WP
GameDefaultMap=/City17/Maps/City17_Persistent_WP.City17_Persistent_WP
TransitionMap=None
ServerDefaultMap=/Game/Project/Maps/Boot/L_OrchestratorBoot.L_OrchestratorBoot
GlobalDefaultGameMode=/Script/Engine.GameModeBase
; Use AlisGI to read entry experience and start ProjectLoading (content pipeline kick-off)
GameInstanceClass=/Script/Alis.AlisGI

[/Script/Engine.Engine]
GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient
SmoothedFrameRateRange=(LowerBound=(Type=Inclusive,Value=22.000000),UpperBound=(Type=Exclusive,Value=120.000000))
bSmoothFrameRate=False
NearClipPlane=5.000000
+ActiveGameNameRedirects=(OldGameName="/Script/Alis_0_2_0", NewGameName="/Script/Alis")
;+ActiveClassRedirects=(OldClassName="/Script/Alis.Alis_0_2_0",NewClassName="/Script/Alis.Alis")
MinDesiredFrameRate=0.000000

[/Script/Engine.RendererSettings]

; ==============================
; CORE RENDER PIPELINE (STABLE)
; ==============================
r.ForwardShading=False
r.Nanite.ProjectEnabled=True
r.VirtualTextures=True
r.GBufferFormat=3
r.CustomDepth=1
r.GenerateMeshDistanceFields=True
r.AllowStaticLighting=False

; ==============================
; EXPOSURE (AUTO)
; Restored from Artur's config - fixes overexposed/washed out scene
; Auto exposure + HDR range critical for modern raytracing lighting
; ==============================
r.DefaultFeature.AutoExposure=True
r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True
r.DefaultFeature.LightUnits=1
r.EyeAdaptation.ExposureCompensationBias=2.5

; ==============================
; ANTI-ALIASING
; ==============================
r.DefaultFeature.AntiAliasing=2 ; TAA

; ==============================
; GLOBAL ILLUMINATION (LUMEN OPTIMIZED)
; ==============================
r.DynamicGlobalIlluminationMethod=1 ; Lumen
r.Lumen.GlobalIllumination=1
; Hardware RT enabled for better quality
r.Lumen.HardwareRayTracing=True
r.Lumen.HardwareRayTracing.LightingMode=0
r.Lumen.TraceMeshSDFs=0
r.Lumen.ScreenProbeGather.Quality=2
r.Lumen.ScreenProbeGather.RadianceCache=1
r.Lumen.DiffuseIndirect.Allow=1

; ==============================
; SHADOWS (VSM OPTIMIZED FOR SOFT SHADOWS)
; ==============================
r.Shadow.Virtual.Enable=1
; NEW: 4->8 for soft shadows
r.Shadow.Virtual.SMRT.RayCountDirectional=8
; NEW: 2->4 for quality
r.Shadow.Virtual.SMRT.SamplesPerRayDirectional=4
; NEW: fix for dark corners
r.Shadow.Virtual.OnePassProjection.MaxLightsPerPixel=32
r.RayTracing=True
r.RayTracing.Shadows=False
; Enabled for cinematics/screenshots
r.PathTracing=True

; ==============================
; REFLECTIONS (3060 FRIENDLY)
; ==============================
r.ReflectionMethod=1 ; SSR
r.SSR.Quality=2
r.SSR.MaxRoughness=0.6
r.ReflectionCaptureResolution=64

; ==============================
; AMBIENT OCCLUSION (REDUCED FOR BRIGHTNESS)
; ==============================
r.DefaultFeature.AmbientOcclusion=True
r.DefaultFeature.AmbientOcclusionStaticFraction=True
r.AmbientOcclusionLevels=2
r.AmbientOcclusionMaxQuality=50

; ==============================
; POST FX (CLEAN)
; ==============================
r.DefaultFeature.MotionBlur=False
r.MotionBlurQuality=0
r.BloomQuality=0
r.SceneColorFringeQuality=0
r.TonemapperFilm=1
r.Tonemapper.GrainQuantization=0

; ==============================
; MISC
; ==============================
r.SupportStationarySkylight=True
r.SupportLowQualityLightmaps=True
r.NormalMapsForStaticLighting=False
r.SkinCache.CompileShaders=True
r.Substrate=False
; KEPT: MegaLights support
r.MegaLights.EnableForProject=True

; ==============================
; RESTORED FROM BACKUP (IMPORTANT)
; ==============================
; Decal buffer for decals
r.DBuffer=True
; CRITICAL: texture streaming
r.TextureStreaming=True
; For mirror/portal effects
r.AllowGlobalClipPlane=False
; Disabled for performance
r.Lumen.TranslucencyReflections.FrontLayer.EnableForProject=False

; ==============================
; SKELETAL MESH & CHARACTERS (CRITICAL FOR GAMEPLAY)
; ==============================
; Optimization for skeletal meshes
r.SkinCache.DefaultBehavior=0
; Support for >256 bones
r.GPUSkin.Support16BitBoneIndex=True
; Unlimited bone influences
r.GPUSkin.UnlimitedBoneInfluences=True

; ==============================
; REFRACTION & TRANSLUCENCY
; ==============================
; Glass/water refraction
r.Lumen.Reflections.HardwareRayTracing.Translucent.Refraction.EnableForProject=True

; ==============================
; LEGACY TECH (EXPLICITLY DISABLED)
; ==============================
; Old GI technology (replaced by Lumen)
r.LightPropagationVolume=False
; Screen Space GI disabled (using Lumen)
r.SSGI.Enable=False

; ==============================
; RAYTRACING COMPATIBILITY
; ==============================
; LOD for RT textures
r.RayTracing.UseTextureLod=False

; ==============================
; MOBILE COMPATIBILITY (UNUSED BUT SAFE)
; ==============================
; Desktop-only project
r.Mobile.EnableStaticAndCSMShadowReceivers=False
; Desktop-only project
r.Mobile.AllowDistanceFieldShadows=False

[/Script/Engine.AssetManagerSettings]
+PrimaryAssetTypesToScan=(PrimaryAssetType="Map",AssetBaseClass="/Script/Engine.World",bHasBlueprintClasses=False,bIsEditorOnly=True,Directories=((Path="/Game/Maps")),SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=Unknown))
+PrimaryAssetTypesToScan=(PrimaryAssetType="PrimaryAssetLabel",AssetBaseClass="/Script/Engine.PrimaryAssetLabel",bHasBlueprintClasses=False,bIsEditorOnly=True,Directories=((Path="/Game")),SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))
bOnlyCookProductionAssets=False
bShouldManagerDetermineTypeAndName=False
bShouldGuessTypeAndNameInEditor=True
bShouldAcquireMissingChunksOnLoad=False
bShouldWarnAboutInvalidAssets=True

[Internationalization]
+LocalizationPaths=%GAMEDIR%Content/Localization/GameLocalization
+LocalizationPaths=%GAMEDIR%Content/Localization/Game

[/Script/Engine.PhysicsSettings]
DefaultGravityZ=-980.000000
DefaultTerminalVelocity=4000.000000
DefaultFluidFriction=0.300000
SimulateScratchMemorySize=262144
RagdollAggregateThreshold=4
TriangleMeshTriangleMinAreaThreshold=5.000000
bEnableShapeSharing=False
bEnablePCM=False
bEnableStabilization=False
bWarnMissingLocks=True
bEnable2DPhysics=False
PhysicErrorCorrection=(PingExtrapolation=0.100000,PingLimit=100.000000,ErrorPerLinearDifference=1.000000,ErrorPerAngularDifference=1.000000,MaxRestoredStateError=1.000000,MaxLinearHardSnapDistance=400.000000,PositionLerp=0.000000,AngleLerp=0.400000,LinearVelocityCoefficient=100.000000,AngularVelocityCoefficient=10.000000,ErrorAccumulationSeconds=0.500000,ErrorAccumulationDistanceSq=15.000000,ErrorAccumulationSimilarity=100.000000)
LockedAxis=Invalid
DefaultDegreesOfFreedom=Full3D
BounceThresholdVelocity=200.000000
FrictionCombineMode=Average
RestitutionCombineMode=Average
MaxAngularVelocity=3600.000000
MaxDepenetrationVelocity=0.000000
ContactOffsetMultiplier=0.010000
MinContactOffset=0.000100
MaxContactOffset=1.000000
bSimulateSkeletalMeshOnDedicatedServer=True
DefaultShapeComplexity=CTF_UseSimpleAndComplex
bDefaultHasComplexCollision=True
bSuppressFaceRemapTable=False
bSupportUVFromHitResults=False
bDisableActiveActors=False
bDisableKinematicStaticPairs=False
bDisableKinematicKinematicPairs=False
bDisableCCD=False
bEnableEnhancedDeterminism=False
MaxPhysicsDeltaTime=0.100000
bSubstepping=True
bSubsteppingAsync=False
MaxSubstepDeltaTime=0.016700
MaxSubsteps=6
SyncSceneSmoothingFactor=0.000000
InitialAverageFrameRate=0.016667
PhysXTreeRebuildRate=10
+PhysicalSurfaces=(Type=SurfaceType1,Name="Grass")
+PhysicalSurfaces=(Type=SurfaceType2,Name="Concrete")
+PhysicalSurfaces=(Type=SurfaceType3,Name="Wood")
DefaultBroadphaseSettings=(bUseMBPOnClient=False,bUseMBPOnServer=False,MBPBounds=(Min=(X=0.000000,Y=0.000000,Z=0.000000),Max=(X=0.000000,Y=0.000000,Z=0.000000),IsValid=0),MBPNumSubdivs=2)
bTickPhysicsAsync=False
MinPhysicsDeltaTime=0.000100

[/Script/LuminRuntimeSettings.LuminRuntimeSettings]
FrameTimingHint=FPS_120

[/Script/Engine.CollisionProfile]
-Profiles=(Name="NoCollision",CollisionEnabled=NoCollision,ObjectTypeName="WorldStatic",CustomResponses=((Channel="Visibility",Response=ECR_Ignore),(Channel="Camera",Response=ECR_Ignore)),HelpMessage="No collision",bCanModify=False)
-Profiles=(Name="BlockAll",CollisionEnabled=QueryAndPhysics,ObjectTypeName="WorldStatic",CustomResponses=,HelpMessage="WorldStatic object that blocks all actors by default. All new custom channels will use its own default response. ",bCanModify=False)
-Profiles=(Name="OverlapAll",CollisionEnabled=QueryOnly,ObjectTypeName="WorldStatic",CustomResponses=((Channel="WorldStatic",Response=ECR_Overlap),(Channel="Pawn",Response=ECR_Overlap),(Channel="Visibility",Response=ECR_Overlap),(Channel="WorldDynamic",Response=ECR_Overlap),(Channel="Camera",Response=ECR_Overlap),(Channel="PhysicsBody",Response=ECR_Overlap),(Channel="Vehicle",Response=ECR_Overlap),(Channel="Destructible",Response=ECR_Overlap)),HelpMessage="WorldStatic object that overlaps all actors by default. All new custom channels will use its own default response. ",bCanModify=False)
-Profiles=(Name="BlockAllDynamic",CollisionEnabled=QueryAndPhysics,ObjectTypeName="WorldDynamic",CustomResponses=,HelpMessage="WorldDynamic object that blocks all actors by default. All new custom channels will use its own default response. ",bCanModify=False)
-Profiles=(Name="OverlapAllDynamic",CollisionEnabled=QueryOnly,ObjectTypeName="WorldDynamic",CustomResponses=((Channel="WorldStatic",Response=ECR_Overlap),(Channel="Pawn",Response=ECR_Overlap),(Channel="Visibility",Response=ECR_Overlap),(Channel="WorldDynamic",Response=ECR_Overlap),(Channel="Camera",Response=ECR_Overlap),(Channel="PhysicsBody",Response=ECR_Overlap),(Channel="Vehicle",Response=ECR_Overlap),(Channel="Destructible",Response=ECR_Overlap)),HelpMessage="WorldDynamic object that overlaps all actors by default. All new custom channels will use its own default response. ",bCanModify=False)
-Profiles=(Name="IgnoreOnlyPawn",CollisionEnabled=QueryOnly,ObjectTypeName="WorldDynamic",CustomResponses=((Channel="Pawn",Response=ECR_Ignore),(Channel="Vehicle",Response=ECR_Ignore)),HelpMessage="WorldDynamic object that ignores Pawn and Vehicle. All other channels will be set to default.",bCanModify=False)
-Profiles=(Name="OverlapOnlyPawn",CollisionEnabled=QueryOnly,ObjectTypeName="WorldDynamic",CustomResponses=((Channel="Pawn",Response=ECR_Overlap),(Channel="Vehicle",Response=ECR_Overlap),(Channel="Camera",Response=ECR_Ignore)),HelpMessage="WorldDynamic object that overlaps Pawn, Camera, and Vehicle. All other channels will be set to default. ",bCanModify=False)
-Profiles=(Name="Pawn",CollisionEnabled=QueryAndPhysics,ObjectTypeName="Pawn",CustomResponses=((Channel="Visibility",Response=ECR_Ignore)),HelpMessage="Pawn object. Can be used for capsule of any playerable character or AI. ",bCanModify=False)
-Profiles=(Name="Spectator",CollisionEnabled=QueryOnly,ObjectTypeName="Pawn",CustomResponses=((Channel="WorldStatic",Response=ECR_Block),(Channel="Pawn",Response=ECR_Ignore),(Channel="Visibility",Response=ECR_Ignore),(Channel="WorldDynamic",Response=ECR_Ignore),(Channel="Camera",Response=ECR_Ignore),(Channel="PhysicsBody",Response=ECR_Ignore),(Channel="Vehicle",Response=ECR_Ignore),(Channel="Destructible",Response=ECR_Ignore)),HelpMessage="Pawn object that ignores all other actors except WorldStatic.",bCanModify=False)
-Profiles=(Name="CharacterMesh",CollisionEnabled=QueryOnly,ObjectTypeName="Pawn",CustomResponses=((Channel="Pawn",Response=ECR_Ignore),(Channel="Vehicle",Response=ECR_Ignore),(Channel="Visibility",Response=ECR_Ignore)),HelpMessage="Pawn object that is used for Character Mesh. All other channels will be set to default.",bCanModify=False)
-Profiles=(Name="PhysicsActor",CollisionEnabled=QueryAndPhysics,ObjectTypeName="PhysicsBody",CustomResponses=,HelpMessage="Simulating actors",bCanModify=False)
-Profiles=(Name="Destructible",CollisionEnabled=QueryAndPhysics,ObjectTypeName="Destructible",CustomResponses=,HelpMessage="Destructible actors",bCanModify=False)
-Profiles=(Name="InvisibleWall",CollisionEnabled=QueryAndPhysics,ObjectTypeName="WorldStatic",CustomResponses=((Channel="Visibility",Response=ECR_Ignore)),HelpMessage="WorldStatic object that is invisible.",bCanModify=False)
-Profiles=(Name="InvisibleWallDynamic",CollisionEnabled=QueryAndPhysics,ObjectTypeName="WorldDynamic",CustomResponses=((Channel="Visibility",Response=ECR_Ignore)),HelpMessage="WorldDynamic object that is invisible.",bCanModify=False)
-Profiles=(Name="Trigger",CollisionEnabled=QueryOnly,ObjectTypeName="WorldDynamic",CustomResponses=((Channel="WorldStatic",Response=ECR_Overlap),(Channel="Pawn",Response=ECR_Overlap),(Channel="Visibility",Response=ECR_Ignore),(Channel="WorldDynamic",Response=ECR_Overlap),(Channel="Camera",Response=ECR_Overlap),(Channel="PhysicsBody",Response=ECR_Overlap),(Channel="Vehicle",Response=ECR_Overlap),(Channel="Destructible",Response=ECR_Overlap)),HelpMessage="WorldDynamic object that is used for trigger. All other channels will be set to default.",bCanModify=False)
-Profiles=(Name="Ragdoll",CollisionEnabled=QueryAndPhysics,ObjectTypeName="PhysicsBody",CustomResponses=((Channel="Pawn",Response=ECR_Ignore),(Channel="Visibility",Response=ECR_Ignore)),HelpMessage="Simulating Skeletal Mesh Component. All other channels will be set to default.",bCanModify=False)
-Profiles=(Name="Vehicle",CollisionEnabled=QueryAndPhysics,ObjectTypeName="Vehicle",CustomResponses=,HelpMessage="Vehicle object that blocks Vehicle, WorldStatic, and WorldDynamic. All other channels will be set to default.",bCanModify=False)
-Profiles=(Name="UI",CollisionEnabled=QueryOnly,ObjectTypeName="WorldDynamic",CustomResponses=((Channel="WorldStatic",Response=ECR_Overlap),(Channel="Pawn",Response=ECR_Overlap),(Channel="Visibility",Response=ECR_Block),(Channel="WorldDynamic",Response=ECR_Overlap),(Channel="Camera",Response=ECR_Overlap),(Channel="PhysicsBody",Response=ECR_Overlap),(Channel="Vehicle",Response=ECR_Overlap),(Channel="Destructible",Response=ECR_Overlap)),HelpMessage="WorldStatic object that overlaps all actors by default. All new custom channels will use its own default response. ",bCanModify=False)
+Profiles=(Name="NoCollision",CollisionEnabled=NoCollision,bCanModify=False,ObjectTypeName="WorldStatic",CustomResponses=((Channel="Visibility",Response=ECR_Ignore),(Channel="Camera",Response=ECR_Ignore)),HelpMessage="No collision")
+Profiles=(Name="BlockAll",CollisionEnabled=QueryAndPhysics,bCanModify=False,ObjectTypeName="WorldStatic",CustomResponses=,HelpMessage="WorldStatic object that blocks all actors by default. All new custom channels will use its own default response. ")
+Profiles=(Name="OverlapAll",CollisionEnabled=QueryOnly,bCanModify=False,ObjectTypeName="WorldStatic",CustomResponses=((Channel="WorldStatic",Response=ECR_Overlap),(Channel="WorldDynamic",Response=ECR_Overlap),(Channel="Pawn",Response=ECR_Overlap),(Channel="Visibility",Response=ECR_Overlap),(Channel="Camera",Response=ECR_Overlap),(Channel="PhysicsBody",Response=ECR_Overlap),(Channel="Vehicle",Response=ECR_Overlap),(Channel="Destructible",Response=ECR_Overlap)),HelpMessage="WorldStatic object that overlaps all actors by default. All new custom channels will use its own default response. ")
+Profiles=(Name="BlockAllDynamic",CollisionEnabled=QueryAndPhysics,bCanModify=False,ObjectTypeName="WorldDynamic",CustomResponses=,HelpMessage="WorldDynamic object that blocks all actors by default. All new custom channels will use its own default response. ")
+Profiles=(Name="OverlapAllDynamic",CollisionEnabled=QueryOnly,bCanModify=False,ObjectTypeName="WorldDynamic",CustomResponses=((Channel="WorldStatic",Response=ECR_Overlap),(Channel="WorldDynamic",Response=ECR_Overlap),(Channel="Pawn",Response=ECR_Overlap),(Channel="Visibility",Response=ECR_Overlap),(Channel="Camera",Response=ECR_Overlap),(Channel="PhysicsBody",Response=ECR_Overlap),(Channel="Vehicle",Response=ECR_Overlap),(Channel="Destructible",Response=ECR_Overlap)),HelpMessage="WorldDynamic object that overlaps all actors by default. All new custom channels will use its own default response. ")
+Profiles=(Name="IgnoreOnlyPawn",CollisionEnabled=QueryOnly,bCanModify=False,ObjectTypeName="WorldDynamic",CustomResponses=((Channel="Pawn",Response=ECR_Ignore),(Channel="Vehicle",Response=ECR_Ignore)),HelpMessage="WorldDynamic object that ignores Pawn and Vehicle. All other channels will be set to default.")
+Profiles=(Name="OverlapOnlyPawn",CollisionEnabled=QueryOnly,bCanModify=False,ObjectTypeName="WorldDynamic",CustomResponses=((Channel="Pawn",Response=ECR_Overlap),(Channel="Camera",Response=ECR_Ignore),(Channel="Vehicle",Response=ECR_Overlap)),HelpMessage="WorldDynamic object that overlaps Pawn, Camera, and Vehicle. All other channels will be set to default. ")
+Profiles=(Name="Pawn",CollisionEnabled=QueryAndPhysics,bCanModify=False,ObjectTypeName="Pawn",CustomResponses=((Channel="Visibility",Response=ECR_Ignore)),HelpMessage="Pawn object. Can be used for capsule of any playerable character or AI. ")
+Profiles=(Name="Spectator",CollisionEnabled=QueryOnly,bCanModify=False,ObjectTypeName="Pawn",CustomResponses=((Channel="WorldDynamic",Response=ECR_Ignore),(Channel="Pawn",Response=ECR_Ignore),(Channel="Visibility",Response=ECR_Ignore),(Channel="Camera",Response=ECR_Ignore),(Channel="PhysicsBody",Response=ECR_Ignore),(Channel="Vehicle",Response=ECR_Ignore),(Channel="Destructible",Response=ECR_Ignore)),HelpMessage="Pawn object that ignores all other actors except WorldStatic.")
+Profiles=(Name="CharacterMesh",CollisionEnabled=QueryOnly,bCanModify=False,ObjectTypeName="Pawn",CustomResponses=((Channel="Pawn",Response=ECR_Ignore),(Channel="Visibility",Response=ECR_Ignore),(Channel="Vehicle",Response=ECR_Ignore)),HelpMessage="Pawn object that is used for Character Mesh. All other channels will be set to default.")
+Profiles=(Name="PhysicsActor",CollisionEnabled=QueryAndPhysics,bCanModify=False,ObjectTypeName="PhysicsBody",CustomResponses=,HelpMessage="Simulating actors")
+Profiles=(Name="Destructible",CollisionEnabled=QueryAndPhysics,bCanModify=False,ObjectTypeName="Destructible",CustomResponses=,HelpMessage="Destructible actors")
+Profiles=(Name="InvisibleWall",CollisionEnabled=QueryAndPhysics,bCanModify=False,ObjectTypeName="WorldStatic",CustomResponses=((Channel="Visibility",Response=ECR_Ignore)),HelpMessage="WorldStatic object that is invisible.")
+Profiles=(Name="InvisibleWallDynamic",CollisionEnabled=QueryAndPhysics,bCanModify=False,ObjectTypeName="WorldDynamic",CustomResponses=((Channel="Visibility",Response=ECR_Ignore)),HelpMessage="WorldDynamic object that is invisible.")
+Profiles=(Name="Trigger",CollisionEnabled=QueryOnly,bCanModify=False,ObjectTypeName="WorldDynamic",CustomResponses=((Channel="WorldStatic",Response=ECR_Overlap),(Channel="WorldDynamic",Response=ECR_Overlap),(Channel="Pawn",Response=ECR_Overlap),(Channel="Visibility",Response=ECR_Ignore),(Channel="Camera",Response=ECR_Overlap),(Channel="PhysicsBody",Response=ECR_Overlap),(Channel="Vehicle",Response=ECR_Overlap),(Channel="Destructible",Response=ECR_Overlap)),HelpMessage="WorldDynamic object that is used for trigger. All other channels will be set to default.")
+Profiles=(Name="Ragdoll",CollisionEnabled=QueryAndPhysics,bCanModify=False,ObjectTypeName="PhysicsBody",CustomResponses=((Channel="Pawn",Response=ECR_Ignore)),HelpMessage="Simulating Skeletal Mesh Component. All other channels will be set to default.")
+Profiles=(Name="Vehicle",CollisionEnabled=QueryAndPhysics,bCanModify=False,ObjectTypeName="Vehicle",CustomResponses=,HelpMessage="Vehicle object that blocks Vehicle, WorldStatic, and WorldDynamic. All other channels will be set to default.")
+Profiles=(Name="UI",CollisionEnabled=QueryOnly,bCanModify=False,ObjectTypeName="WorldDynamic",CustomResponses=((Channel="WorldStatic",Response=ECR_Overlap),(Channel="WorldDynamic",Response=ECR_Overlap),(Channel="Pawn",Response=ECR_Overlap),(Channel="Camera",Response=ECR_Overlap),(Channel="PhysicsBody",Response=ECR_Overlap),(Channel="Vehicle",Response=ECR_Overlap),(Channel="Destructible",Response=ECR_Overlap)),HelpMessage="WorldStatic object that overlaps all actors by default. All new custom channels will use its own default response. ")
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Ignore,bTraceType=True,bStaticObject=False,Name="HighlightLT")
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel2,DefaultResponse=ECR_Ignore,bTraceType=True,bStaticObject=False,Name="InterLT")
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel3,DefaultResponse=ECR_Ignore,bTraceType=False,bStaticObject=False,Name="Indoor")
-ProfileRedirects=(OldName="BlockingVolume",NewName="InvisibleWall")
-ProfileRedirects=(OldName="InterpActor",NewName="IgnoreOnlyPawn")
-ProfileRedirects=(OldName="StaticMeshComponent",NewName="BlockAllDynamic")
-ProfileRedirects=(OldName="SkeletalMeshActor",NewName="PhysicsActor")
-ProfileRedirects=(OldName="InvisibleActor",NewName="InvisibleWallDynamic")
+ProfileRedirects=(OldName="BlockingVolume",NewName="InvisibleWall")
+ProfileRedirects=(OldName="InterpActor",NewName="IgnoreOnlyPawn")
+ProfileRedirects=(OldName="StaticMeshComponent",NewName="BlockAllDynamic")
+ProfileRedirects=(OldName="SkeletalMeshActor",NewName="PhysicsActor")
+ProfileRedirects=(OldName="InvisibleActor",NewName="InvisibleWallDynamic")
-CollisionChannelRedirects=(OldName="Static",NewName="WorldStatic")
-CollisionChannelRedirects=(OldName="Dynamic",NewName="WorldDynamic")
-CollisionChannelRedirects=(OldName="VehicleMovement",NewName="Vehicle")
-CollisionChannelRedirects=(OldName="PawnMovement",NewName="Pawn")
+CollisionChannelRedirects=(OldName="Static",NewName="WorldStatic")
+CollisionChannelRedirects=(OldName="Dynamic",NewName="WorldDynamic")
+CollisionChannelRedirects=(OldName="VehicleMovement",NewName="Vehicle")
+CollisionChannelRedirects=(OldName="PawnMovement",NewName="Pawn")
+CollisionChannelRedirects=(OldName="InteractionLT",NewName="HighlightLT")
+CollisionChannelRedirects=(OldName="Inter_LT",NewName="InterLT")

[/Script/NavigationSystem.RecastNavMesh]
RuntimeGeneration=Dynamic
CellSize=5.000000

[/Script/NavigationSystem.NavigationSystemV1]
bGenerateNavigationOnlyAroundNavigationInvokers=True

[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]
bEnablePlugin=True
bAllowNetworkConnection=True
SecurityToken=9E5A82C341B561FEB76D2D9145419BC9
bIncludeInShipping=False
bAllowExternalStartInShipping=False
bCompileAFSProject=False
bUseCompression=False
bLogFiles=False
bReportStats=False
ConnectionType=USBOnly
bUseManualIPAddress=False
ManualIPAddress=

[/Script/WindowsTargetPlatform.WindowsTargetSettings]
DefaultGraphicsRHI=DefaultGraphicsRHI_DX12
-D3D12TargetedShaderFormats=PCD3D_SM5
+D3D12TargetedShaderFormats=PCD3D_SM5
+D3D12TargetedShaderFormats=PCD3D_SM6
-D3D11TargetedShaderFormats=PCD3D_SM5
+D3D11TargetedShaderFormats=PCD3D_SM5
Compiler=Default
AudioSampleRate=48000
AudioCallbackBufferFrameSize=1024
AudioNumBuffersToEnqueue=1
AudioMaxChannels=0
AudioNumSourceWorkers=4
SpatializationPlugin=
SourceDataOverridePlugin=
ReverbPlugin=
OcclusionPlugin=
CompressionOverrides=(bOverrideCompressionTimes=False,DurationThreshold=5.000000,MaxNumRandomBranches=0,SoundCueQualityIndex=0)
CacheSizeKB=65536
MaxChunkSizeOverrideKB=0
bResampleForDevice=False
MaxSampleRate=48000.000000
HighSampleRate=32000.000000
MedSampleRate=24000.000000
LowSampleRate=12000.000000
MinSampleRate=8000.000000
CompressionQualityModifier=1.000000
AutoStreamingThreshold=0.000000
SoundCueCookQualityIndex=-1

[/Script/Engine.UserInterfaceSettings]
ApplicationScale=1.000000

[/Script/IOSRuntimeSettings.IOSRuntimeSettings]
FrameRateLock=PUFRL_60

[/Script/Engine.WorldPartitionSettings]
bNewMapsEnableWorldPartition=True
