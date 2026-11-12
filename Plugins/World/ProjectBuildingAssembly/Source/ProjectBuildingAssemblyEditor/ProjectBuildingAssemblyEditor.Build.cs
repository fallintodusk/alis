using UnrealBuildTool;

public class ProjectBuildingAssemblyEditor : ModuleRules
{
    public ProjectBuildingAssemblyEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "Json", "MeshDescription", "StaticMeshDescription", "UnrealEd", "Projects"
        });
    }
}
