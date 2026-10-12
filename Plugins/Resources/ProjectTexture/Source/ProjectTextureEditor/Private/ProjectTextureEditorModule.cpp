// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Modules/ModuleManager.h"
#include "ProjectTextureFixtureDomain.h"

class FProjectTextureEditorModule final : public IModuleInterface
{
public:
	virtual void ShutdownModule() override { ProjectTextureFixtureDomain::Shutdown(); }
};

IMPLEMENT_MODULE(FProjectTextureEditorModule, ProjectTextureEditor)
