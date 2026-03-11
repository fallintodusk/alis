#include "ProjectMindModule.h"
#include "Interfaces/IMindService.h"
#include "Interfaces/IMindThoughtSource.h"
#include "Interfaces/IMindRuntimeControl.h"
#include "ProjectServiceLocator.h"
#include "Services/DefaultInventoryMindThoughtSource.h"
#include "Services/DefaultVitalsMindThoughtSource.h"
#include "Services/MindServiceImpl.h"
#include "Services/NullQuestMindThoughtSource.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMind, Log, All);

void FProjectMindModule::StartupModule()
{
	UE_LOG(LogProjectMind, Log, TEXT("ProjectMind: StartupModule"));

	TSharedRef<FMindServiceImpl> MindServiceRef = MakeShared<FMindServiceImpl>();
	MindService = MindServiceRef;
	FProjectServiceLocator::Register<IMindService>(StaticCastSharedRef<IMindService>(MindServiceRef));
	FProjectServiceLocator::Register<IMindRuntimeControl>(StaticCastSharedRef<IMindRuntimeControl>(MindServiceRef));
	MindService->Start();

	if (!FProjectServiceLocator::Resolve<IMindVitalsThoughtSource>().IsValid())
	{
		TSharedRef<FDefaultVitalsMindThoughtSource> SourceRef = MakeShared<FDefaultVitalsMindThoughtSource>();
		DefaultVitalsThoughtSource = SourceRef;
		FProjectServiceLocator::Register<IMindVitalsThoughtSource>(
			StaticCastSharedRef<IMindVitalsThoughtSource>(SourceRef));
	}

	if (!FProjectServiceLocator::Resolve<IMindInventoryThoughtSource>().IsValid())
	{
		TSharedRef<FDefaultInventoryMindThoughtSource> SourceRef = MakeShared<FDefaultInventoryMindThoughtSource>();
		DefaultInventoryThoughtSource = SourceRef;
		FProjectServiceLocator::Register<IMindInventoryThoughtSource>(
			StaticCastSharedRef<IMindInventoryThoughtSource>(SourceRef));
	}

	if (!FProjectServiceLocator::Resolve<IMindQuestThoughtSource>().IsValid())
	{
		TSharedRef<FNullQuestMindThoughtSource> QuestSourceRef = MakeShared<FNullQuestMindThoughtSource>();
		DefaultQuestThoughtSource = QuestSourceRef;
		FProjectServiceLocator::Register<IMindQuestThoughtSource>(StaticCastSharedRef<IMindQuestThoughtSource>(QuestSourceRef));
	}
}

void FProjectMindModule::ShutdownModule()
{
	if (MindService.IsValid())
	{
		MindService->Stop();
		MindService.Reset();
	}

	const TSharedPtr<IMindVitalsThoughtSource> CurrentVitalsSource = FProjectServiceLocator::Resolve<IMindVitalsThoughtSource>();
	if (CurrentVitalsSource.IsValid() && CurrentVitalsSource.Get() == DefaultVitalsThoughtSource.Get())
	{
		FProjectServiceLocator::Unregister<IMindVitalsThoughtSource>();
	}
	DefaultVitalsThoughtSource.Reset();

	const TSharedPtr<IMindInventoryThoughtSource> CurrentInventorySource = FProjectServiceLocator::Resolve<IMindInventoryThoughtSource>();
	if (CurrentInventorySource.IsValid() && CurrentInventorySource.Get() == DefaultInventoryThoughtSource.Get())
	{
		FProjectServiceLocator::Unregister<IMindInventoryThoughtSource>();
	}
	DefaultInventoryThoughtSource.Reset();

	const TSharedPtr<IMindQuestThoughtSource> CurrentQuestSource = FProjectServiceLocator::Resolve<IMindQuestThoughtSource>();
	if (CurrentQuestSource.IsValid() && CurrentQuestSource.Get() == DefaultQuestThoughtSource.Get())
	{
		FProjectServiceLocator::Unregister<IMindQuestThoughtSource>();
	}
	DefaultQuestThoughtSource.Reset();

	FProjectServiceLocator::Unregister<IMindService>();
	FProjectServiceLocator::Unregister<IMindRuntimeControl>();

	UE_LOG(LogProjectMind, Log, TEXT("ProjectMind: ShutdownModule"));
}

IMPLEMENT_MODULE(FProjectMindModule, ProjectMind)
