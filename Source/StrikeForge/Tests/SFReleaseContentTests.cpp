#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"
#include "Datas/Unit/UnitStatData.h"
#include "InGame/HUD/Production/SFProductionUIDataRow.h"
#include "InGame/SFSelectINfo/SFSelectionUIDataRow.h"
#include "Units/Unit/UnitBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFReleaseContentTest, "StrikeForge.Release.UnitPortraitsAndVoices",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFReleaseContentTest::RunTest(const FString& Parameters)
{
    auto* Units = LoadObject<UDataTable>(nullptr,TEXT("/Game/Core/DataTable/DT_Units.DT_Units"));
    auto* UI = LoadObject<UDataTable>(nullptr,TEXT("/Game/Core/DataTable/DT_SFProductionUI.DT_SFProductionUI"));
    auto* Selected = LoadObject<UDataTable>(nullptr,TEXT("/Game/Game/IN_UI/DT/DT_SFSelectionUI.DT_SFSelectionUI"));
    if (!TestNotNull(TEXT("Units"),Units) || !TestNotNull(TEXT("Production"),UI) || !TestNotNull(TEXT("Selection"),Selected)) return false;
    for (FName Name : Units->GetRowNames())
    {
        auto* Row = Units->FindRow<FUnitStatData>(Name,TEXT("Release"));
        auto* Production = UI->FindRow<FSFProductionUIDataRow>(Name,TEXT("Release"));
        auto* Selection = Selected->FindRow<FSFSelectionUIDataRow>(Name,TEXT("Release"));
        const FString Prefix = Name.ToString()+TEXT(": ");
        // Unvoiced units intentionally stay silent; validate only authored sounds.
        if (!Row->SelectionSound.IsNull()) TestNotNull(Prefix+TEXT("selection voice"), Row->SelectionSound.LoadSynchronous());
        if (!Row->MoveCommandSound.IsNull()) TestNotNull(Prefix+TEXT("move voice"), Row->MoveCommandSound.LoadSynchronous());
        if (TestNotNull(Prefix+TEXT("production row"),Production)) TestNotNull(Prefix+TEXT("production icon"),Production->Icon.LoadSynchronous());
        if (TestNotNull(Prefix+TEXT("selection row"),Selection)) TestNotNull(Prefix+TEXT("portrait"),Selection->PortraitImage.LoadSynchronous());
        UClass* Class = Row->UnitClass.LoadSynchronous();
        if (TestNotNull(Prefix+TEXT("class"),Class)) TestEqual(Prefix+TEXT("placed unit identity"),Class->GetDefaultObject<AUnitBase>()->GetUnitRowName(),Name);
    }
    return true;
}
#endif
