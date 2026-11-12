// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "ProjectWorldConstructedLayerProducer.h"

class FProjectWorldConstructedEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{ IModularFeatures::Get().RegisterModularFeature(IProjectWorldLayerProducer::GetModularFeatureName(),&FProjectWorldConstructedLayerProducer::Get()); }
	virtual void ShutdownModule() override
	{ IModularFeatures::Get().UnregisterModularFeature(IProjectWorldLayerProducer::GetModularFeatureName(),&FProjectWorldConstructedLayerProducer::Get()); }
};
IMPLEMENT_MODULE(FProjectWorldConstructedEditorModule,ProjectWorldConstructedEditor)
