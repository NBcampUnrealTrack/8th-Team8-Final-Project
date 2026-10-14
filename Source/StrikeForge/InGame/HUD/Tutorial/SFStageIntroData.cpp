// Fill out your copyright notice in the Description page of Project Settings.


#include "InGame/HUD/Tutorial/SFStageIntroData.h"

FName SFStageStory::ResolveRowName(const UDataTable* Table, FName Stage, ESFRace Race, bool bVictory)
{
    if (Stage.IsNone()) return NAME_None;
    const FString Ending = bVictory ? TEXT("Clear") : TEXT("");
    const TCHAR* Faction = Race == ESFRace::Beast ? TEXT("Beast") : Race == ESFRace::Human ? TEXT("Human") : nullptr;
    if (Table && Faction)
    {
        const FName Variant(*(Stage.ToString() + TEXT("_") + Faction + Ending));
        if (Table->FindRow<FSFStageIntroDataRow>(Variant, TEXT("ResolveStory"), false)) return Variant;
    }
    return FName(*(Stage.ToString() + Ending));
}

