#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Slate/WidgetRenderer.h"
#include "UI/LobbyWidget.h"
#include "UI/StartWidget.h"
#include "UI/SFMenuAtmosphereWidget.h"
#include "UI/SFLobbyStagePresentation.h"
#include "UI/SFStartSceneAmbience.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "TimerManager.h"

// Standalone-only QA: capture the actual scene and UI together at the requested window size.
static FAutoConsoleCommandWithWorldAndArgs SFMenuSceneShot(
	TEXT("SF.Menu.SceneShot"), TEXT("SF.Menu.SceneShot <name> [quit]: capture the game viewport; optional third argument is delay in seconds."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
	{
		if (!World || Args.IsEmpty()) return;
		const float Delay = Args.Num()>2 ? FMath::Max(1.f, FCString::Atof(*Args[2])) : 8.f;
		const FString Path=FPaths::ProjectSavedDir()/TEXT("LobbyPolish")/(FPaths::GetCleanFilename(Args[0])+TEXT(".png"));
		FTimerHandle ShotTimer;
		World->GetTimerManager().SetTimer(ShotTimer,FTimerDelegate::CreateLambda([Path]()
		{ FScreenshotRequest::RequestScreenshot(Path,true,false); }),Delay,false);
		if (Args.Num()>1 && Args[1]==TEXT("quit"))
		{
			FTimerHandle ExitTimer;
			World->GetTimerManager().SetTimer(ExitTimer,FTimerDelegate::CreateLambda([](){ FPlatformMisc::RequestExit(false); }),Delay+3.f,false);
		}
	}));

// Exercise the real focus / press delegates without activating navigation.
static FAutoConsoleCommandWithWorldAndArgs SFStartFeedbackPreview(
    TEXT("SF.Menu.StartFeedbackPreview"), TEXT("SF.Menu.StartFeedbackPreview <0..2>: capture hover / press frames and quit; optional focus leaves the game running."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        if (!World) return;
        const int32 Index=Args.IsEmpty() ? 0 : FMath::Clamp(FCString::Atoi(*Args[0]),0,2);
        const bool bFocusOnly=Args.Num()>1 && Args[1]==TEXT("focus");
        FTimerHandle Ready;
        World->GetTimerManager().SetTimer(Ready,FTimerDelegate::CreateWeakLambda(World,[World,Index,bFocusOnly]()
        {
            TArray<UUserWidget*> Widgets;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Widgets,UStartWidget::StaticClass(),false);
            if (Widgets.IsEmpty()) return;
            const FName Names[]={TEXT("Start_Btn"),TEXT("Setting_Btn"),TEXT("Exit_Btn")};
            auto* Button=Cast<UButton>(Widgets[0]->GetWidgetFromName(Names[Index]));
            if (!Button) return;
            Button->SetKeyboardFocus();
            if (bFocusOnly) return;
            const float Times[]={.27f,.65f,1.32f};
            for (int32 Frame=0;Frame<3;++Frame)
            {
                FTimerHandle Shot;
                const FString Path=FPaths::ProjectSavedDir()/TEXT("StartFeedback")/FString::Printf(TEXT("button_%d_frame_%d.png"),Index,Frame);
                World->GetTimerManager().SetTimer(Shot,FTimerDelegate::CreateLambda([Path]()
                { FScreenshotRequest::RequestScreenshot(Path,true,false); }),Times[Frame],false);
            }
            FTimerHandle Press;
            World->GetTimerManager().SetTimer(Press,FTimerDelegate::CreateWeakLambda(Button,[Button]()
            { Button->OnPressed.Broadcast(); }),1.2f,false);
            FTimerHandle End;
            World->GetTimerManager().SetTimer(End,FTimerDelegate::CreateLambda([](){ FPlatformMisc::RequestExit(false); }),2.8f,false);
        }),bFocusOnly ? .01f : 6.f,false);
    }));

// Hold a departure frame without starting the real navigation timer.
static FAutoConsoleCommandWithWorldAndArgs SFMenuCloudPreview(
	TEXT("SF.Menu.CloudPreview"), TEXT("SF.Menu.CloudPreview <seconds>: inspect a departure frame on the title screen."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
	{
		if (!World || Args.IsEmpty()) return;
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Widgets,USFMenuAtmosphereWidget::StaticClass(),false);
		USFMenuAtmosphereWidget* Effect=Widgets.IsEmpty() ? CreateWidget<USFMenuAtmosphereWidget>(World->GetFirstPlayerController()) : CastChecked<USFMenuAtmosphereWidget>(Widgets[0]);
		if (!Effect) return;
		if (Widgets.IsEmpty()) { Effect->SetDeparture(true); Effect->AddToViewport(50); }
		Effect->SetPreviewTime(FCString::Atof(*Args[0]));
	}));

static FAutoConsoleCommandWithWorldAndArgs SFMenuHeaderPreview(
	TEXT("SF.Menu.HeaderPreview"), TEXT("SF.Menu.HeaderPreview <index>: preview header hover without invoking navigation."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
	{
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Widgets,USFLobbyStagePresentation::StaticClass(),false);
		for (UUserWidget* Widget : Widgets)
			CastChecked<USFLobbyStagePresentation>(Widget)->PreviewHeaderHover=Args.IsEmpty() ? INDEX_NONE : FCString::Atoi(*Args[0]);
	}));

static FAutoConsoleCommandWithWorldAndArgs SFMenuStagePreview(
	TEXT("SF.Menu.StagePreview"), TEXT("SF.Menu.StagePreview <hover index> [launch]: visual only, never travels."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
	{
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Widgets,USFLobbyStagePresentation::StaticClass(),false);
		for (UUserWidget* Widget : Widgets)
		{
			auto* Presentation=CastChecked<USFLobbyStagePresentation>(Widget);
			Presentation->PreviewHover=Args.IsEmpty() ? INDEX_NONE : FCString::Atoi(*Args[0]);
			if (Args.Num()>1) Presentation->PreviewLaunch(Presentation->PreviewHover);
		}
	}));

// Development-only visual regression helper. Captures the actual live widget at its
// authored 1920x1080 size without changing the editor window or any saved assets.
// Set time before a rendered frame, then capture; material uniforms are frame-cached.
static FAutoConsoleCommandWithWorldAndArgs SFMenuPreviewTime(
	TEXT("SF.Menu.PreviewTime"), TEXT("SF.Menu.PreviewTime [seconds]: freeze presentation time; omit to resume."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		TArray<UUserWidget*> Effects;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Effects, USFMenuAtmosphereWidget::StaticClass(), false);
		for (UUserWidget* Effect : Effects)
			CastChecked<USFMenuAtmosphereWidget>(Effect)->SetPreviewTime(
				Args.IsEmpty() ? TOptional<float>() : TOptional<float>(FCString::Atof(*Args[0])));
	}));

static FAutoConsoleCommandWithWorldAndArgs SFMenuCapture(
	TEXT("SF.Menu.Capture"), TEXT("SF.Menu.Capture <name>: capture the active menu and report decoration state."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.IsEmpty()) return;
		TArray<UUserWidget*> Menus;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Menus, ULobbyWidget::StaticClass(), true);
		if (Menus.IsEmpty()) UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Menus, UStartWidget::StaticClass(), true);
		if (Menus.IsEmpty()) return;
		UUserWidget* Menu = Menus[0];
		UE_LOG(LogTemp, Display, TEXT("MENU_VIEWPORT z=%d"), UGameViewportSubsystem::Get(World)->GetWidgetSlot(Menu).ZOrder);
		if (Cast<ULobbyWidget>(Menu))
		{
			const TCHAR* Names[] = { TEXT("Stage1"), TEXT("Stage2"), TEXT("Stage3"), TEXT("Stage4"),
				TEXT("Stage5"), TEXT("Stage6"), TEXT("Stage6_1"), TEXT("Back_Btn"), TEXT("Setting_Btn"), TEXT("Story_Btn") };
			for (const TCHAR* Name : Names)
			{
				UWidget* Widget = Menu->GetWidgetFromName(Name);
				ensureAlwaysMsgf(Widget && Widget->GetParent(), TEXT("Menu control missing: %s"), Name);
				if (UButton* Button=Cast<UButton>(Widget))
					UE_LOG(LogTemp,Display,TEXT("MENU_BUTTON %s enabled=%d bound=%d"),Name,Button->GetIsEnabled(),Button->OnClicked.IsBound());
			}
		}
		TArray<UUserWidget*> Effects;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Effects, USFMenuAtmosphereWidget::StaticClass(), false);
		for (UUserWidget* Effect : Effects)
		{
			UImage* Image = Cast<UImage>(Effect->GetRootWidget());
			UMaterialInstanceDynamic* Material = Image ? Image->GetDynamicMaterial() : nullptr;
			UE_LOG(LogTemp, Display, TEXT("MENU_EFFECT %s elapsed=%.3f departure=%.0f hitInvisible=%d"),
				*Effect->GetName(), Material ? Material->K2_GetScalarParameterValue(TEXT("Elapsed")) : -1.f,
				Material ? Material->K2_GetScalarParameterValue(TEXT("Departure")) : -1.f,
				Effect->GetVisibility() == ESlateVisibility::HitTestInvisible);
			UE_LOG(LogTemp, Display, TEXT("MENU_EFFECT_GEOMETRY size=%s image=%s opacity=%.2f"),
				*Effect->GetCachedGeometry().GetLocalSize().ToString(),
				Image ? *Image->GetCachedGeometry().GetLocalSize().ToString() : TEXT("missing"), Effect->GetRenderOpacity());
		}
		FWidgetRenderer* Renderer = new FWidgetRenderer(false);
		UTextureRenderTarget2D* Target = Renderer->DrawWidget(Menu->TakeWidget(), FVector2D(1920, 1080));
		FBufferArchive Bytes;
		if (Target && FImageUtils::ExportRenderTarget2DAsPNG(Target, Bytes))
		{
			const FString Path = FPaths::ProjectSavedDir() / TEXT("LobbyPolish") / (FPaths::GetCleanFilename(Args[0]) + TEXT(".png"));
			FFileHelper::SaveArrayToFile(Bytes, *Path);
			UE_LOG(LogTemp, Display, TEXT("MENU_CAPTURE %s"), *Path);
		}
		if (Cast<UStartWidget>(Menu) && !Effects.IsEmpty())
		{
			UTextureRenderTarget2D* MistTarget = Renderer->DrawWidget(Effects[0]->TakeWidget(), FVector2D(1920,1080));
			FBufferArchive MistBytes;
			if (MistTarget && FImageUtils::ExportRenderTarget2DAsPNG(MistTarget, MistBytes))
				FFileHelper::SaveArrayToFile(MistBytes, *(FPaths::ProjectSavedDir() / TEXT("LobbyPolish/mist_effect.png")));
		}
		BeginCleanup(Renderer);
	}));

#endif
