#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFResourcePanelWidget.generated.h"

class ASFGameState;
class UTextBlock;
class UWidgetAnimation;

UCLASS()
class STRIKEFORGE_API USFResourcePanelWidget
	: public UUserWidget
{
	GENERATED_BODY()

	// ==================== Functions ====================

public:
	UFUNCTION(
		BlueprintCallable,
		Category = "SF|HUD|Resource"
	)
	void UpdateMineral(
		int32 NewMineralValue,
		float NewMineralRate
	);

	UFUNCTION(
		BlueprintCallable,
		Category = "SF|HUD|Resource"
	)
	void UpdateEther(
		int32 NewEtherValue,
		float NewEtherRate
	);

	UFUNCTION(
		BlueprintCallable,
		Category = "SF|HUD|Resource"
	)
	void UpdatePopulation(
		int32 CurrentPopulation,
		int32 MaximumPopulation
	);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual void NativeDestruct() override;

private:
	void BindResourceData();

	void UnbindResourceData();

	void RefreshResourceData();

	FText MakeResourceRateText(
		float ResourceRate
	) const;

	// ==================== Variables ====================

protected:
	UPROPERTY(
		BlueprintReadOnly,
		Category = "SF|HUD|Resource",
		meta = (BindWidget)
	)
	TObjectPtr<UTextBlock> MineralValueText = nullptr;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "SF|HUD|Resource",
		meta = (BindWidget)
	)
	TObjectPtr<UTextBlock> MineralRateText = nullptr;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "SF|HUD|Resource",
		meta = (BindWidget)
	)
	TObjectPtr<UTextBlock> EtherValueText = nullptr;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "SF|HUD|Resource",
		meta = (BindWidget)
	)
	TObjectPtr<UTextBlock> EtherRateText = nullptr;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "SF|HUD|Resource",
		meta = (BindWidget)
	)
	TObjectPtr<UTextBlock> PopulationValueText = nullptr;

	UPROPERTY(
		Transient,
		BlueprintReadOnly,
		Category = "SF|HUD|Resource",
		meta = (BindWidgetAnim)
	)
	TObjectPtr<UWidgetAnimation> MineralGainPop = nullptr;

	UPROPERTY(
		Transient,
		BlueprintReadOnly,
		Category = "SF|HUD|Resource",
		meta = (BindWidgetAnim)
	)
	TObjectPtr<UWidgetAnimation> EtherGainPop = nullptr;

private:
	UPROPERTY(Transient)
	TObjectPtr<ASFGameState> BoundGameState = nullptr;
	// MyTeamindex
	int32 CachedPlayerTeamIndex = 0;

	int32 PreviousMineralValue = 0;
	int32 PreviousEtherValue = 0;

	bool bHasPreviousMineralValue = false;
	bool bHasPreviousEtherValue = false;
};
