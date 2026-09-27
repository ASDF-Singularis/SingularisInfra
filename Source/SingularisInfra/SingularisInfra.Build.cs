using UnrealBuildTool;

public class SingularisInfra : ModuleRules
{
	public SingularisInfra(ReadOnlyTargetRules target) : base(target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			[
				"Core",
				"CoreUObject",
				"Engine",
				"NetCore",

				"InputCore",
				"EnhancedInput",

				"GameplayTags"
			]
		);
	}
}