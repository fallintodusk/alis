#include "ProjectBuildingAssemblyPlan.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace ProjectBuildingAssembly;
static void Require(bool Value, const char* Message) { if (!Value) throw std::runtime_error(Message); }
int main()
{
    try
    {
        // Dimensions retain Manhattan way/250440683's source-shaped rectangle.
        FInput Input; Input.FeatureId = "alis:osm:way:250440683";
        Input.Width = 15.7706190658; Input.Depth = 5.3121967318;
        Input.Height = 7.5; Input.SupportSpan = 0.1;
        FPlan Plan, Cold; std::string Error;
        Require(Build(Input, Plan, Error), "Captured building must produce a modular layout");
        Require(Build(Input, Cold, Error) && Plan == Cold, "Cold layout must be deterministic");
        Require(Plan.Floors == 3, "The source height must produce three bounded floors");
        for (EPart Part : {EPart::Wall, EPart::Window, EPart::Floor, EPart::Roof})
            Require(std::any_of(Plan.Instances.begin(), Plan.Instances.end(), [&](const auto& P){return P.Part == Part;}), "Every reusable part family must participate");
        for (const auto& P : Plan.Instances)
        {
            const bool AlongY = P.Yaw == 90;
            const double DX = AlongY ? P.Depth : P.Width, DY = AlongY ? P.Width : P.Depth;
            Require(P.X-DX/2 >= -1e-9 && P.X+DX/2 <= Input.Width+1e-9, "Part exceeds admitted width");
            Require(P.Y-DY/2 >= -1e-9 && P.Y+DY/2 <= Input.Depth+1e-9, "Part exceeds admitted depth");
            Require(P.Z+P.Height/2 <= Input.Height+1e-9, "Part exceeds admitted source height");
            Require(P.Width>0 && P.Depth>0 && P.Height>0, "Part dimensions must be positive");
        }
        const double Step=Input.Height/Plan.Floors;
        for (int Floor=1;Floor<Plan.Floors;++Floor)
        {
            const auto Slab=std::find_if(Plan.Instances.begin(),Plan.Instances.end(),[&](const auto& P)
                {return P.Part==EPart::Floor && P.Z>Floor*Step-1e-6 && P.Z<(Floor+1)*Step;});
            Require(Slab!=Plan.Instances.end(),"Each story has a floor slab");
            Require(std::abs(Slab->Z-Slab->Height/2-Floor*Step)<1e-9,"Slab bottom must meet previous story wall top");
            Require(std::abs(Slab->Z+Slab->Height/2-Floor*Step-Input.Kit.SlabThickness)<1e-9,"Slab top must meet next story wall bottom");
        }
        auto Changed = Input; Changed.Kit.WindowEvery = 3;
        Require(Build(Changed, Cold, Error) && !(Plan == Cold), "Kit change must regenerate layout");
        for (int Case=0; Case<6; ++Case)
        {
            auto Bad=Input;
            if (Case==0) Bad.SupportSpan=1.;
            if (Case==1) Bad.Height=std::numeric_limits<double>::quiet_NaN();
            if (Case==2) Bad.Kit.MaximumInstances=2;
            if (Case==3) Bad.Width=-1;
            if (Case==4) Bad.Kit.WindowEvery=0;
            if (Case==5) Bad.Kit.WallThickness=Input.Depth;
            Cold=Plan;
            Require(!Build(Bad, Cold, Error) && Cold.Instances.empty() && !Error.empty(), "Unsupported input must refuse without a partial plan");
        }
        std::cout << "PASS captured layout, deterministic replay, four part families, bounds, kit change, six refusal cases\n";
    }
    catch (const std::exception& E) { std::cerr << E.what() << '\n'; return 1; }
}
