#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidgetPool.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFOverheadUISubsystem.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class USFFogOfWarSubsystem;
class USFOverheadPanelWidget;
class USFOverheadStatusComponent;
class USFBuildingSynergyBadgeWidget;
struct FSFOverheadLayout;
struct FSFOverheadStatus;
enum class ESFOverheadElement : uint8;
enum class EResourceType : uint8;

namespace SFOverheadUI
{
	struct FProjector;
}

/**
 * 월드 위치에 붙는 UI를 한 곳에서 띄운다. 건물 머리 위 판과 시너지 배지를 맡는다.
 *
 * 액터 쪽 상태는 USFOverheadStatusComponent가 들고, 여기는 화면 쪽만 다룬다.
 * 판 위젯은 띄워야 하는 동안만 풀에서 꺼내 쓴다. 꺼진 건물에는 위젯이 없다.
 *
 * 그릴 캔버스는 HUD가 넘겨주고, 위치 갱신도 HUD Tick이 부른다.
 * 월드 Tick은 카메라 갱신보다 먼저 돌아서, 거기서 투영하면 판이 카메라를 한 프레임 늦게 따라간다.
 */
UCLASS(Config = Game)
class STRIKEFORGE_API USFOverheadUISubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static USFOverheadUISubsystem* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// ── 줌 배율 ──

	/** 카메라가 줌을 바꿀 때 호출한다. 떠 있는 판에 바로 반영한다. */
	void SetUIScale(float InScale);

	FORCEINLINE float GetUIScale() const { return CurrentUIScale; }

	// ── 캔버스 (HUD가 호출한다) ──

	/** HUD가 만들어질 때 그릴 캔버스를 넘긴다. 캔버스가 없던 동안 쌓인 판과 배지를 이때 띄운다. */
	void SetOverlayCanvas(UCanvasPanel* InCanvas);

	/** HUD가 사라질 때 호출한다. 띄운 위젯을 모두 거둔다. 등록은 유지한다. */
	void ClearOverlayCanvas(UCanvasPanel* InCanvas);

	/**
	 * HUD Tick에서 매 프레임 호출한다.
	 * 투영 데이터는 프레임당 한 번 구하고, 카메라가 그대로면 앵커가 움직였거나 다시 맞춰야 하는 판만 투영한다.
	 */
	void UpdateScreenPositions();

	// ── 머리 위 판 (USFOverheadStatusComponent만 호출한다) ──

	void RegisterEntry(USFOverheadStatusComponent* InStatus);
	void UnregisterEntry(USFOverheadStatusComponent* InStatus);

	/** 켜진 요소·억제·배치가 바뀌었을 때. 판을 띄우거나 거두고, 떠 있으면 다시 맞춘다. */
	void RefreshEntry(USFOverheadStatusComponent* InStatus);

	/** 요소를 켜고 배지를 재생한다. 배지 위젯이 없는 판은 재생 자리에서 바로 끝을 알리므로 켜는 게 먼저다. */
	void PlayResourceGain(USFOverheadStatusComponent* InStatus, EResourceType InResourceType, int32 InAmount);

protected:
	// 월드서브시스템이 에디터에서 실행되는것을 방지
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		// 기본값에는 Editor도 포함된다. 게임에서만 필요하므로 제외한다.
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	/** 떠 있는 판 하나의 화면 쪽 상태. 액터 쪽 상태는 StatusComponent에 있다. */
	struct FPanelEntry
	{
		TWeakObjectPtr<USFOverheadPanelWidget> Panel;
		TWeakObjectPtr<UCanvasPanelSlot> Slot;

		FDelegateHandle StatusHandle;
		FDelegateHandle ElementFinishedHandle;

		/** 판에 마지막으로 반영한 켜진 요소. 바뀐 요소만 판에 전달한다. */
		uint8 AppliedMask = 0;

		bool bFogHidden = false;

		/** 카메라가 그대로여도 다시 투영해야 하는가. 배치·안개·생성 직후에 켠다. */
		bool bPositionDirty = true;

		FVector LastAnchor = FVector::ZeroVector;
	};

	void ShowEntry(USFOverheadStatusComponent* InStatus);
	void ApplyEntry(const USFOverheadStatusComponent& InStatus, FPanelEntry& Entry) const;
	void HideEntry(USFOverheadStatusComponent* InStatus);
	void ReleaseEntry(USFOverheadStatusComponent* InStatus, const FPanelEntry& Entry);
	void ReleaseAllEntries();
	void ApplyPanelScale(USFOverheadPanelWidget& Panel, const FSFOverheadLayout& Layout) const;

	void HandleStatusChanged(const FSFOverheadStatus& InStatus, TWeakObjectPtr<USFOverheadStatusComponent> InSource);
	void HandleElementFinished(ESFOverheadElement InElement, TWeakObjectPtr<USFOverheadStatusComponent> InSource);
	void HandleFogVisibilityChanged(const AActor* InActor, bool bVisible);

	// ── 시너지 배지 ──

	UFUNCTION()
	void HandleSynergyStateChanged();

	void RefreshSynergyBadges();
	void UpdateSynergyBadgePositions(const SFOverheadUI::FProjector& Projector, const UCanvasPanel& Canvas) const;
	void ReleaseSynergyBadges();

	// ── 설정 (DefaultGame.ini) ──

	/** 머리 위 판 위젯 클래스. */
	UPROPERTY(Config)
	TSoftClassPtr<USFOverheadPanelWidget> OverheadPanelClass;

	/** 시너지 배지 위젯 클래스. */
	UPROPERTY(Config)
	TSoftClassPtr<USFBuildingSynergyBadgeWidget> SynergyBadgeClass;

	/** 배지가 화면 가장자리와 유지할 최소 간격. */
	UPROPERTY(Config)
	float SynergyBadgeScreenMargin = 8.f;

	UPROPERTY(Transient)
	TSubclassOf<USFOverheadPanelWidget> LoadedPanelClass;

	UPROPERTY(Transient)
	TSubclassOf<USFBuildingSynergyBadgeWidget> LoadedBadgeClass;

	// ── 런타임 ──

	TWeakObjectPtr<UCanvasPanel> OverlayCanvas;

	/** 판 위젯 풀. 꺼진 판은 여기로 돌아와 다른 건물이 다시 쓴다. */
	UPROPERTY(Transient)
	FUserWidgetPool PanelPool;

	/** 시너지 배지. 클러스터 수만큼 늘고, 남는 배지는 Collapsed로 쉰다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USFBuildingSynergyBadgeWidget>> SynergyBadges;

	/** BeginPlay~EndPlay 사이의 모든 StatusComponent. 캔버스가 늦게 와도 여기서 다시 띄운다. */
	TSet<TWeakObjectPtr<USFOverheadStatusComponent>> RegisteredEntries;

	/** 지금 판이 떠 있는 StatusComponent. 매 프레임 루프는 여기만 돈다. */
	TMap<TWeakObjectPtr<USFOverheadStatusComponent>, FPanelEntry> ShownEntries;

	TWeakObjectPtr<USFFogOfWarSubsystem> BoundFog;
	FDelegateHandle FogVisibilityHandle;

	FMatrix LastViewProjection = FMatrix::Identity;
	FIntRect LastViewRect;

	float CurrentUIScale = 1.f;
};
