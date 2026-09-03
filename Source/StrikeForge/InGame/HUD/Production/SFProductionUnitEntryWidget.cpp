#include "SFProductionUnitEntryWidget.h"

#include "SFProductionUIDataRow.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

#include "Datas/Unit/UnitStatData.h"

#include "Game/Subsystem/SFDataTableManager.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Logging/SFLog.h"

void USFProductionUnitEntryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (IsValid(ProductionButton))
    {
        ProductionButton->OnClicked.RemoveDynamic(
            this,
            &USFProductionUnitEntryWidget::HandleProductionButtonClicked
        );

        ProductionButton->OnClicked.AddUniqueDynamic(
            this,
            &USFProductionUnitEntryWidget::HandleProductionButtonClicked
        );
    }
}

void USFProductionUnitEntryWidget::InitializeProductionUnit(
    FName InUnitRowName
)
{
    UnitRowName = InUnitRowName;

    if (UnitRowName.IsNone())
    {
        return;
    }

    UGameInstance* GameInstance =
        GetGameInstance();

    if (!IsValid(GameInstance))
    {
        return;
    }

    USFDataTableManager* DataTableManager =
        GameInstance->GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataTableManager))
    {
        SF_LOG(
            Error,
            "[SFProductionUnitEntryWidget] "
            "DataTableManager is invalid."
        );

        return;
    }

    const FSFProductionUIDataRow* UIData =
        DataTableManager->GetDataRow<FSFProductionUIDataRow>(
            EDataTableType::ProductionUI,
            UnitRowName,
            TEXT("InitializeProductionUnit")
        );

    if (!UIData)
    {
        SF_LOG(
            Warning,
            "[SFProductionUnitEntryWidget] "
            "Production UI Row was not found: %s",
            *UnitRowName.ToString()
        );

        return;
    }

    const FUnitStatData* UnitStats =
        DataTableManager->GetDataRow<FUnitStatData>(
            EDataTableType::Unit,
            UnitRowName,
            TEXT("InitializeProductionUnit - Unit")
        );

    if (!UnitStats)
    {
        SF_LOG(
            Warning,
            "[SFProductionUnitEntryWidget] "
            "Unit Row was not found: %s",
            *UnitRowName.ToString()
        );
    }

    if (IsValid(UnitNameText))
    {
        UnitNameText->SetText(
            UIData->DisplayName
        );
    }

    if (IsValid(UnitIconImage))
    {
        UTexture2D* IconTexture =
            UIData->Icon.LoadSynchronous();

        if (IsValid(IconTexture))
        {
            UnitIconImage->SetBrushFromTexture(
                IconTexture
            );
        }
    }

    if (IsValid(UnitNeedGold))
    {
        const int32 MineralCost =
            UnitStats
            ? UnitStats->UnitProductionCost.Mineral
            : 0;

        UnitNeedGold->SetText(
            FText::AsNumber(MineralCost)
        );
    }

    if (IsValid(UnitNeedEther))
    {
        const int32 EtherCost =
            UnitStats
            ? UnitStats->UnitProductionCost.Ether
            : 0;

        UnitNeedEther->SetText(
            FText::AsNumber(EtherCost)
        );
    }
    SF_LOG(
        Log,
        "[SFProductionUnitEntryWidget] "
        "Initialized | Row: %s | Name: %s",
        *UnitRowName.ToString(),
        *UIData->DisplayName.ToString()
    );
}

void USFProductionUnitEntryWidget::HandleProductionButtonClicked()
{
    if (UnitRowName.IsNone())
    {
        return;
    }

    SF_LOG(
        Log,
        "[SFProductionUnitEntryWidget] "
        "Clicked | Unit: %s",
        *UnitRowName.ToString()
    );

    OnProductionUnitClicked.Broadcast(
        UnitRowName
    );
}