using UnrealBuildTool;

public class AegisMotionEditor : ModuleRules
{
    public AegisMotionEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",

            // Runtime module (so we can reference FAnimNode_AegisLocalHingeBone)
            "AegisMotion",

            // AnimGraph editor integration
            "AnimGraph",
            "AnimGraphRuntime",
            "BlueprintGraph",
            "UnrealEd"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
            "EditorStyle"
        });
    }
}
