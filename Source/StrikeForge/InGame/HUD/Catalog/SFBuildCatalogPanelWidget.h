#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/GameState/SFGameState.h"
#include "SFBuildCatalogPanelWidget.generated.h"

class UHorizontalBox;
class USFBuildCatalogEntryWidget;
class ASFBaseBuilding;
class ASFGameState;
class USFTeamState;

UCLASS()
class STRIKEFORGE_API USFBuildCatalogPanelWidget
	: public UUserWidget
{
	GENERATED_BODY()

	//==================== = Functions ====================

public:
	bool RequestBuildingBySlot(int32 SlotIndex);

	/** 지정 건물 카드의 실제 화면 위치를 튜토리얼 테두리에 제공한다. */
	UWidget* FindTutorialEntryWidget(FName BuildingRowName) const;

private:
	void BindResourceData();

	void UnbindResourceData();

	void RefreshEntryAffordability();

	void BindBuildingCompletionEvents();

	void UnbindBuildingCompletionEvents();

	void BindBuildingCompletionEvent(
		ASFBaseBuilding* Building
	);

	void HandleBuildingSpawned(
		AActor* SpawnedActor
	);

	UFUNCTION()
	void HandleBuildingTierActivated(
		int32 ActivatedTier
	);

	UFUNCTION()
	void HandleMineralChanged(
		int32 NewMineral,
		float NewIncomeRate
	);

	UFUNCTION()
	void HandleEtherChanged(
		int32 NewEther,
		float NewIncomeRate
	);
	
protected:
	/** Widget 생성 시 Stage 상태 이벤트를 구독한다. UUserWidget 수명 주기에서 호출되며 HandleStageStateChanged로 이어진다. */
	virtual void NativeConstruct() override;
	/** Widget 제거 시 Stage 상태 이벤트를 해제한다. UUserWidget 수명 주기에서 호출되며 부모 NativeDestruct로 이어진다. */
	virtual void NativeDestruct() override;
private:
	/** Stage가 Playing으로 바뀌면 건설 목록을 갱신한다. GameState 델리게이트에서 호출되며 RefreshBuildingEntries로 이어진다. */
	UFUNCTION()
	void HandleStageStateChanged(
		ESFStageState NewState);
	/** 이미 로드된 Definition으로 건설 UI Entry를 만든다. NativeConstruct와 Stage 상태 변경에서 호출된다. */
	void RefreshBuildingEntries();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> BuildingEntryHorizontalBox;

	UPROPERTY(EditDefaultsOnly, Category = "SF|Build Catalog")
	TSubclassOf<USFBuildCatalogEntryWidget> BuildingEntryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ASFGameState> BoundGameState = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USFBuildCatalogEntryWidget>>
		CurrentEntryWidgets;

	int32 CachedTeamIndex = 0;

	FDelegateHandle ActorSpawnedHandle;

	TArray<TWeakObjectPtr<ASFBaseBuilding>>
		BoundBuildings;
};
