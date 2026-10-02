using System;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnrealBuildTool;

public class ProjectWorldMeshTerrainEditor : ModuleRules
{
	public ProjectWorldMeshTerrainEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		string[] FingerprintFiles = GetCompilerFingerprintFiles();
		ExternalDependencies.AddRange(FingerprintFiles.Select(
			PathName => Path.GetRelativePath(ModuleDirectory, PathName)));
		PrivateDefinitions.Add(
			"PROJECT_WORLD_MESH_TERRAIN_COMPILER_SOURCE_SHA256=\"" +
			ComputeCompilerSourceFingerprint(FingerprintFiles) + "\"");

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AssetRegistry",
			"Core",
			"CoreUObject",
			"Engine",
			"GeometryCore",
			"Json",
			"MeshPartition",
			"MeshPartitionEditor",
			"ProjectCore",
			"ProjectWorld",
			"ProjectWorldEditor",
			"ProjectWorldMeshTerrain",
			"RenderCore",
			"UnrealEd"
		});
	}

	private string[] GetCompilerFingerprintFiles()
	{
		string RepositoryRoot = GetRepositoryRoot();
		string[] FingerprintRelativePaths =
		{
			"Plugins/World/ProjectWorld/Source/ProjectWorld/Private/ProjectWorldTerrainRuntimeRole.cpp",
			"Plugins/World/ProjectWorld/Source/ProjectWorld/Public/ProjectWorldTerrainRuntimeRole.h",
			"Plugins/World/ProjectWorldMeshTerrain/Data/Schemas/mesh-terrain-layout-receipt.schema.json",
			"Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/ProjectWorldMeshTerrain.Build.cs",
			"Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/Private/ProjectWorldMeshTerrainPartition.cpp",
			"Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/Private/ProjectWorldMeshTerrainTransformer.cpp",
			"Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/Public/ProjectWorldMeshTerrainPartition.h",
			"Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/Public/ProjectWorldMeshTerrainTransformer.h",
			"Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/ProjectWorldMeshTerrainEditor.Build.cs",
			"Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainBuildPipeline.h",
			"Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainLayoutReceipt.cpp",
			"Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainProducer.cpp",
			"Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainProducer.h",
			"Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Public/ProjectWorldMeshTerrainLayoutReceipt.h"
		};
		return FingerprintRelativePaths
			.OrderBy(PathName => PathName, StringComparer.Ordinal)
			.Select(PathName => Path.Combine(RepositoryRoot, PathName.Replace('/', Path.DirectorySeparatorChar)))
			.Select(PathName => File.Exists(PathName) ? PathName :
				throw new BuildException("Missing Mesh Terrain compiler source: " + PathName))
			.ToArray();
	}

	private string GetRepositoryRoot()
	{
		return Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", "..", ".."));
	}

	private string ComputeCompilerSourceFingerprint(string[] FingerprintFiles)
	{
		string RepositoryRoot = GetRepositoryRoot();
		using (MemoryStream Payload = new MemoryStream())
		using (BinaryWriter Writer = new BinaryWriter(Payload, Encoding.UTF8, true))
		using (SHA256 Sha = SHA256.Create())
		{
			foreach (string SourceFile in FingerprintFiles)
			{
				Writer.Write(Path.GetRelativePath(RepositoryRoot, SourceFile).Replace('\\', '/'));
				byte[] Bytes = File.ReadAllBytes(SourceFile);
				Writer.Write(Bytes.Length);
				Writer.Write(Bytes);
			}
			Writer.Flush();
			return BitConverter.ToString(Sha.ComputeHash(Payload.ToArray()))
				.Replace("-", string.Empty)
				.ToLowerInvariant();
		}
	}
}
