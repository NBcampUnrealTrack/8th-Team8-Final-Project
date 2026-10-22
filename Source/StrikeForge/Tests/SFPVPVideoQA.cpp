#if WITH_DEV_AUTOMATION_TESTS
#include "SFPVPVideoQA.h"
#include "FrameGrabber.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "AudioMixerBlueprintLibrary.h"

namespace
{
struct FVideoFrame : IFramePayload
{
    int32 Index;
    double Time;
    FVideoFrame(int32 InIndex,double InTime):Index(InIndex),Time(InTime){}
};

struct FVideoRecording
{
    TWeakObjectPtr<UWorld> World;
    TUniquePtr<FFrameGrabber> Grabber;
    FString Folder,Manifest;
    double Started=0,Next=0;
    float Duration=0;
    int32 Requested=0,Saved=0;

    void Drain()
    {
        for(auto& Frame:Grabber->GetCapturedFrames())
        {
            const auto* Payload=Frame.GetPayload<FVideoFrame>();
            const FString Name=FString::Printf(TEXT("frame_%06d.jpg"),Payload->Index);
            const FImageView View(Frame.ColorBuffer.GetData(),Frame.BufferSize.X,Frame.BufferSize.Y);
            if(FImageUtils::SaveImageByExtension(*(Folder/Name),View,90))
            {Manifest+=FString::Printf(TEXT("%s,%.6f\n"),*Name,Payload->Time);++Saved;}
        }
    }

    bool Tick(float)
    {
        const double Elapsed=FPlatformTime::Seconds()-Started;
        Drain();
        if(!World.IsValid()||Elapsed>=Duration)
        {
            Grabber->StopCapturingFrames();Grabber->Shutdown();Drain();
            if(World.IsValid())UAudioMixerBlueprintLibrary::StopRecordingOutput(World.Get(),EAudioRecordingExportType::WavFile,TEXT("audio"),Folder);
            FFileHelper::SaveStringToFile(Manifest,*(Folder/TEXT("frames.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            FFileHelper::SaveStringToFile(FString::Printf(TEXT("frames=%d seconds=%.3f\n"),Saved,Elapsed),*(Folder/TEXT("complete.txt")));
            UE_LOG(LogTemp,Display,TEXT("[PVPVideo] complete frames=%d seconds=%.3f folder=%s"),Saved,Elapsed,*Folder);
            return false;
        }
        if(Elapsed>=Next)
        {
            Grabber->CaptureThisFrame(MakeShared<FVideoFrame,ESPMode::ThreadSafe>(Requested++,Elapsed));
            Next=Elapsed+.05;
        }
        return true;
    }
};
}

void SFStartPVPVideoQA(UWorld* World,const FString& Folder,float Seconds)
{
    auto* Engine=Cast<UGameEngine>(GEngine);
    if(!World||!Engine||!Engine->SceneViewport.IsValid())return;
    auto Recording=MakeShared<FVideoRecording>();
    Recording->World=World;Recording->Duration=FMath::Clamp(Seconds,1.f,600.f);
    Recording->Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("PVPVideo")/FPaths::GetCleanFilename(Folder));
    IFileManager::Get().MakeDirectory(*Recording->Folder,true);
    Recording->Grabber=MakeUnique<FFrameGrabber>(Engine->SceneViewport.ToSharedRef(),Engine->SceneViewport->GetSizeXY());
    Recording->Grabber->StartCapturingFrames();
    Recording->Started=FPlatformTime::Seconds();
    FApp::SetUnfocusedVolumeMultiplier(1.f);
    UAudioMixerBlueprintLibrary::StartRecordingOutput(World,Recording->Duration+2);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Recording](float Delta){return Recording->Tick(Delta);}));
    UE_LOG(LogTemp,Display,TEXT("[PVPVideo] recording %s"),*Recording->Folder);
}
#endif
