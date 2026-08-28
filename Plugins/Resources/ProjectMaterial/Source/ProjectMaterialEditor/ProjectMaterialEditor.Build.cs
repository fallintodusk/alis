// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

using System;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnrealBuildTool;

public class ProjectMaterialEditor : ModuleRules
{
	public ProjectMaterialEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		string[] FingerprintFiles = GetCompilerFingerprintFiles();
		ExternalDependencies.AddRange(FingerprintFiles.Select(
			PathName => Path.GetRelativePath(ModuleDirectory, PathName)));
		PrivateDefinitions.Add(
			"PROJECT_MATERIAL_COMPILER_SOURCE_SHA256=\"" +
			ComputeCompilerSourceFingerprint(FingerprintFiles) + "\"");

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AssetRegistry",
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"MaterialEditor",
			"Projects",
			"UnrealEd"
		});
	}

	private string[] GetCompilerFingerprintFiles()
	{
		string PluginRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		string SchemaRoot = Path.Combine(PluginRoot, "Data", "Schemas");
		return Directory.GetFiles(ModuleDirectory, "*.*", SearchOption.AllDirectories)
			.Where(PathName =>
				(PathName.EndsWith(".cpp", StringComparison.OrdinalIgnoreCase) ||
				 PathName.EndsWith(".h", StringComparison.OrdinalIgnoreCase)) &&
				 PathName.IndexOf(
					Path.DirectorySeparatorChar + "Tests" + Path.DirectorySeparatorChar,
					StringComparison.OrdinalIgnoreCase) < 0)
			.Concat(Directory.GetFiles(SchemaRoot, "*.json", SearchOption.AllDirectories))
			.Concat(new[] { Path.Combine(ModuleDirectory, "ProjectMaterialEditor.Build.cs") })
			.OrderBy(PathName => Path.GetRelativePath(PluginRoot, PathName), StringComparer.Ordinal)
			.ToArray();
	}

	private string ComputeCompilerSourceFingerprint(string[] FingerprintFiles)
	{
		string PluginRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		using (MemoryStream Payload = new MemoryStream())
		using (BinaryWriter Writer = new BinaryWriter(Payload, Encoding.UTF8, true))
		using (SHA256 Sha = SHA256.Create())
		{
			foreach (string SourceFile in FingerprintFiles)
			{
				Writer.Write(Path.GetRelativePath(PluginRoot, SourceFile).Replace('\\', '/'));
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
