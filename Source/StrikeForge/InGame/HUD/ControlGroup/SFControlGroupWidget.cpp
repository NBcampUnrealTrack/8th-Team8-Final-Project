#include "SFControlGroupWidget.h"

#include "SFControlNumWidget.h"

void USFControlGroupWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ControlNumWidgets.Reset();

    ControlNumWidgets.Add(WBP_ControlNum01);
    ControlNumWidgets.Add(WBP_ControlNum02);
    ControlNumWidgets.Add(WBP_ControlNum03);
    ControlNumWidgets.Add(WBP_ControlNum04);
    ControlNumWidgets.Add(WBP_ControlNum05);
    ControlNumWidgets.Add(WBP_ControlNum06);

    for (int32 Index = 0; Index < ControlNumWidgets.Num(); ++Index)
    {
        USFControlNumWidget* ControlNum =
            ControlNumWidgets[Index];

        if (!IsValid(ControlNum))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "[SFControlGroupWidget] "
                    "ControlNum %d is invalid."
                ),
                Index + 1
            );

            continue;
        }

        ControlNum->InitializeControlNum(Index);

        ControlNum->OnControlNumClicked
            .AddUniqueDynamic(
                this,
                &USFControlGroupWidget::HandleControlNumClicked
            );
    }
}

void USFControlGroupWidget::NativeDestruct()
{
    for (USFControlNumWidget* ControlNum : ControlNumWidgets)
    {
        if (!IsValid(ControlNum))
        {
            continue;
        }

        ControlNum->OnControlNumClicked
            .RemoveDynamic(
                this,
                &USFControlGroupWidget::HandleControlNumClicked
            );
    }

    Super::NativeDestruct();
}

void USFControlGroupWidget::UpdateControlGroupSlot(
    int32 GroupIndex,
    UTexture2D* RepresentativeImage,
    int32 ActorCount
)
{
    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[SFControlGroupWidget] Update called | "
            "Index: %d | Count: %d"
        ),
        GroupIndex,
        ActorCount
    );

    if (!ControlNumWidgets.IsValidIndex(GroupIndex))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "[SFControlGroupWidget] Invalid group index: %d"
            ),
            GroupIndex
        );

        return;
    }

    USFControlNumWidget* ControlNum =
        ControlNumWidgets[GroupIndex];

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[SFControlGroupWidget] Slot: %s"
        ),
        *GetNameSafe(ControlNum)
    );

    if (!IsValid(ControlNum))
    {
        return;
    }

    ControlNum->SetControlGroupData(
        RepresentativeImage,
        ActorCount
    );
}
void USFControlGroupWidget::ClearControlGroupSlot(
    int32 GroupIndex
)
{
    if (!ControlNumWidgets.IsValidIndex(GroupIndex))
    {
        return;
    }

    if (IsValid(ControlNumWidgets[GroupIndex]))
    {
        ControlNumWidgets[GroupIndex]
            ->ClearControlGroupData();
    }
}

void USFControlGroupWidget::ClearAllControlGroupSlots()
{
    for (USFControlNumWidget* ControlNum : ControlNumWidgets)
    {
        if (IsValid(ControlNum))
        {
            ControlNum->ClearControlGroupData();
        }
    }
}

bool USFControlGroupWidget::IsControlGroupAssigned(
    int32 GroupIndex
) const
{
    if (!ControlNumWidgets.IsValidIndex(GroupIndex))
    {
        return false;
    }

    return IsValid(ControlNumWidgets[GroupIndex]) &&
        ControlNumWidgets[GroupIndex]
        ->HasControlGroupData();
}

void USFControlGroupWidget::HandleControlNumClicked(
    int32 GroupIndex
)
{
    OnControlGroupClicked.Broadcast(GroupIndex);
}

void USFControlGroupWidget::RemoveControlGroupSlot(
    int32 GroupIndex
)
{
    if (!ControlNumWidgets.IsValidIndex(GroupIndex))
    {
        return;
    }

    USFControlNumWidget* ControlNumSlot =
        ControlNumWidgets[GroupIndex].Get();

    if (!IsValid(ControlNumSlot))
    {
        return;
    }

    ControlNumSlot->PlaySlideOutAndHide();
}

void USFControlGroupWidget::UpdateControlGroupSlotCount(
    int32 GroupIndex,
    int32 ActorCount
)
{
    if (!ControlNumWidgets.IsValidIndex(GroupIndex))
    {
        return;
    }

    USFControlNumWidget* ControlNumSlot =
        ControlNumWidgets[GroupIndex].Get();

    if (!IsValid(ControlNumSlot))
    {
        return;
    }

    ControlNumSlot->SetActorCount(ActorCount);
}