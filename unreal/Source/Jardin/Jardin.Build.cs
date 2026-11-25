using UnrealBuildTool;

public class Jardin : ModuleRules
{
	public Jardin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// El nucleo se valida bit a bit contra la implementacion de referencia.
		// En Windows, Unreal usa /fp:fast por defecto; esa optimizacion cambia
		// algunos redondeos del ruido y, por tanto, el checksum aunque el paisaje
		// se vea igual. /fp:precise conserva la paridad con el harness independiente.
		FPSemantics = FPSemanticsMode.Precise;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",

			// Malla del terreno construida por codigo.
			"ProceduralMeshComponent",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Necesarios para crear las texturas de los mapas de depuracion.
			"RenderCore",
			"RHI",

			// Mallas estaticas creadas en tiempo de ejecucion (matas de pasto).
			"MeshDescription",
			"StaticMeshDescription",
		});

		// El nucleo de generacion vive en Core/ y es C++ plano, sin tipos de
		// Unreal. UnrealBuildTool compila sus .cpp junto al modulo, pero el
		// codigo sigue siendo independiente del motor y se puede compilar
		// aparte con cl.exe para probarlo.
		PublicIncludePaths.Add(ModuleDirectory);
	}
}
