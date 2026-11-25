using UnrealBuildTool;
using System.Collections.Generic;

public class InvetoryTarget : TargetRules
{
	public InvetoryTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Invetory");
	}
}
