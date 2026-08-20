#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LobbyWidget.generated.h"
class UButton;
class UWidget;
class UTexture2D;
class USettingWidget;
UCLASS()
class STRIKEFORGE_API ULobbyWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnBackButtonClicked();
	UFUNCTION()
	void OnSettingButtonClicked();
	UFUNCTION()
	void OnStage1Clicked();
	UFUNCTION()
	void OnStage2Clicked();
	UFUNCTION()
	void OnStage3Clicked();
	UFUNCTION()
	void OnStage4Clicked();
	UFUNCTION()
	void OnStage5Clicked();
	UFUNCTION()
	void OnStage6Clicked();

	void OpenStageLevel(int32 StageIndex);
	// 26.08.11 추가
	// 로비의 모든 스테이지 버튼 상태 갱신
	void RefreshStageButtons();
	// 스테이지 하나의 진행 상태와 이미지 갱신
	void RefreshStageButton(UButton* StageButton, FName StageRowName);
	// Texture를 버튼 Style에 적용
	void ApplyStageButtonTexture(UButton* StageButton, UTexture2D* StageTexture);

protected:
	UPROPERTY(meta = (BindWidget))
	UButton* Stage1;
	UPROPERTY(meta = (BindWidget))
	UButton* Stage2;
	UPROPERTY(meta = (BindWidget))
	UButton* Stage3;
	UPROPERTY(meta = (BindWidget))
	UButton* Stage4;
	UPROPERTY(meta = (BindWidget))
	UButton* Stage5;
	UPROPERTY(meta = (BindWidget))
	UButton* Stage6;
	UPROPERTY(meta = (BindWidget))
	UButton* Setting_Btn;
	UPROPERTY(meta = (BindWidget))
	UButton* Back_Btn;

	// WBP_Lobby 캔버스에 직접 배치된 WBP_Setting 인스턴스 (Visibility 토글로 열고 닫음)
	UPROPERTY(meta = (BindWidget))
	USettingWidget* SettingWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Stage")
	TArray<FName> StageRowNames = { TEXT("Stage1"), TEXT("Stage2"), TEXT("Stage3"), TEXT("Stage4"), TEXT("Stage5"), TEXT("Stage6") };
};