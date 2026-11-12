#include "ProjectBuildingAssemblyPlan.h"
#include <algorithm>
#include <cmath>

namespace ProjectBuildingAssembly
{
    bool Build(const FInput& I, FPlan& Out, std::string& Error)
    {
        Out = {};
        Error.clear();
        const auto& K=I.Kit;
        const auto Positive=[](double V){return std::isfinite(V) && V>0;};
        if (I.FeatureId.empty() || I.FeatureId.size()>256 || !Positive(I.Width) || !Positive(I.Depth) ||
            !Positive(I.Height) || I.Width>1000 || I.Depth>1000 || I.Height>300 ||
            !std::isfinite(I.SupportSpan) || I.SupportSpan<0 || !std::isfinite(K.MaximumSupportSpan) ||
            K.MaximumSupportSpan<0 || I.SupportSpan>K.MaximumSupportSpan ||
            !Positive(K.PanelWidth) || K.PanelWidth<.25 || !Positive(K.FloorHeight) || K.FloorHeight<1 ||
            !Positive(K.WallThickness) || K.WallThickness>=std::min(I.Width,I.Depth)/3 ||
            !Positive(K.SlabThickness) || K.SlabThickness>=I.Height/4 ||
            !std::isfinite(K.FoundationEmbed) || K.FoundationEmbed<0 || K.FoundationEmbed>1 ||
            !std::isfinite(K.FrameFraction) || K.FrameFraction<.05 || K.FrameFraction>.4 ||
            K.WindowEvery<1 || K.MaximumInstances<4 || K.MaximumInstances>65536)
        {Error="Unsupported modular dimensions or terrain support envelope";return false;}
        const int Floors=std::max(1,static_cast<int>(std::floor(I.Height/K.FloorHeight+.5)));
        const double Step=I.Height/Floors;
        const int NX=static_cast<int>(std::ceil(I.Width/K.PanelWidth));
        const int NY=static_cast<int>(std::ceil(I.Depth/K.PanelWidth));
        if (Step<=K.SlabThickness*2 || Floors*(2*NX+2*NY)+Floors+1>K.MaximumInstances)
        {Error="Modular layout exceeds its finite floor or instance capacity";return false;}
        FPlan Candidate; Candidate.Floors=Floors;
        const auto Add=[&](EPart Part,double X,double Y,double Z,double W,double D,double H,double Yaw=0.)
            {Candidate.Instances.push_back({Part,X,Y,Z,W,D,H,Yaw});};
        const double Bottom=-I.SupportSpan-K.FoundationEmbed;
        Add(EPart::Floor,I.Width/2,I.Depth/2,(Bottom+K.SlabThickness)/2,
            I.Width,I.Depth,K.SlabThickness-Bottom);
        for (int Floor=0;Floor<Floors;++Floor)
        {
            if (Floor>0) Add(EPart::Floor,I.Width/2,I.Depth/2,Floor*Step+K.SlabThickness/2,
                I.Width,I.Depth,K.SlabThickness);
            const double Low=Floor*Step+K.SlabThickness;
            const double High=(Floor+1)*Step-(Floor==Floors-1 ? K.SlabThickness : 0.);
            for (int Side=0;Side<4;++Side)
            {
                const bool AlongY=Side%2!=0;
                const int Count=AlongY ? NY : NX;
                const double Length=(AlongY ? I.Depth : I.Width)/Count;
                for (int Panel=0;Panel<Count;++Panel)
                {
                    // The first ground-level bay is an entrance, not a sealed wall.
                    if (Floor==0 && Side==0 && Panel==0) continue;
                    const EPart Part=Panel%K.WindowEvery==0 ? EPart::Window : EPart::Wall;
                    const double Station=(Panel+.5)*Length;
                    const double X=AlongY ? (Side==1 ? I.Width-K.WallThickness/2 : K.WallThickness/2) : Station;
                    const double Y=AlongY ? Station : (Side==0 ? K.WallThickness/2 : I.Depth-K.WallThickness/2);
                    Add(Part,X,Y,(Low+High)/2,Length,K.WallThickness,High-Low,AlongY ? 90. : 0.);
                }
            }
        }
        Add(EPart::Roof,I.Width/2,I.Depth/2,I.Height-K.SlabThickness/2,
            I.Width,I.Depth,K.SlabThickness);
        Out=std::move(Candidate);
        return true;
    }
}
