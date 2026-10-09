#include "SFFogOfWarSubsystem.h"

#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "InGame/Controller/SFRTSCameraPawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

FIntRect SFFogOfWar::TextureRectForBounds(const FBox2D& Bounds, FVector2D MapMin, FVector2D MapMax, int32 Size)
{
	if (!Bounds.bIsValid || Size <= 0 || MapMax.X <= MapMin.X || MapMax.Y <= MapMin.Y)
		return FIntRect(0, 0, 0, 0);
	const FVector2D Scale = FVector2D(Size) / (MapMax - MapMin);
	const FVector2D Min = (Bounds.Min - MapMin) * Scale;
	const FVector2D Max = (Bounds.Max - MapMin) * Scale;
	// Include neighboring texels for the bilinear grid interpolation and rounding.
	return FIntRect(FMath::Clamp(FMath::FloorToInt(Min.X) - 1, 0, Size),
		FMath::Clamp(FMath::FloorToInt(Min.Y) - 1, 0, Size),
		FMath::Clamp(FMath::CeilToInt(Max.X) + 1, 0, Size),
		FMath::Clamp(FMath::CeilToInt(Max.Y) + 1, 0, Size));
}

FIntRect SFFogOfWar::MinimapRect(const FIntRect& WorldRect, int32 Size)
{
	return FIntRect(WorldRect.Min.Y, Size - WorldRect.Max.X, WorldRect.Max.Y, Size - WorldRect.Min.X);
}

FIntPoint FSFFogTeamGrid::ToCell(const FVector& Location)
{
	return FIntPoint(FMath::FloorToInt(Location.X / CellSize), FMath::FloorToInt(Location.Y / CellSize));
}

int32 FSFFogTeamGrid::Index(FIntPoint Cell) const
{
    const FIntPoint Local = Cell - MinCell;
    return Local.X >= 0 && Local.Y >= 0 && Local.X < Dimensions.X && Local.Y < Dimensions.Y
        ? Local.Y * Dimensions.X + Local.X : INDEX_NONE;
}

void FSFFogTeamGrid::EnsureBounds(FIntPoint Min, FIntPoint Max)
{
    if (Index(Min) != INDEX_NONE && Index(Max) != INDEX_NONE) return;
    if (Dimensions.X > 0)
    {
        Min.X = FMath::Min(Min.X, MinCell.X);
        Min.Y = FMath::Min(Min.Y, MinCell.Y);
        Max.X = FMath::Max(Max.X, MinCell.X + Dimensions.X - 1);
        Max.Y = FMath::Max(Max.Y, MinCell.Y + Dimensions.Y - 1);
    }
    // Grow in chunks; never discard exploration when bounds/camera settings change.
    const FIntPoint NewMin(FMath::FloorToInt(Min.X / 64.) * 64, FMath::FloorToInt(Min.Y / 64.) * 64);
    const FIntPoint NewMax((FMath::FloorToInt(Max.X / 64.) + 1) * 64, (FMath::FloorToInt(Max.Y / 64.) + 1) * 64);
    const FIntPoint NewDimensions = NewMax - NewMin;
    const int32 Num = NewDimensions.X * NewDimensions.Y;
    TArray<uint32> NewCounts; NewCounts.Init(0, Num);
    TArray<uint8> NewVisible, NewExplored;
    NewVisible.Init(0, Num); NewExplored.Init(0, Num);
    TArray<int32> NewHeads; NewHeads.Init(INDEX_NONE, Num);
    for (int32 Y = 0; Y < Dimensions.Y; ++Y)
    {
        const int32 OldStart = Y * Dimensions.X;
        const int32 NewStart = (Y + MinCell.Y - NewMin.Y) * NewDimensions.X + MinCell.X - NewMin.X;
        FMemory::Memcpy(NewCounts.GetData() + NewStart, VisibleCount.GetData() + OldStart, Dimensions.X * sizeof(uint32));
        FMemory::Memcpy(NewVisible.GetData() + NewStart, Visible.GetData() + OldStart, Dimensions.X);
        FMemory::Memcpy(NewExplored.GetData() + NewStart, Explored.GetData() + OldStart, Dimensions.X);
        FMemory::Memcpy(NewHeads.GetData() + NewStart, Heads.GetData() + OldStart, Dimensions.X * sizeof(int32));
    }
    for (FContribution& Node : Contributions)
        if (Node.Cell != INDEX_NONE)
        {
            const FIntPoint Cell(Node.Cell % Dimensions.X + MinCell.X, Node.Cell / Dimensions.X + MinCell.Y);
            Node.Cell = (Cell.Y - NewMin.Y) * NewDimensions.X + Cell.X - NewMin.X;
        }
    MinCell = NewMin; Dimensions = NewDimensions;
    VisibleCount = MoveTemp(NewCounts); Visible = MoveTemp(NewVisible);
    Explored = MoveTemp(NewExplored); Heads = MoveTemp(NewHeads);
}

void FSFFogTeamGrid::AddVision(const FVector& Origin, float Radius, TArray<int32>& OutHandles)
{
    check(OutHandles.IsEmpty());
    if (Radius <= 0.f) return;
    const FIntPoint Min = ToCell(Origin - FVector(Radius, Radius, 0.));
    const FIntPoint Max = ToCell(Origin + FVector(Radius, Radius, 0.));
    const FIntPoint OriginCell = ToCell(Origin);
    EnsureBounds(Min, Max);
    const double RadiusSquared = FMath::Square(double(Radius));
    const double InnerSquared = FMath::Square(double(FMath::Max(0.f, Radius - EdgeSoftness)));
    for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
        for (int32 X = Min.X; X <= Max.X; ++X)
        {
            const double D2 = FMath::Square((X + 0.5) * CellSize - Origin.X) + FMath::Square((Y + 0.5) * CellSize - Origin.Y);
            if (D2 >= RadiusSquared && OriginCell != FIntPoint(X, Y)) continue;
            // Only the 75cm edge needs sqrt. Actual sub-cell position is retained.
            const uint8 Value = D2 <= InnerSquared ? 255 : static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(
                FMath::SmoothStep(0.f, FMath::Min(EdgeSoftness, Radius), Radius - static_cast<float>(FMath::Sqrt(D2))) * 255.f), 1, 255));
            const int32 Cell = Index(FIntPoint(X, Y));
            const int32 Handle = FreeHandles.IsEmpty() ? Contributions.AddDefaulted() : FreeHandles.Pop(EAllowShrinking::No);
            FContribution& Node = Contributions[Handle];
            Node.Cell = Cell; Node.Coverage = Value;
            Node.Previous = INDEX_NONE; Node.Next = Heads[Cell];
            if (Node.Next != INDEX_NONE) Contributions[Node.Next].Previous = Handle;
            Heads[Cell] = Handle;
            ++VisibleCount[Cell];
            Visible[Cell] = FMath::Max(Visible[Cell], Value);
            Explored[Cell] = FMath::Max(Explored[Cell], Value);
            OutHandles.Add(Handle);
        }
}

void FSFFogTeamGrid::RemoveVision(TArray<int32>& Handles)
{
    for (const int32 Handle : Handles)
    {
        FContribution& Node = Contributions[Handle];
        const int32 Cell = Node.Cell;
        check(Cell != INDEX_NONE && VisibleCount[Cell] > 0);
        if (Node.Previous != INDEX_NONE) Contributions[Node.Previous].Next = Node.Next;
        else Heads[Cell] = Node.Next;
        if (Node.Next != INDEX_NONE) Contributions[Node.Next].Previous = Node.Previous;
        --VisibleCount[Cell];
        if (Visible[Cell] == Node.Coverage)
        {
            uint8 Remaining = 0;
            for (int32 Other = Heads[Cell]; Other != INDEX_NONE && Remaining < 255; Other = Contributions[Other].Next)
                Remaining = FMath::Max(Remaining, Contributions[Other].Coverage);
            Visible[Cell] = Remaining;
        }
        Node.Cell = INDEX_NONE;
        FreeHandles.Add(Handle);
    }
    Handles.Reset();
}

float FSFFogTeamGrid::SampleCoverage(const TArray<uint8>& Cells, const FVector& Location) const
{
    const FVector2D Position(Location.X / CellSize - 0.5, Location.Y / CellSize - 0.5);
    const int32 X = FMath::FloorToInt(Position.X), Y = FMath::FloorToInt(Position.Y);
    auto Sample = [this, &Cells](int32 InX, int32 InY)
    {
        const int32 I = Index(FIntPoint(InX, InY));
        return I != INDEX_NONE ? Cells[I] / 255.f : 0.f;
    };
    return FMath::Lerp(FMath::Lerp(Sample(X, Y), Sample(X + 1, Y), Position.X - X),
        FMath::Lerp(Sample(X, Y + 1), Sample(X + 1, Y + 1), Position.X - X), Position.Y - Y);
}

bool FSFFogTeamGrid::IsVisible(const FVector& Location) const
{
    const int32 I = Index(ToCell(Location));
    return I != INDEX_NONE && VisibleCount[I] > 0;
}

bool FSFFogTeamGrid::IsExplored(const FVector& Location) const
{
    const int32 I = Index(ToCell(Location));
    return I != INDEX_NONE && Explored[I] > 0;
}

SIZE_T FSFFogTeamGrid::GetAllocatedSize() const
{
    return VisibleCount.GetAllocatedSize() + Visible.GetAllocatedSize() + Explored.GetAllocatedSize()
        + Heads.GetAllocatedSize() + Contributions.GetAllocatedSize() + FreeHandles.GetAllocatedSize();
}

void SFFogOfWar::MergeRect(TArray<FIntRect>& Rects, FIntRect Rect)
{
    if (Rect.Width() <= 0 || Rect.Height() <= 0) return;
    for (int32 I = 0; I < Rects.Num();)
    {
        const FIntRect& Other = Rects[I];
        if (Rect.Min.X <= Other.Max.X && Rect.Max.X >= Other.Min.X && Rect.Min.Y <= Other.Max.Y && Rect.Max.Y >= Other.Min.Y)
        {
            Rect.Union(Other);
            Rects.RemoveAtSwap(I, 1, EAllowShrinking::No);
            I = 0;
        }
        else ++I;
    }
    Rects.Add(Rect);
    // Bound upload calls without merging distant changes into one giant rectangle.
    if (Rects.Num() > 16)
    {
        int32 A = 0, B = 1, BestCost = MAX_int32;
        for (int32 I = 0; I < Rects.Num(); ++I)
            for (int32 J = I + 1; J < Rects.Num(); ++J)
            {
                FIntRect Combined = Rects[I]; Combined.Union(Rects[J]);
                const int32 Cost = Combined.Area() - Rects[I].Area() - Rects[J].Area();
                if (Cost < BestCost) { BestCost = Cost; A = I; B = J; }
            }
        FIntRect Combined = Rects[A]; Combined.Union(Rects[B]);
        Rects.RemoveAt(B, 1, EAllowShrinking::No);
        Rects.RemoveAt(A, 1, EAllowShrinking::No);
        MergeRect(Rects, Combined);
    }
}

USFFogOfWarSubsystem* USFFogOfWarSubsystem::Get(const UObject* Context)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<USFFogOfWarSubsystem>() : nullptr;
}

void USFFogOfWarSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FBox Bounds(ForceInit);
	for (TActorIterator<ANavMeshBoundsVolume> It(&InWorld); It; ++It)
	{
		Bounds += It->GetComponentsBoundingBox(true);
	}
	if (Bounds.IsValid)
	{
		SetMapBounds(FVector2D(Bounds.Min) - FVector2D(200.), FVector2D(Bounds.Max) + FVector2D(200.));
	}
	bStarted = true;
}

TStatId USFFogOfWarSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USFFogOfWarSubsystem, STATGROUP_Tickables);
}

void USFFogOfWarSubsystem::RegisterSource(UCombatTargetComponent* Source)
{
	if (!Source || Sources.Contains(Source)) return;
	Sources.Add(Source);
	OnSourceRegistered.Broadcast(Source);
}

void USFFogOfWarSubsystem::UnregisterSource(UCombatTargetComponent* Source)
{
	if (Sources.Remove(Source) == 0) return;
	PendingSourceRemovals.Add(Source);
	OnSourceUnregistered.Broadcast(Source);
}

void USFFogOfWarSubsystem::SetMapBounds(FVector2D Min, FVector2D Max)
{
	if (Max.X <= Min.X || Max.Y <= Min.Y) return;
	if (WorldMin == Min && WorldMax == Max) return;
	WorldMin = Min;
	WorldMax = Max;
	bTexturesDirty = true;
}

bool USFFogOfWarSubsystem::IsLocationVisible(int32 Team, FVector Location) const
{
	const FSFFogTeamGrid* Grid = Teams.Find(Team);
	return Grid && Grid->IsVisible(Location);
}

bool USFFogOfWarSubsystem::IsLocationExplored(int32 Team, FVector Location) const
{
	const FSFFogTeamGrid* Grid = Teams.Find(Team);
	return Grid && Grid->IsExplored(Location);
}

bool USFFogOfWarSubsystem::IsActorVisibleToLocalPlayer(const AActor* Actor) const
{
	if (!IsValid(Actor)) return false;
	const USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this);
	const int32 Team = Combat ? Combat->GetLocalPlayerTeam() : INDEX_NONE;
	// Menus/portrait scenes have no gameplay team and must remain unaffected.
	if (Team == INDEX_NONE) return true;
	const UCombatTargetComponent* Target = USFCombatSubsystem::FindTargetComponent(Actor);
	if (Target && Target->GetTeamIndex() == Team) return true;
	return IsLocationVisible(Team, Actor->GetActorLocation());
}

bool USFFogOfWarSubsystem::IsActorHiddenByFog(const AActor* Actor) const
{
	return Actor && FogNotVisibleActors.Contains(Actor);
}

void USFFogOfWarSubsystem::Tick(float DeltaTime)
{
	if (!bStarted) return;
	// World tickables run after movement/PostPhysics and before the camera update.
	// Do not wait on a timer: use this frame's source positions for this frame's view.
	UpdateVision();
	if (GetWorld()->GetNetMode() != NM_DedicatedServer) UpdatePresentation();
}

void USFFogOfWarSubsystem::UpdateVision()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SFFog_UpdateVision);
    DirtyTeamRects.Reset();
    auto MarkDirty = [this](const FSFFogSourceState& State)
    {
        const FVector2D Extent(State.Radius + FSFFogTeamGrid::CellSize * 2.f);
        SFFogOfWar::MergeRect(DirtyTeamRects.FindOrAdd(State.Team), SFFogOfWar::TextureRectForBounds(
            FBox2D(State.Origin - Extent, State.Origin + Extent), WorldMin, WorldMax, TextureSize));
    };
    for (auto It = SourceStates.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || PendingSourceRemovals.Contains(It.Key()))
        {
            MarkDirty(It.Value());
            Teams.FindChecked(It.Value().Team).RemoveVision(It.Value().Contributions);
            It.RemoveCurrent();
        }
    PendingSourceRemovals.Reset();
    Sources.RemoveAllSwap([](const auto& Source) { return !Source.IsValid(); });
    for (const auto& WeakSource : Sources)
    {
        const UCombatTargetComponent* Source = WeakSource.Get();
        const ASFBaseBuilding* Building = Cast<ASFBaseBuilding>(Source->GetOwner());
        const bool bActive = (!Building || Building->ProvidesVision()) && Source->IsAlive()
            && Source->GetTeamIndex() != INDEX_NONE && Source->GetSightRadius() > 0.f;
        FSFFogSourceState* Previous = SourceStates.Find(WeakSource);
        FSFFogSourceState Current;
        Current.Origin = IsValid(Source->GetOwner()) ? FVector2D(Source->GetOwner()->GetActorLocation()) : FVector2D::ZeroVector;
        Current.Radius = Source->GetSightRadius();
        Current.Team = Source->GetTeamIndex();
        if (bActive && Previous && Current == *Previous) continue;
        if (Previous)
        {
            MarkDirty(*Previous);
            Teams.FindChecked(Previous->Team).RemoveVision(Previous->Contributions);
            Current.Contributions = MoveTemp(Previous->Contributions);
        }
        if (bActive)
        {
            FSFFogTeamGrid& Grid = Teams.FindOrAdd(Current.Team);
            if (Grid.Dimensions.X == 0)
                Grid.EnsureBounds(FSFFogTeamGrid::ToCell(FVector(WorldMin, 0.)), FSFFogTeamGrid::ToCell(FVector(WorldMax, 0.)));
            Grid.AddVision(FVector(Current.Origin, 0.), Current.Radius, Current.Contributions);
            MarkDirty(Current);
            SourceStates.Add(WeakSource, MoveTemp(Current));
        }
        else if (Previous) SourceStates.Remove(WeakSource);
    }
}

void USFFogOfWarSubsystem::EnsureTextures()
{
	if (VisionTexture) return;
	auto CreateTexture = []()
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(TextureSize, TextureSize, PF_B8G8R8A8);
		Texture->SRGB = false;
		Texture->NeverStream = true;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->UpdateResource();
		return Texture;
	};
	VisionTexture = CreateTexture();
	MinimapTexture = CreateTexture();
	// Bilinear filtering also smooths the circular border when the UI scales the texture down.
}

void USFFogOfWarSubsystem::UpdateTextures(int32 Team)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SFFog_UpdateTextures);
	EnsureTextures();
    TArray<FIntRect> Rects;
    if (bTexturesDirty || TextureTeam != Team) Rects.Add(FIntRect(0, 0, TextureSize, TextureSize));
    else if (const TArray<FIntRect>* Dirty = DirtyTeamRects.Find(Team)) Rects = *Dirty;
    for (const FIntRect& Rect : Rects)
    {
		const int32 Width = Rect.Width();
		const int32 Height = Rect.Height();
		FColor* VisionPixels = new FColor[Width * Height];
		FColor* MinimapPixels = new FColor[Width * Height];
		const FVector2D Span = WorldMax - WorldMin;
		const FSFFogTeamGrid* Grid = Teams.Find(Team);
		for (int32 Y = Rect.Min.Y; Y < Rect.Max.Y; ++Y)
		{
			for (int32 X = Rect.Min.X; X < Rect.Max.X; ++X)
			{
				const FVector Location(WorldMin.X + (X + 0.5) / TextureSize * Span.X,
					WorldMin.Y + (Y + 0.5) / TextureSize * Span.Y, 0.);
				const float Visible = Grid ? Grid->SampleCoverage(Grid->Visible, Location) : 0.f;
				const float Explored = Grid ? Grid->SampleCoverage(Grid->Explored, Location) : 0.f;
				VisionPixels[(Y - Rect.Min.Y) * Width + X - Rect.Min.X] = FColor(FMath::RoundToInt(Visible * 255.f), FMath::RoundToInt(Explored * 255.f), 0, 255);
				// Minimap horizontal = world Y; vertical = reversed world X.
				// Visible: original image; explored: 38% brightness; unknown: solid black.
				const uint8 FogAlpha = FMath::RoundToInt((1.f - FMath::Lerp(0.38f * Explored, 1.f, Visible)) * 255.f);
				MinimapPixels[(Rect.Max.X - 1 - X) * Height + Y - Rect.Min.Y] = FColor(0, 0, 0, FogAlpha);
			}
		}
		auto Upload = [](UTexture2D* Texture, FColor* Pixels, const FIntRect& RegionRect)
		{
			auto* Region = new FUpdateTextureRegion2D(RegionRect.Min.X, RegionRect.Min.Y, 0, 0, RegionRect.Width(), RegionRect.Height());
			Texture->UpdateTextureRegions(0, 1, Region, RegionRect.Width() * sizeof(FColor), sizeof(FColor),
				reinterpret_cast<uint8*>(Pixels), [](uint8* Data, const FUpdateTextureRegion2D* UpdateRegion)
				{
					delete[] reinterpret_cast<FColor*>(Data);
					delete UpdateRegion;
				});
		};
		Upload(VisionTexture, VisionPixels, Rect);
		Upload(MinimapTexture, MinimapPixels, SFFogOfWar::MinimapRect(Rect, TextureSize));
    }
	TextureTeam = Team;
	bTexturesDirty = false;
}

void USFFogOfWarSubsystem::UpdatePresentation()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SFFog_UpdatePresentation);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const int32 Team = USFCombatSubsystem::Get(this)->GetLocalPlayerTeam();
	if (!PC || !PC->IsLocalController() || Team == INDEX_NONE) { RestorePresentation(); return; }
	if (ViewingController.Get() != PC) RestorePresentation();
	ViewingController = PC;
	UpdateTextures(Team);

	// Only the gameplay camera receives this blendable; minimap and portrait captures keep their original settings.
	ASFRTSCameraPawn* Pawn = Cast<ASFRTSCameraPawn>(PC->GetViewTarget());
	UCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
	if (ViewingCamera.Get() != Camera)
	{
		if (ViewingCamera.IsValid() && FogMaterial) ViewingCamera->RemoveBlendable(FogMaterial);
		ViewingCamera = Camera;
	}
	if (Camera)
	{
		if (!FogMaterial)
		{
			UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Core/FogOfWar/M_FogOfWar.M_FogOfWar"));
			if (Base) FogMaterial = UMaterialInstanceDynamic::Create(Base, this);
		}
		if (FogMaterial)
		{
			FogMaterial->SetTextureParameterValue(TEXT("VisionMask"), VisionTexture);
			FogMaterial->SetVectorParameterValue(TEXT("MapMin"), FLinearColor(WorldMin.X, WorldMin.Y, 0.f, 0.f));
			FogMaterial->SetVectorParameterValue(TEXT("MapSize"), FLinearColor(WorldMax.X - WorldMin.X, WorldMax.Y - WorldMin.Y, 1.f, 1.f));
			Camera->AddOrUpdateBlendable(FogMaterial);
		}
	}

	TSet<TWeakObjectPtr<AActor>> HiddenNow;
	TSet<TWeakObjectPtr<UWidgetComponent>> WidgetsNow;
	TSet<TWeakObjectPtr<const AActor>> NotVisibleNow;
	for (const auto& WeakSource : Sources)
	{
		AActor* Actor = WeakSource.IsValid() ? WeakSource->GetOwner() : nullptr;
		if (!IsValid(Actor) || IsActorVisibleToLocalPlayer(Actor)) continue;
		// 다른 시스템이 숨긴 액터도 가시성 신호에는 포함한다.
		NotVisibleNow.Add(Actor);
		// Do not undo actors hidden by another system.
		if (FogHiddenActors.Contains(Actor) || !PC->HiddenActors.Contains(Actor))
		{
			if (!FogHiddenActors.Contains(Actor)) PC->HiddenActors.AddUnique(Actor);
			HiddenNow.Add(Actor);
		}
		TInlineComponentArray<UWidgetComponent*> Widgets(Actor);
		for (UWidgetComponent* Widget : Widgets)
		{
			if (FogHiddenWidgets.Contains(Widget) || !Widget->bHiddenInGame)
			{
				if (!FogHiddenWidgets.Contains(Widget)) Widget->SetHiddenInGame(true);
				WidgetsNow.Add(Widget);
			}
		}
	}
	for (auto Actor : FogHiddenActors)
		if (!HiddenNow.Contains(Actor)) PC->HiddenActors.Remove(Actor.Get());
	for (auto Widget : FogHiddenWidgets)
		if (Widget.IsValid() && !WidgetsNow.Contains(Widget)) Widget->SetHiddenInGame(false);
	FogHiddenActors = MoveTemp(HiddenNow);
	FogHiddenWidgets = MoveTemp(WidgetsNow);
	ApplyFogVisibility(MoveTemp(NotVisibleNow));
}

void USFFogOfWarSubsystem::ApplyFogVisibility(TSet<TWeakObjectPtr<const AActor>>&& NotVisibleNow)
{
	// 듣는 쪽이 없으면 차이를 계산할 필요 없이 집합만 교체한다.
	if (!OnActorFogVisibilityChanged.IsBound())
	{
		FogNotVisibleActors = MoveTemp(NotVisibleNow);
		return;
	}
	TArray<const AActor*> NewlyHidden;
	TArray<const AActor*> NewlyVisible;
	for (const auto& Actor : NotVisibleNow)
		if (!FogNotVisibleActors.Contains(Actor)) NewlyHidden.Add(Actor.Get());
	for (const auto& Actor : FogNotVisibleActors)
		if (Actor.IsValid() && !NotVisibleNow.Contains(Actor)) NewlyVisible.Add(Actor.Get());
	// 집합을 먼저 바꾼다. 받는 쪽이 IsActorHiddenByFog로 다시 물어도 같은 답이 나와야 한다.
	FogNotVisibleActors = MoveTemp(NotVisibleNow);
	for (const AActor* Actor : NewlyHidden) OnActorFogVisibilityChanged.Broadcast(Actor, false);
	for (const AActor* Actor : NewlyVisible) OnActorFogVisibilityChanged.Broadcast(Actor, true);
}

void USFFogOfWarSubsystem::RestorePresentation()
{
	if (ViewingController.IsValid())
		for (auto Actor : FogHiddenActors) ViewingController->HiddenActors.Remove(Actor.Get());
	for (auto Widget : FogHiddenWidgets)
		if (Widget.IsValid()) Widget->SetHiddenInGame(false);
	if (ViewingCamera.IsValid() && FogMaterial) ViewingCamera->RemoveBlendable(FogMaterial);
	FogHiddenActors.Reset();
	FogHiddenWidgets.Reset();
	// 숨김을 한꺼번에 풀었으므로 가려져 있던 액터 모두에 '보임'을 알린다.
	ApplyFogVisibility({});
	ViewingController.Reset();
	ViewingCamera.Reset();
	bTexturesDirty = true;
}

void USFFogOfWarSubsystem::Deinitialize()
{
	RestorePresentation();
	Sources.Reset();
	SourceStates.Reset();
	DirtyTeamRects.Reset();
	PendingSourceRemovals.Reset();
	Teams.Reset();
	bStarted = false;
	Super::Deinitialize();
}
