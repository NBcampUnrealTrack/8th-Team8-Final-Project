#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Game/Subsystem/SFFogOfWarSubsystem.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "InGame/HUD/Minimap/SFMinimapPanelWidget.h"
#include "HAL/PlatformTime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFogExplorationTest, "StrikeForge.FogOfWar.Exploration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFFogExplorationTest::RunTest(const FString& Parameters)
{
	FSFFogTeamGrid Allies, Enemies;
	const FVector Start(50., 50., 0.);
	TArray<int32> Handles;
	Allies.AddVision(Start, 250.f, Handles);
	TestTrue(TEXT("A living source reveals its position"), Allies.IsVisible(Start));
	TestFalse(TEXT("Enemy exploration is independent"), Enemies.IsExplored(Start));
	TestFalse(TEXT("Circle corners outside sight stay hidden"), Allies.IsVisible(FVector(250., 250., 0.)));

	Allies.RemoveVision(Handles);
	Allies.AddVision(FVector(1050., 50., 0.), 250.f, Handles);
	TestFalse(TEXT("Moving away closes current sight"), Allies.IsVisible(Start));
	TestTrue(TEXT("Exploration survives movement or source death"), Allies.IsExplored(Start));
	TestTrue(TEXT("New location is explored"), Allies.IsExplored(FVector(1050., 50., 0.)));

	Allies.RemoveVision(Handles);
	Allies.AddVision(Start, 0.f, Handles);
	TestTrue(TEXT("Zero sight reveals nothing"), Handles.IsEmpty());
	TestEqual(TEXT("Negative world coordinates use floor, not truncation"),
		FSFFogTeamGrid::ToCell(FVector(-1., -101., 0.)), FIntPoint(-1, -3));
	Allies.AddVision(FVector(-50., -50., 0.), 50.f, Handles);
	TestTrue(TEXT("Vision works across the world origin"), Allies.IsVisible(FVector(-50., -50., 0.)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFogCircularEdgeTest, "StrikeForge.FogOfWar.CircularEdges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFFogCircularEdgeTest::RunTest(const FString& Parameters)
{
	FSFFogTeamGrid Grid;
	TArray<int32> Handles;
	Grid.AddVision(FVector::ZeroVector, 1000.f, Handles);
	const float Interior = Grid.SampleCoverage(Grid.Visible, FVector(850., 0., 0.));
	const float Edge = Grid.SampleCoverage(Grid.Visible, FVector(950., 0., 0.));
	const float Outside = Grid.SampleCoverage(Grid.Visible, FVector(1100., 0., 0.));
	TestTrue(TEXT("Edge fades between fully visible interior and black exterior"), Interior > Edge && Edge > Outside);
	TestEqual(TEXT("Unexplored exterior remains black"), Outside, 0.f);
	const float Diagonal = Grid.SampleCoverage(Grid.Visible, FVector(950. / FMath::Sqrt(2.), 950. / FMath::Sqrt(2.), 0.));
	TestTrue(TEXT("Equal distances in axial and diagonal directions have similar circular coverage"), FMath::Abs(Edge - Diagonal) < 0.1f);
	const float Left = Grid.SampleCoverage(Grid.Visible, FVector(949., 0., 0.));
	const float Right = Grid.SampleCoverage(Grid.Visible, FVector(951., 0., 0.));
	TestTrue(TEXT("Moving across a grid boundary does not jump from black to visible"), FMath::Abs(Left - Right) < 0.05f);
	Grid.RemoveVision(Handles);
	TestEqual(TEXT("Exploration keeps the same soft edge after sight closes"), Grid.SampleCoverage(Grid.Explored, FVector(950., 0., 0.)), Edge);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFogSourceLifecycleTest, "StrikeForge.FogOfWar.SourceLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFFogSourceLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	USFFogOfWarSubsystem* Fog = World->GetSubsystem<USFFogOfWarSubsystem>();
	if (!TestNotNull(TEXT("Fog subsystem is automatically created"), Fog)) { World->DestroyWorld(false); return false; }
	Fog->OnWorldBeginPlay(*World);
	auto SpawnSource = [World](const FVector& Location, int32 Team)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Actor);
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Actor->SetActorLocation(Location);
		UHealthComponent* Health = NewObject<UHealthComponent>(Actor);
		Health->RegisterComponent();
		Health->InitializeHealth(100.f);
		UCombatTargetComponent* Combat = NewObject<UCombatTargetComponent>(Actor);
		Combat->SightRadius = 250.f;
		Combat->SetTeamIndex(Team);
		Combat->RegisterComponent();
		Actor->DispatchBeginPlay();
		return Combat;
	};
	UCombatTargetComponent* Ally = SpawnSource(FVector(50., 50., 0.), 0);
	UCombatTargetComponent* Enemy = SpawnSource(FVector(1050., 50., 0.), 1);
	Fog->Tick(1.f / 60.f);
	TestTrue(TEXT("Ally reveals its team"), Fog->IsLocationVisible(0, Ally->GetOwner()->GetActorLocation()));
	TestFalse(TEXT("Enemy sight does not reveal the player's map"), Fog->IsLocationVisible(0, Enemy->GetOwner()->GetActorLocation()));
	TestFalse(TEXT("Cannot attack outside personal sight"), Ally->IsWithinSight(Enemy->GetOwner()));
	Enemy->GetOwner()->SetActorLocation(FVector(250., 50., 0.));
	TestTrue(TEXT("Player detects a nearby enemy"), Ally->IsWithinSight(Enemy->GetOwner()));
	TestTrue(TEXT("Enemy uses the same sight rule"), Enemy->IsWithinSight(Ally->GetOwner()));
	Enemy->GetOwner()->SetActorLocation(FVector(1050., 50., 0.));
	Ally->GetHealthComponent()->ApplyDamage(100.f);
	Fog->Tick(1.f / 60.f);
	TestFalse(TEXT("Dead sources immediately stop providing sight"), Fog->IsLocationVisible(0, FVector(50., 50., 0.)));
	TestTrue(TEXT("Death preserves exploration"), Fog->IsLocationExplored(0, FVector(50., 50., 0.)));
	Enemy->SetTeamIndex(0);
	Fog->Tick(1.f / 60.f);
	TestTrue(TEXT("Team changes transfer current vision"), Fog->IsLocationVisible(0, Enemy->GetOwner()->GetActorLocation()));
	TestFalse(TEXT("Old team loses current vision"), Fog->IsLocationVisible(1, Enemy->GetOwner()->GetActorLocation()));
	Enemy->GetOwner()->SetActorLocation(FVector(2050., 50., 0.));
	Fog->Tick(1.f / 60.f);
	TestTrue(TEXT("Movement reveals the new position on the next frame without a 100ms wait"), Fog->IsLocationVisible(0, FVector(2050., 50., 0.)));
	TestFalse(TEXT("Movement closes the old position on the same frame"), Fog->IsLocationVisible(0, FVector(1050., 50., 0.)));
	TestTrue(TEXT("Movement keeps old exploration"), Fog->IsLocationExplored(0, FVector(1050., 50., 0.)));
	Fog->Tick(1.f / 60.f);
	TestTrue(TEXT("Stationary sources retain vision when no grid rebuild is needed"), Fog->IsLocationVisible(0, FVector(2050., 50., 0.)));
	Enemy->SightRadius = 0.f;
	Fog->Tick(1.f / 60.f);
	TestFalse(TEXT("Changing sight radius to zero closes sight next frame"), Fog->IsLocationVisible(0, FVector(2050., 50., 0.)));
	Enemy->SightRadius = 250.f;
	Fog->Tick(1.f / 60.f);
	TestTrue(TEXT("Restoring radius restores sight next frame"), Fog->IsLocationVisible(0, FVector(2050., 50., 0.)));
	Fog->UnregisterSource(Enemy);
	Fog->Tick(1.f / 60.f);
	TestFalse(TEXT("Unregister closes old sight without waiting"), Fog->IsLocationVisible(0, FVector(2050., 50., 0.)));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFogTextureRegionTest, "StrikeForge.FogOfWar.TextureRegions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFFogTextureRegionTest::RunTest(const FString& Parameters)
{
	constexpr int32 Size = 128;
	const FVector2D MapMin(-3200., -3200.), MapMax(3200., 3200.);
	const FIntRect Full = SFFogOfWar::TextureRectForBounds(FBox2D(MapMin, MapMax), MapMin, MapMax, Size);
	TestEqual(TEXT("Full upload covers the texture including its last row/column"), Full, FIntRect(0, 0, Size, Size));
	const FIntRect OffMap = SFFogOfWar::TextureRectForBounds(FBox2D(FVector2D(4000.), FVector2D(5000.)), MapMin, MapMax, Size);
	TestEqual(TEXT("Off-map source does not upload pixels"), OffMap.Area(), 0);
	TestEqual(TEXT("Minimap rotates a non-square partial rectangle"),
		SFFogOfWar::MinimapRect(FIntRect(10, 20, 30, 60), Size), FIntRect(20, 98, 60, 118));

	// Compare dirty coverage to a full recompute at the center and all map edges.
	for (const FVector Origin : { FVector::ZeroVector, FVector(-3150., 0., 0.),
		FVector(3150., 0., 0.), FVector(0., -3150., 0.), FVector(0., 3150., 0.) })
	{
		const FVector Moved = Origin + FVector(110., 30., 0.);
		FSFFogTeamGrid Before;
		TArray<int32> Moving, Fixed;
		Before.AddVision(Origin, 350.f, Moving);
		Before.AddVision(Origin + FVector(0., 300., 0.), 250.f, Fixed);
		FSFFogTeamGrid After = Before;
		After.RemoveVision(Moving);
		After.AddVision(Moved, 350.f, Moving);
		const FVector2D Extent(350.f + FSFFogTeamGrid::CellSize * 2.f);
		FBox2D Dirty(FVector2D(Origin) - Extent, FVector2D(Origin) + Extent);
		Dirty += FBox2D(FVector2D(Moved) - Extent, FVector2D(Moved) + Extent);
		const FIntRect Rect = SFFogOfWar::TextureRectForBounds(Dirty, MapMin, MapMax, Size);
		const FIntRect MiniRect = SFFogOfWar::MinimapRect(Rect, Size);
		bool bAllChangesCovered = true, bMinimapMatches = true;
		for (int32 Y = 0; Y < Size; ++Y)
			for (int32 X = 0; X < Size; ++X)
			{
				const FVector Location(MapMin.X + (X + 0.5) / Size * (MapMax.X - MapMin.X),
					MapMin.Y + (Y + 0.5) / Size * (MapMax.Y - MapMin.Y), 0.);
				if (!Rect.Contains(FIntPoint(X, Y)))
				{
					bAllChangesCovered &= Before.SampleCoverage(Before.Visible, Location) == After.SampleCoverage(After.Visible, Location);
					bAllChangesCovered &= Before.SampleCoverage(Before.Explored, Location) == After.SampleCoverage(After.Explored, Location);
				}
				else
				{
					const int32 LocalIndex = (Rect.Max.X - 1 - X) * Rect.Height() + Y - Rect.Min.Y;
					const FIntPoint Destination(MiniRect.Min.X + LocalIndex % MiniRect.Width(), MiniRect.Min.Y + LocalIndex / MiniRect.Width());
					bMinimapMatches &= Destination == FIntPoint(Y, Size - 1 - X);
				}
			}
		TestTrue(TEXT("Partial update includes every changed visible/explored texel, including overlapping sources and map edges"), bAllChangesCovered);
		TestTrue(TEXT("Partial minimap upload uses exactly the same orientation as a full upload"), bMinimapMatches);
	}
	return true;
}

// Independent reference: the pre-optimization sparse, full-rebuild implementation.
namespace
{
struct FFogReference
{
    TMap<FIntPoint, uint8> Visible, Explored;
    void Reveal(FVector Origin, float Radius)
    {
        if (Radius <= 0.f) return;
        const FIntPoint Min = FSFFogTeamGrid::ToCell(Origin - FVector(Radius, Radius, 0.));
        const FIntPoint Max = FSFFogTeamGrid::ToCell(Origin + FVector(Radius, Radius, 0.));
        for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
            for (int32 X = Min.X; X <= Max.X; ++X)
            {
                const float Distance = FVector::Dist2D(Origin, FVector((X + .5) * 50., (Y + .5) * 50., Origin.Z));
                if (Distance < Radius || FSFFogTeamGrid::ToCell(Origin) == FIntPoint(X, Y))
                {
                    const uint8 Value = FMath::Clamp(FMath::RoundToInt(FMath::SmoothStep(0.f, FMath::Min(75.f, Radius), Radius - Distance) * 255.f), 1, 255);
                    uint8& V = Visible.FindOrAdd(FIntPoint(X, Y)); V = FMath::Max(V, Value);
                    uint8& E = Explored.FindOrAdd(FIntPoint(X, Y)); E = FMath::Max(E, Value);
                }
            }
    }
    float Sample(const TMap<FIntPoint, uint8>& Cells, FVector Location) const
    {
        const FVector2D P(Location.X / 50. - .5, Location.Y / 50. - .5);
        const int32 X = FMath::FloorToInt(P.X), Y = FMath::FloorToInt(P.Y);
        auto At = [&Cells](int32 A, int32 B) { const uint8* V = Cells.Find(FIntPoint(A, B)); return V ? *V / 255.f : 0.f; };
        return FMath::Lerp(FMath::Lerp(At(X,Y), At(X+1,Y), P.X-X), FMath::Lerp(At(X,Y+1), At(X+1,Y+1), P.X-X), P.Y-Y);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFogIncrementalTest, "StrikeForge.FogOfWar.DenseIncremental",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFFogIncrementalTest::RunTest(const FString& Parameters)
{
    FSFFogTeamGrid Grid;
    FFogReference Reference;
    TArray<FVector> Positions;
    TArray<float> Radii;
    TArray<TArray<int32>> Handles;
    Positions.Init(FVector::ZeroVector, 8); Radii.Init(0.f, 8); Handles.SetNum(8);
    FRandomStream Random(4321);
    bool bEqual = true;
    for (int32 Step = 0; Step < 100; ++Step)
    {
        const int32 Source = Step % 8;
        Grid.RemoveVision(Handles[Source]);
        Positions[Source] = FVector(Random.FRandRange(-1700.f, 1700.f), Random.FRandRange(-1700.f, 1700.f), 0.);
        Radii[Source] = Step % 7 == 0 ? 0.f : Random.FRandRange(5.f, 800.f);
        Grid.AddVision(Positions[Source], Radii[Source], Handles[Source]);
        Reference.Visible.Reset();
        for (int32 I = 0; I < 8; ++I) Reference.Reveal(Positions[I], Radii[I]);
        for (int32 Y = 0; Y < Grid.Dimensions.Y; ++Y)
            for (int32 X = 0; X < Grid.Dimensions.X; ++X)
            {
                const int32 I = Y * Grid.Dimensions.X + X;
                const FIntPoint Cell = Grid.MinCell + FIntPoint(X,Y);
                bEqual &= Grid.Visible[I] == Reference.Visible.FindRef(Cell);
                bEqual &= Grid.Explored[I] == Reference.Explored.FindRef(Cell);
                bEqual &= (Grid.VisibleCount[I] > 0) == Reference.Visible.Contains(Cell);
            }
    }
    TestTrue(TEXT("100 moves/removals/radius changes with fractional and negative coordinates exactly match the old max-coverage/history"), bEqual);
    for (auto& SourceHandles : Handles) Grid.RemoveVision(SourceHandles);
    bool bAllClosed = true;
    for (const uint32 Count : Grid.VisibleCount) bAllClosed &= Count == 0;
    TestTrue(TEXT("All contributions can be removed even after dense grid expansions"), bAllClosed);
    TArray<int32> A, B;
    Grid.AddVision(FVector::ZeroVector, 1000.f, A);
    Grid.AddVision(FVector(80., 0., 0.), 1000.f, B);
    const FVector Edge(975., 25., 0.);
    const int32 I = Grid.Index(FSFFogTeamGrid::ToCell(Edge));
    TestEqual(TEXT("Overlapping sources have separate counts"), Grid.VisibleCount[I], uint32(2));
    Grid.RemoveVision(B);
    FFogReference One; One.Reveal(FVector::ZeroVector, 1000.f);
    TestEqual(TEXT("Removing stronger coverage restores the weaker remaining edge"), Grid.Visible[I], One.Visible.FindRef(FSFFogTeamGrid::ToCell(Edge)));
    TestEqual(TEXT("One source remains after removing overlap"), Grid.VisibleCount[I], uint32(1));
    Grid.RemoveVision(A);
    TestEqual(TEXT("Last removal clears visibility"), Grid.Visible[I], uint8(0));
    TestTrue(TEXT("Last removal keeps smooth exploration"), Grid.Explored[I] > 0);

    TArray<FIntRect> Rects;
    SFFogOfWar::MergeRect(Rects, FIntRect(0, 0, 10, 10));
    SFFogOfWar::MergeRect(Rects, FIntRect(100, 100, 110, 110));
    TestEqual(TEXT("Distant moves stay in separate upload regions"), Rects.Num(), 2);
    SFFogOfWar::MergeRect(Rects, FIntRect(8, 8, 12, 12));
    TestEqual(TEXT("Overlapping changes merge"), Rects.Num(), 2);
    for (int32 N = 0; N < 40; ++N) SFFogOfWar::MergeRect(Rects, FIntRect(N * 12, 300, N * 12 + 2, 302));
    TestTrue(TEXT("Upload region count is bounded"), Rects.Num() <= 16);
    bool bCovered = true;
    for (int32 N = 0; N < 40; ++N)
        bCovered &= Rects.ContainsByPredicate([N](const FIntRect& Rect) { return Rect.Contains(FIntPoint(N * 12, 300)); });
    TestTrue(TEXT("Bounded merging does not lose any changed pixel"), bCovered);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFogBenchmarkTest, "StrikeForge.FogOfWar.CPUBenchmark",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFFogBenchmarkTest::RunTest(const FString& Parameters)
{
    // CPU-only synthetic measurement, not a claim about PIE frame time or GPU cost.
    constexpr int32 Sources = 30, Steps = 90;
    TArray<FVector> Positions;
    TArray<TArray<int32>> Handles; Handles.SetNum(Sources);
    FSFFogTeamGrid Grid; FFogReference Reference;
    Grid.EnsureBounds(FIntPoint(-100,-100), FIntPoint(100,100));
    for (int32 I = 0; I < Sources; ++I)
    {
        Positions.Add(FVector((I % 6) * 800. - 2000., (I / 6) * 800. - 1600., 0.));
        Grid.AddVision(Positions[I], 1200.f, Handles[I]);
        Reference.Reveal(Positions[I], 1200.f);
    }
    double Start = FPlatformTime::Seconds();
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        Reference.Visible.Reset();
        for (int32 I = 0; I < Sources; ++I) Reference.Reveal(Positions[I] + (I == 0 ? FVector(Step * 3., 0., 0.) : FVector::ZeroVector), 1200.f);
    }
    const double OldVisionMs = (FPlatformTime::Seconds() - Start) * 1000. / Steps;
    Start = FPlatformTime::Seconds();
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        Grid.RemoveVision(Handles[0]);
        Grid.AddVision(Positions[0] + FVector(Step * 3., 0., 0.), 1200.f, Handles[0]);
    }
    const double NewVisionMs = (FPlatformTime::Seconds() - Start) * 1000. / Steps;
    Start = FPlatformTime::Seconds();
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        Reference.Visible.Reset();
        for (int32 I = 0; I < Sources; ++I) Reference.Reveal(Positions[I] + FVector(Step * 3., Step * 2., 0.), 1200.f);
    }
    const double OldAllMovingMs = (FPlatformTime::Seconds() - Start) * 1000. / Steps;
    Start = FPlatformTime::Seconds();
    for (int32 Step = 0; Step < Steps; ++Step)
        for (int32 I = 0; I < Sources; ++I)
        {
            Grid.RemoveVision(Handles[I]);
            Grid.AddVision(Positions[I] + FVector(Step * 3., Step * 2., 0.), 1200.f, Handles[I]);
        }
    const double NewAllMovingMs = (FPlatformTime::Seconds() - Start) * 1000. / Steps;
    constexpr int32 Size = 512, Passes = 4;
    double OldSum = 0., NewSum = 0.;
    Start = FPlatformTime::Seconds();
    for (int32 Pass = 0; Pass < Passes; ++Pass)
        for (int32 Y = 0; Y < Size; ++Y)
            for (int32 X = 0; X < Size; ++X)
            {
                const FVector P(-5000. + (X+.5) / Size * 10000., -5000. + (Y+.5) / Size * 10000., 0.);
                OldSum += Reference.Sample(Reference.Visible, P) + Reference.Sample(Reference.Explored, P);
            }
    const double OldTextureMs = (FPlatformTime::Seconds() - Start) * 1000. / Passes;
    Start = FPlatformTime::Seconds();
    for (int32 Pass = 0; Pass < Passes; ++Pass)
        for (int32 Y = 0; Y < Size; ++Y)
            for (int32 X = 0; X < Size; ++X)
            {
                const FVector P(-5000. + (X+.5) / Size * 10000., -5000. + (Y+.5) / Size * 10000., 0.);
                NewSum += Grid.SampleCoverage(Grid.Visible, P) + Grid.SampleCoverage(Grid.Explored, P);
            }
    const double NewTextureMs = (FPlatformTime::Seconds() - Start) * 1000. / Passes;
    TestTrue(TEXT("Array and map sampling have identical checksum"), FMath::Abs(OldSum - NewSum) < 0.001);
    AddInfo(FString::Printf(TEXT("FOG_CPU_BENCH 30 sources, 1 moving: vision old=%.3fms new=%.3fms; full 512x512 sampling old=%.3fms new=%.3fms; grid+pool=%.2fMiB. GPU/upload/UI excluded."),
        OldVisionMs, NewVisionMs, OldTextureMs, NewTextureMs, Grid.GetAllocatedSize() / 1048576.));
    AddInfo(FString::Printf(TEXT("FOG_CPU_BENCH all 30 moving: vision old=%.3fms new=%.3fms."), OldAllMovingMs, NewAllMovingMs));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMinimapRegistryTest, "StrikeForge.FogOfWar.MinimapRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMinimapRegistryTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Minimap test world"), World)) return false;
    USFMinimapPanelWidget* Panel = NewObject<USFMinimapPanelWidget>(World);
    Panel->WidgetTree = NewObject<UWidgetTree>(Panel);
    Panel->MinimapMarkerLayer = Panel->WidgetTree->ConstructWidget<UCanvasPanel>();
    Panel->WidgetTree->RootWidget = Panel->MinimapMarkerLayer;
    AActor* Existing = World->SpawnActor<AActor>();
    Existing->Tags.Add(TEXT("MinimapAlly")); Existing->DispatchBeginPlay();
    Panel->CreateMinimapMarkers();
    TestEqual(TEXT("HUD bootstraps actors created before it"), Panel->ActorToMarker.Num(), 1);
    Panel->BindMarkerRegistry();
    AActor* Spawned = World->SpawnActor<AActor>();
    Panel->ProcessPendingMarkers();
    TestEqual(TEXT("Registration waits for BeginPlay and late spawn tags"), Panel->ActorToMarker.Num(), 1);
    Spawned->Tags.Add(TEXT("MinimapEnemy")); Spawned->DispatchBeginPlay();
    Panel->ProcessPendingMarkers();
    Panel->QueueMarkerActor(Spawned); Panel->ProcessPendingMarkers();
    TestEqual(TEXT("Spawn event registers exactly one marker without a world scan"), Panel->ActorToMarker.Num(), 2);
    Spawned->Tags.Remove(TEXT("MinimapEnemy")); Spawned->Tags.Add(TEXT("MinimapAlly"));
    Panel->UpdateMinimapMarkers(1.f / 60.f);
    TestEqual(TEXT("Cached markers update classification after tag changes"), Panel->MinimapMarkerEntries[Panel->ActorToMarker.FindChecked(Spawned)].MarkerType, ESFMinimapMarkerType::Ally);
    Existing->Destroy(); Panel->UpdateMinimapMarkers(1.f / 60.f);
    TestEqual(TEXT("Removal deletes its marker and repairs swapped index"), Panel->ActorToMarker.Num(), 1);
    TestEqual(TEXT("Remaining index points at its own actor"), Panel->MinimapMarkerEntries[Panel->ActorToMarker.FindChecked(Spawned)].TargetActor.Get(), Spawned);
    Panel->ClearMinimapMarkers(); Panel->CreateMinimapMarkers();
    TestEqual(TEXT("HUD rebuild does not duplicate markers"), Panel->ActorToMarker.Num(), 1);
    AActor* Unit = World->SpawnActor<AActor>();
    Unit->Tags.Add(TEXT("MinimapAlly"));
    UHealthComponent* Health = NewObject<UHealthComponent>(Unit);
    Health->RegisterComponent(); Health->InitializeHealth(100.f);
    UCombatTargetComponent* Target = NewObject<UCombatTargetComponent>(Unit);
    Target->SetTeamIndex(0); Target->RegisterComponent(); Unit->DispatchBeginPlay();
    Panel->ProcessPendingMarkers();
    TestTrue(TEXT("Component BeginPlay registers gameplay actor"), Panel->ActorToMarker.Contains(Unit));
    Target->SetTeamIndex(INDEX_NONE); Panel->UpdateMinimapMarkers(1.f / 60.f);
    TestFalse(TEXT("Neutral transition removes combat marker"), Panel->ActorToMarker.Contains(Unit));
    Target->SetTeamIndex(0); Panel->ProcessPendingMarkers();
    TestTrue(TEXT("Team notification restores previously absent marker without scanning"), Panel->ActorToMarker.Contains(Unit));
    Health->ApplyDamage(100.f); Panel->UpdateMinimapMarkers(1.f / 60.f);
    TestFalse(TEXT("Death removes marker while corpse actor still exists"), Panel->ActorToMarker.Contains(Unit));
    Panel->UnbindMarkerRegistry();
    TestFalse(TEXT("HUD teardown disconnects registry delegates"), World->GetSubsystem<USFFogOfWarSubsystem>()->OnSourceRegistered.IsBoundToObject(Panel));
    Panel->ClearMinimapMarkers();
    World->DestroyWorld(false);
    return true;
}
#endif
