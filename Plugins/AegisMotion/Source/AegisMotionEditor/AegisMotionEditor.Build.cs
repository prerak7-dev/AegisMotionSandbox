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
            "AegisMotion",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "AnimGraph",
            "BlueprintGraph",
            "GraphEditor",
            "KismetCompiler",
            "Slate",
            "SlateCore",
            "EditorFramework",
        });
    }
}
