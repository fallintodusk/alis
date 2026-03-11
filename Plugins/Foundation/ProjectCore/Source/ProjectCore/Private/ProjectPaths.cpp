// Copyright ALIS. All Rights Reserved.

#include "ProjectPaths.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/FileManager.h"

FString FProjectPaths::GetPluginDataDir(const FString& PluginName)
{
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
	{
		return Plugin->GetBaseDir() / TEXT("Data");
	}
	return FString();
}

bool FProjectPaths::PluginDataFileExists(const FString& PluginName, const FString& RelativePath)
{
	const FString DataDir = GetPluginDataDir(PluginName);
	if (DataDir.IsEmpty())
	{
		return false;
	}
	return FPaths::FileExists(DataDir / RelativePath);
}
