#include "SFInGameHUDWidget.h"

#include "SFInGamePlayerController.h"
#include "SFSelectInfo/SFSelectable.h"

#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "SFSelectInfo/SFSelectionPanelWidget.h"


void USFInGameHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(
            GetOwningPlayer()
        );

    if (!IsValid(PlayerController))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "SFInGameHUDWidget: "
                "SFInGamePlayerController를 찾지 못했습니다."
            )
        );

        return;
    }

    PlayerController->OnSelectInput.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectInput
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "SFInGameHUDWidget: "
            "OnSelectInput 바인딩 완료"
        )
    );
}

void USFInGameHUDWidget::NativeDestruct()
{
    ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(
            GetOwningPlayer()
        );

    if (IsValid(PlayerController))
    {
        PlayerController->OnSelectInput.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectInput
        );
    }

    Super::NativeDestruct();
}

void USFInGameHUDWidget::HandleSelectInput()
{
    PerformSelectionTrace();
}

void USFInGameHUDWidget::PerformSelectionTrace()
{
    APlayerController* PlayerController =
        GetOwningPlayer();

    if (!IsValid(PlayerController))
    {
        ClearSelection();
        return;
    }

    FHitResult HitResult;

    const ETraceTypeQuery SelectionTraceType =
        UEngineTypes::ConvertToTraceType(
            ECC_Visibility
        );

    const bool bHit =
        PlayerController->GetHitResultUnderCursorByChannel(
            SelectionTraceType,
            false,
            HitResult
        );

    if (!bHit)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("SF Selection Trace: No Hit")
        );

        ClearSelection();
        return;
    }

    AActor* HitActor = HitResult.GetActor();

    if (!IsValid(HitActor))
    {
        ClearSelection();
        return;
    }

    const bool bIsSelectable =
        HitActor->GetClass()->ImplementsInterface(
            USFSelectable::StaticClass()
        );

    if (!bIsSelectable)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "SF Selection Trace: "
                "%s is not selectable."
            ),
            *HitActor->GetName()
        );

        ClearSelection();
        return;
    }

    const FSFSelectableInfo SelectableInfo =
        ISFSelectable::Execute_GetSelectableInfo(
            HitActor
        );

    SelectedActor = HitActor;
    SelectedInfo = SelectableInfo;

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ShowSelection(
            SelectedInfo
        );
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "SF Selectable Selected | "
            "Actor: %s | "
            "Name: %s | "
            "Health: %.1f / %.1f | "
            "RowName: %s"
        ),
        *HitActor->GetName(),
        *SelectableInfo.DisplayName.ToString(),
        SelectableInfo.CurrentHealth,
        SelectableInfo.MaximumHealth,
        *SelectableInfo.DataRowName.ToString()
    );

    DrawDebugSphere(
        GetWorld(),
        HitResult.ImpactPoint,
        30.0f,
        12,
        FColor::Green,
        false,
        1.5f
    );
}
void USFInGameHUDWidget::ClearSelection()
{
    SelectedActor = nullptr;
    SelectedInfo = FSFSelectableInfo();

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ClearSelection();
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("SF Selection Cleared")
    );
}