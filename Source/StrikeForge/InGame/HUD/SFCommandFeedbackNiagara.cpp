#include "SFCommandFeedbackNiagara.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraRendererProperties.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

FVector SFCommandFeedback::GroundLocation(AActor* Target)
{
    FVector Origin, Extent;
    Target->GetActorBounds(true, Origin, Extent);
    const FVector Center = Target->GetActorLocation();
    FVector Location(Center.X, Center.Y, Origin.Z - Extent.Z + 8.f);
    // Niagara geometry does not project onto terrain like the old decal did.
    // Building collision boxes can extend below the floor, so trace the actual ground.
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CommandFeedbackGround), true, Target);
    FHitResult Hit;
    const FVector Start(Center.X, Center.Y, FMath::Max(Center.Z, Origin.Z + Extent.Z) + 100.f);
    const FVector End(Center.X, Center.Y, FMath::Min(Center.Z, Origin.Z - Extent.Z) - 2000.f);
    UWorld* World = Target->GetWorld();
    if (World && (World->LineTraceSingleByChannel(Hit, Start, End, ECC_GameTraceChannel1, Params)
        || World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)))
        Location.Z = Hit.ImpactPoint.Z + 8.f;
    return Location;
}

UNiagaraComponent* SFCommandFeedback::Spawn(UObject* WorldContext, UNiagaraSystem* System,
    const FVector& Location, const FVector& Scale, const FLinearColor& Color,
    bool bAutoDestroy, TArray<TObjectPtr<UMaterialInstanceDynamic>>* OutMaterials)
{
    if (!IsValid(System)) return nullptr;
    UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        WorldContext, System, Location, FRotator::ZeroRotator, Scale,
        bAutoDestroy, false, ENCPoolMethod::None, false);
    if (!Component) return nullptr;
    TArray<UMaterialInterface*> Materials;
    // The component has not activated yet, so it has no system instance to query.
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
        if (const auto* Data = Handle.GetInstance().GetEmitterData())
            for (const UNiagaraRendererProperties* Renderer : Data->GetRenderers())
                Renderer->GetUsedMaterials(nullptr, Materials);
    TSet<UMaterialInterface*> Configured;
    for (UMaterialInterface* Material : Materials)
    {
        if (!Material || Configured.Contains(Material)) continue;
        Configured.Add(Material);
        UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, Component);
        Dynamic->SetVectorParameterValue(TEXT("FeedbackColor"), FLinearColor(Color.R, Color.G, Color.B, 1.f));
        Dynamic->SetScalarParameterValue(TEXT("FeedbackFlash"), 0.f);
        Component->SetVariableMaterial(FName(TEXT("User.") + Material->GetName()), Dynamic);
        if (OutMaterials) OutMaterials->Add(Dynamic);
    }
    Component->SetCastShadow(false);
    Component->Activate(true);
    return Component;
}

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace SFCommandFeedbackAuthoring
{
const FString Folder = TEXT("/Game/Game/IN_UI/WorldFeedback/Niagara/");
template<class T> T* CopyAsset(T* Source, const FString& Name)
{
    check(Source);
    UPackage* Package = CreatePackage(*(Folder + Name));
    Package->FullyLoad();
    T* Asset = DuplicateObject<T>(Source, Package, *Name);
    Asset->SetFlags(RF_Public | RF_Standalone);
    return Asset;
}
void Save(UObject* Asset)
{
    Asset->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    const FString Filename = FPackageName::LongPackageNameToFilename(
        Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    check(UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args));
}
UMaterial* MakeBase(UMaterial* Source)
{
    UMaterial* Base = CopyAsset(Source, TEXT("M_SFCommand_") + Source->GetName());
    TArray<TObjectPtr<UMaterialExpression>> Originals(Base->GetExpressions());
    auto* Tint = NewObject<UMaterialExpressionVectorParameter>(Base);
    Tint->ParameterName = TEXT("FeedbackColor");
    Tint->DefaultValue = FLinearColor::Red;
    Base->GetExpressionCollection().AddExpression(Tint);
    auto* Flash = NewObject<UMaterialExpressionScalarParameter>(Base);
    Flash->ParameterName = TEXT("FeedbackFlash"); Flash->DefaultValue = 0.f;
    Base->GetExpressionCollection().AddExpression(Flash);
    for (UMaterialExpression* Expression : Originals)
    {
        if (Expression->GetClass()->GetFName() != TEXT("MaterialExpressionParticleColor")) continue;
        UMaterialExpression* Particle = Expression;
        auto* Recolor = NewObject<UMaterialExpressionCustom>(Base);
        Recolor->OutputType = CMOT_Float3;
        Recolor->Code = TEXT("return Tint * max(P.r,max(P.g,P.b)) * (1.0 + Flash * 0.5);");
        Recolor->Inputs.Empty();
        FCustomInput P; P.InputName=TEXT("P"); P.Input.Connect(0,Particle); Recolor->Inputs.Add(P);
        FCustomInput T; T.InputName=TEXT("Tint"); T.Input.Connect(0,Tint); Recolor->Inputs.Add(T);
        FCustomInput F; F.InputName=TEXT("Flash"); F.Input.Connect(0,Flash); Recolor->Inputs.Add(F);
        for (UMaterialExpression* Other : Originals)
            for (FExpressionInputIterator Input(Other); Input; ++Input)
                if (Input->Expression == Particle && Input->OutputIndex == 0)
                    Input->Connect(0,Recolor);
        Base->GetExpressionCollection().AddExpression(Recolor);
    }
    Base->PostEditChange(); Save(Base);
    return Base;
}
void Author()
{
    TMap<UMaterial*, UMaterial*> Bases;
    TMap<UMaterialInterface*, UMaterialInstanceConstant*> Converted;
    auto Convert = [&](UMaterialInterface* Original)
    {
        check(Original);
        if (auto* Existing=Converted.Find(Original)) return *Existing;
        auto* Source = CastChecked<UMaterialInstanceConstant>(Original);
        UMaterial* SourceBase = Source->GetMaterial();
        UMaterial*& Base = Bases.FindOrAdd(SourceBase);
        if (!Base) Base = MakeBase(SourceBase);
        auto* Material=CopyAsset(Source,TEXT("MI_SFCommand_")+Source->GetName());
        Material->SetParentEditorOnly(Base); Material->PostEditChange(); Save(Material);
        Converted.Add(Original,Material); return Material;
    };
    for (bool bHover : {false,true})
    {
        const TCHAR* Path = bHover
            ? TEXT("/Game/MarkersPointersV1/Particles/NiagaraSystems/Markers/NS_SquareMarker01_Blue.NS_SquareMarker01_Blue")
            : TEXT("/Game/MarkersPointersV1/Particles/NiagaraSystems/ClickMovement/NS_ClickMovement_02.NS_ClickMovement_02");
        UNiagaraSystem* System=CopyAsset(LoadObject<UNiagaraSystem>(nullptr,Path),
            bHover ? TEXT("NS_SFHoverRound") : TEXT("NS_SFClickMovement"));
        auto Bind = [&](FNiagaraUserParameterBinding& Binding,UMaterialInterface* Material)
        {
            Binding.Parameter = FNiagaraVariable(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()),
                FName(TEXT("User.")+Material->GetName()));
            System->GetExposedParameters().AddParameter(Binding.Parameter);
        };
        for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
        {
            auto* Data=Handle.GetInstance().GetEmitterData(); if (!Data) continue;
            Data->bLocalSpace=true;
            for (UNiagaraRendererProperties* Renderer : Data->GetRenderers())
            {
                if (auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(Renderer))
                {
                    UMaterialInterface* Original=Sprite->Material;
                    if (bHover) Original=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MarkersPointersV1/Materials/Mi_Circle_01.Mi_Circle_01"));
                    Sprite->Material=Convert(Original);
                    Bind(Sprite->MaterialUserParamBinding,Sprite->Material);
                }
                if (auto* Mesh=Cast<UNiagaraMeshRendererProperties>(Renderer))
                {
                    if (bHover)
                        for (auto& Entry : Mesh->Meshes)
                            Entry.Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/MarkersPointersV1/Models/SM_Cylinder_02.SM_Cylinder_02"));
                    Mesh->bOverrideMaterials=true;
                    for (auto& Override : Mesh->OverrideMaterials)
                    {
                        Override.ExplicitMat=Convert(Override.ExplicitMat);
                        Bind(Override.UserParamBinding,Override.ExplicitMat);
                    }
                }
            }
        }
        System->PostEditChange(); System->RequestCompile(true); System->WaitForCompilationComplete();
        Save(System);
        UE_LOG(LogTemp,Display,TEXT("SF_COMMAND_FX_SAVED %s"),*System->GetPathName());
    }
}
static FAutoConsoleCommand Command(TEXT("SF.Feedback.AuthorNiagara"),
    TEXT("Regenerate command feedback variants from MarkersPointersV1 assets."),
    FConsoleCommandDelegate::CreateStatic(&Author));
}
#endif

