#include "StartWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UStartWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Start_Btn)
	{
		Start_Btn->OnClicked.AddDynamic(this, &UStartWidget::OnStartButtonClicked);
	}

	if (Exit_Btn)
	{
		Exit_Btn->OnClicked.AddDynamic(this, &UStartWidget::OnExitButtonClicked);
	}
}

void UStartWidget::OnStartButtonClicked()
{
	UGameplayStatics::OpenLevel(this, FName("Lobby"));
}

void UStartWidget::OnExitButtonClicked()
{
	APlayerController* SpecificPlayer = GetOwningPlayer();
	UKismetSystemLibrary::QuitGame(this, SpecificPlayer, EQuitPreference::Quit, false);
}