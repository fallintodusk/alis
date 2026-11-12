#pragma once
#include <string>
#include <vector>

// The layout core has no engine dependency so the same code runs in fast host tests.
namespace ProjectBuildingAssembly
{
    enum class EPart { Wall, Window, Floor, Roof };
    struct FKit
    {
        double PanelWidth = 2.0;
        double FloorHeight = 3.0;
        double WallThickness = 0.18;
        double SlabThickness = 0.15;
        double FoundationEmbed = 0.1;
        double MaximumSupportSpan = 0.5;
        double FrameFraction = 0.12;
        int WindowEvery = 2;
        int MaximumInstances = 4096;
    };
    struct FInput
    {
        std::string FeatureId;
        double Width = 0, Depth = 0, Height = 0, SupportSpan = 0;
        FKit Kit;
    };
    struct FInstance
    {
        EPart Part = EPart::Wall;
        double X = 0, Y = 0, Z = 0, Width = 0, Depth = 0, Height = 0, Yaw = 0;
        bool operator==(const FInstance&) const = default;
    };
    struct FPlan
    {
        std::vector<FInstance> Instances;
        int Floors = 0;
        bool operator==(const FPlan&) const = default;
    };
    PROJECTBUILDINGASSEMBLYEDITOR_API bool Build(const FInput& Input, FPlan& Out, std::string& Error);
}
