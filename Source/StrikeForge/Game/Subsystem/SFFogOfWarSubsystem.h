#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFFogOfWarSubsystem.generated.h"

class UCombatTargetComponent;
class UTexture2D;
class UMaterialInstanceDynamic;
class UCameraComponent;
class UWidgetComponent;
class APlayerController;

namespace SFFogOfWar
{
	FIntRect TextureRectForBounds(const FBox2D& Bounds, FVector2D MapMin, FVector2D MapMax, int32 Size);
	FIntRect MinimapRect(const FIntRect& WorldRect, int32 Size);
	void MergeRect(TArray<FIntRect>& Rects, FIntRect Rect);
}

struct FSFFogSourceState
{
	FVector2D Origin = FVector2D::ZeroVector;
	float Radius = 0.f;
	int32 Team = INDEX_NONE;
	TArray<int32> Contributions;
	bool operator==(const FSFFogSourceState& Other) const
	{
		return Origin == Other.Origin && Radius == Other.Radius && Team == Other.Team;
	}
};

/** Dense world-space cells; contribution handles stay valid when the grid grows. */
struct FSFFogTeamGrid
{
	static constexpr float CellSize = 50.f;
	static constexpr float EdgeSoftness = 75.f;
	FIntPoint MinCell = FIntPoint::ZeroValue;
	FIntPoint Dimensions = FIntPoint::ZeroValue;
	TArray<uint32> VisibleCount;
	TArray<uint8> Visible;
	TArray<uint8> Explored;
	static FIntPoint ToCell(const FVector& Location);
	int32 Index(FIntPoint Cell) const;
	void EnsureBounds(FIntPoint Min, FIntPoint Max);
	void AddVision(const FVector& Origin, float Radius, TArray<int32>& OutHandles);
	void RemoveVision(TArray<int32>& Handles);
	float SampleCoverage(const TArray<uint8>& Cells, const FVector& Location) const;
	bool IsVisible(const FVector& Location) const;
	bool IsExplored(const FVector& Location) const;
	SIZE_T GetAllocatedSize() const;
private:
	struct FContribution
	{
		int32 Cell = INDEX_NONE;
		int32 Next = INDEX_NONE;
		int32 Previous = INDEX_NONE;
		uint8 Coverage = 0;
	};
	TArray<int32> Heads;
	TArray<FContribution> Contributions;
	TArray<int32> FreeHandles;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FSFFogSourceEvent, UCombatTargetComponent*);

/** Shared team vision, persistent exploration, and local presentation. Never changes collision or AI ticking. */
UCLASS()
class STRIKEFORGE_API USFFogOfWarSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;
	static USFFogOfWarSubsystem* Get(const UObject* Context);

	void RegisterSource(UCombatTargetComponent* Source);
	void UnregisterSource(UCombatTargetComponent* Source);
	void SetMapBounds(FVector2D Min, FVector2D Max);
	const TArray<TWeakObjectPtr<UCombatTargetComponent>>& GetSources() const { return Sources; }
	FSFFogSourceEvent OnSourceRegistered;
	FSFFogSourceEvent OnSourceUnregistered;
	FSFFogSourceEvent OnSourceChanged;

	UFUNCTION(BlueprintPure, Category = "SF|Fog Of War")
	bool IsLocationVisible(int32 Team, FVector Location) const;
	UFUNCTION(BlueprintPure, Category = "SF|Fog Of War")
	bool IsLocationExplored(int32 Team, FVector Location) const;
	UFUNCTION(BlueprintPure, Category = "SF|Fog Of War")
	bool IsActorVisibleToLocalPlayer(const AActor* Actor) const;
	UTexture2D* GetMinimapFogTexture() const { return MinimapTexture; }

private:
	void UpdateVision();
	void UpdatePresentation();
	void RestorePresentation();
	void UpdateTextures(int32 Team);
	void EnsureTextures();
	TArray<TWeakObjectPtr<UCombatTargetComponent>> Sources;
	TSet<TWeakObjectPtr<UCombatTargetComponent>> PendingSourceRemovals;
	TMap<TWeakObjectPtr<UCombatTargetComponent>, FSFFogSourceState> SourceStates;
	TMap<int32, TArray<FIntRect>> DirtyTeamRects;
	TMap<int32, FSFFogTeamGrid> Teams;
	TSet<TWeakObjectPtr<AActor>> FogHiddenActors;
	TSet<TWeakObjectPtr<UWidgetComponent>> FogHiddenWidgets;
	TWeakObjectPtr<APlayerController> ViewingController;
	TWeakObjectPtr<UCameraComponent> ViewingCamera;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> VisionTexture;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> MinimapTexture;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FogMaterial;
	FVector2D WorldMin = FVector2D(-5000., -5000.);
	FVector2D WorldMax = FVector2D(5000., 5000.);
	int32 TextureTeam = INDEX_NONE;
	bool bTexturesDirty = true;
	bool bStarted = false;
	static constexpr int32 TextureSize = 512;
};
