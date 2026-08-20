#include "StartWidget.h"
#include "SettingWidget.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "StrikeForge/Game/GameInstance/SFGameInstance.h"

void UStartWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 클릭 이벤트 바인딩
	if (IsValid(Start_Btn))
	{
		Start_Btn->OnClicked.AddDynamic(this, &UStartWidget::OnStartButtonClicked);
	}
	if (IsValid(Setting_Btn))
	{
		Setting_Btn->OnClicked.AddDynamic(this, &UStartWidget::OnSettingButtonClicked);
	}
	if (IsValid(Exit_Btn))
	{
		Exit_Btn->OnClicked.AddDynamic(this, &UStartWidget::OnExitButtonClicked);
	}
}

void UStartWidget::OnStartButtonClicked()
{
	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;
	if (!IsValid(GameInstance))
	{
		return;
	}

	GameInstance->MoveToLobby();
}

void UStartWidget::OnSettingButtonClicked()
{
	if (IsValid(SettingWidget))
	{
		SettingWidget->OpenSetting();
	}
}

void UStartWidget::OnExitButtonClicked()
{
	APlayerController* SpecificPlayer = GetOwningPlayer();
	if (IsValid(SpecificPlayer))
	{
		UKismetSystemLibrary::QuitGame(this, SpecificPlayer, EQuitPreference::Quit, false);
	}
}