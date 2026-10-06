#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingWidget.h"
#include "Components/Button.h"
#include "LobbyWidget.generated.h"

class UButton;
class UWidget;
class UTexture2D;
class USFMenuAtmosphereWidget;
class USFLobbyStagePresentation;
class USFStoryWidget;
UCLASS()
class STRIKEFORGE_API ULobbyWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void OnBackButtonClicked();
	UFUNCTION()
	void OnSettingButtonClicked();
	UFUNCTION()
	void OnStoryButtonClicked();
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
	UFUNCTION()
	void OnStage7Clicked();

	void OpenStageLevel(int32 StageIndex);
	// 26.08.11 추가
	// 로비의 모든 스테이지 버튼 상태 갱신
	void RefreshStageButtons();
	// 스테이지 하나의 진행 상태와 이미지 갱신
	void RefreshStageButton(UButton* StageButton, FName StageRowName);
	// Texture를 버튼 Style에 적용
	void ApplyStageButtonTexture(UButton* StageButton, UTexture2D* StageTexture);

protected:
	UPROPERTY(Transient)
	TObjectPtr<USFStoryWidget> StoryWidget;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Story_Btn;
	FOnButtonClickedEvent OriginalStoryClick;
	bool bStoryBound = false;

	UPROPERTY(Transient)
	TObjectPtr<USFLobbyStagePresentation> StagePresentation;
	UPROPERTY(Transient)
	TObjectPtr<USFMenuAtmosphereWidget> AnimatedBackdrop;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> OriginalBackdrop;
	ESlateVisibility OriginalBackdropVisibility = ESlateVisibility::Visible;

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
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Stage6_1;
	UPROPERTY(meta = (BindWidget))
	UButton* Setting_Btn;
	UPROPERTY(meta = (BindWidget))
	UButton* Back_Btn;

	UPROPERTY(meta = (BindWidget))
	USettingWidget* SettingWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Stage")
	TArray<FName> StageRowNames = { TEXT("Stage01"), TEXT("Stage02"), TEXT("Stage03"), TEXT("Stage04"), TEXT("StageHero"), TEXT("Stage06"), TEXT("Stage07") };
};
