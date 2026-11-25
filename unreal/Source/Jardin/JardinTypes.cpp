#include "JardinTypes.h"

jardin::GenerationSettings FJardinGenerationSettings::ToCore() const
{
	jardin::GenerationSettings S;

	S.Seed = Seed;

	// El nucleo trabaja en metros; estos campos ya estan en metros.
	S.WorldSize = WorldSizeMeters;
	S.Resolution = Resolution;
	S.MaxHeight = MaxHeightMeters;

	S.Frequency = Frequency;
	S.Octaves = Octaves;
	S.Lacunarity = Lacunarity;
	S.Gain = Gain;
	S.HeightExponent = HeightExponent;

	S.WarpStrength = WarpStrength;
	S.WarpFrequency = WarpFrequency;
	S.RidgeWeight = RidgeWeight;
	S.MountainFrequency = MountainFrequency;
	S.TerraceWeight = TerraceWeight;
	S.TerraceSteps = TerraceSteps;
	S.TerraceFrequency = TerraceFrequency;
	S.ReliefVariation = ReliefVariation;

	S.PathMode = bPathEnabled ? 1 : 0;
	S.PathWidth = PathWidthMeters;
	S.PathShoulder = PathShoulderMeters;
	S.PathControlSpacing = PathControlSpacingMeters;

	S.WaterMode = static_cast<int32_t>(WaterMode);
	S.WaterMinAreaM2 = WaterMinAreaM2;
	S.WaterMoistureBoost = WaterMoistureBoost;
	S.WaterMoistureRadius = WaterMoistureRadiusMeters;

	S.MoistureFrequency = MoistureFrequency;
	S.MoistureOctaves = MoistureOctaves;

	// Validate acota cualquier valor absurdo antes de generar.
	S.Validate();

	return S;
}

jardin::FractalTreeParams FJardinFractalSettings::ToCore() const
{
	jardin::FractalTreeParams P;
	P.Depth = Depth;
	P.Branches = Branches;
	P.TrunkLength = TrunkLengthMeters;
	P.TrunkRadius = TrunkRadiusMeters;
	P.LengthDecay = LengthDecay;
	P.RadiusDecay = RadiusDecay;
	P.BranchAngleDeg = BranchAngleDegrees;
	P.AngleJitterDeg = AngleJitterDegrees;
	P.UpBias = UpBias;
	P.LeafSize = LeafSizeMeters;
	P.SegmentsPerBranch = SegmentsPerBranch;
	P.BendDeg = BendDegrees;
	P.BranchStart = BranchStart;
	P.TrunkFlare = TrunkFlare;
	P.LeafShape = bRoundLeaves ? 1 : 0;
	P.LeafLevels = LeafLevels;
	P.LeavesPerTip = LeavesPerTip;
	P.LeafJitter = LeafJitter;
	return P;   // Build() llama a Validate(): los limites de presupuesto se aplican ahi
}

jardin::PropLayer FJardinPropLayer::ToCore() const
{
	jardin::PropLayer L;

	// El nombre alimenta el hash de la subsemilla de la capa: cambiarlo
	// cambia donde caen sus objetos, pero no afecta a las demas capas.
	L.Name = TCHAR_TO_UTF8(*Name);

	L.Spacing = SpacingMeters;
	L.ExclusionRadius = ExclusionRadiusMeters;
	L.DensityFrequency = DensityFrequency;
	L.DensityThreshold = DensityThreshold;

	L.MaxSlope = MaxSlope;
	L.MinHeight01 = MinHeight01;
	L.MaxHeight01 = MaxHeight01;
	L.BoundsMargin = BoundsMarginMeters;

	L.AllowedZones.clear();
	L.AllowedZones.reserve(AllowedZones.Num());
	for (uint8 Z : AllowedZones)
	{
		L.AllowedZones.push_back(Z);
	}

	L.MinScale = MinScale;
	L.MaxScale = MaxScale;
	L.MaxCount = MaxCount;

	// Una capa sin ninguna malla sigue calculandose con una variante: asi el
	// reparto no depende de si la capa ya tiene modelos asignados.
	L.VariantCount = FMath::Max(1, EffectiveMeshes().Num());

	// Ecologia. El nucleo trabaja con grupos genericos; aqui solo existe uno
	// con nombre: el dosel (grupo 1), los arboles que dan sombra.
	constexpr int32 CanopyGroup = 1;
	L.MinMoisture = MinMoisture;
	L.MaxMoisture = MaxMoisture;
	L.Clustering = Clustering;
	L.Group = bIsCanopy ? CanopyGroup : 0;
	L.MinWaterDistance = MinWaterDistanceMeters;
	L.MinPathDistance = MinPathDistanceMeters;
	L.NearGroup = bRequireCanopy ? CanopyGroup : -1;
	L.NearDistance = CanopyDistanceMeters;
	L.AvoidGroup = bAvoidCanopy ? CanopyGroup : -1;
	L.AvoidDistance = ClearingDistanceMeters;

	return L;
}

TArray<UStaticMesh*> FJardinPropLayer::EffectiveMeshes() const
{
	TArray<UStaticMesh*> Out;
	for (const TObjectPtr<UStaticMesh>& M : Variants)
	{
		if (M) Out.Add(M.Get());
	}
	if (Out.Num() == 0 && Mesh)
	{
		Out.Add(Mesh.Get());
	}
	return Out;
}

TArray<FJardinPropLayer> FJardinPropLayer::DefaultLayers()
{
	TArray<FJardinPropLayer> Layers;

	FJardinPropLayer ArbolA;
	ArbolA.Name = TEXT("ArbolA");
	ArbolA.SpacingMeters = 4.5f;
	ArbolA.ExclusionRadiusMeters = 2.4f;
	ArbolA.MaxSlope = 0.32f;
	ArbolA.MinHeight01 = 0.05f;
	ArbolA.MaxHeight01 = 0.85f;
	ArbolA.AllowedZones = { 1 };          // solo Bosque
	ArbolA.DensityFrequency = 2.2f;
	ArbolA.DensityThreshold = 0.20f;
	ArbolA.MinScale = 0.85f;
	ArbolA.MaxScale = 1.30f;

	FJardinPropLayer ArbolB;
	ArbolB.Name = TEXT("ArbolB");
	ArbolB.SpacingMeters = 7.0f;
	ArbolB.ExclusionRadiusMeters = 2.0f;
	ArbolB.MaxSlope = 0.30f;
	ArbolB.MinHeight01 = 0.05f;
	ArbolB.MaxHeight01 = 0.75f;
	ArbolB.AllowedZones = { 0, 1 };       // Pradera y Bosque
	ArbolB.DensityFrequency = 3.0f;
	ArbolB.DensityThreshold = 0.55f;
	ArbolB.MinScale = 0.80f;
	ArbolB.MaxScale = 1.20f;

	FJardinPropLayer Roca;
	Roca.Name = TEXT("Roca");
	Roca.SpacingMeters = 3.5f;
	Roca.ExclusionRadiusMeters = 0.9f;
	Roca.MaxSlope = 0.60f;
	Roca.MinHeight01 = 0.02f;
	Roca.MaxHeight01 = 1.0f;
	Roca.AllowedZones = {};               // todas
	Roca.DensityFrequency = 4.0f;
	Roca.DensityThreshold = 0.62f;
	Roca.MinScale = 0.60f;
	Roca.MaxScale = 1.60f;

	Layers.Add(ArbolA);
	Layers.Add(ArbolB);
	Layers.Add(Roca);

	return Layers;
}
