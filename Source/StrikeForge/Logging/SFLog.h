// Log 매크로 함수

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"

#if !UE_BUILD_SHIPPING

#define SF_LOG(Verbosity, Format, ...) \
    UE_LOG(                            \
        LogTemp,                       \
        Verbosity,                     \
        TEXT(Format),                  \
        ##__VA_ARGS__                  \
    )

#define SF_SCREEN_LOG(Verbosity, Time, Color, Format, ...)       \
do                                                               \
{                                                                \
    const FString SFDebugMessage = FString::Printf(              \
        TEXT(Format),                                            \
        ##__VA_ARGS__                                            \
    );                                                           \
                                                                 \
    UE_LOG(                                                      \
        LogTemp,                                                 \
        Verbosity,                                               \
        TEXT("%s"),                                              \
        *SFDebugMessage                                          \
    );                                                           \
                                                                 \
    if (IsValid(GEngine))                                        \
    {                                                            \
        GEngine->AddOnScreenDebugMessage(                        \
            -1,                                                  \
            Time,                                                \
            Color,                                               \
            SFDebugMessage                                       \
        );                                                       \
    }                                                            \
}                                                                \
while (false)

#else

#define SF_LOG(Verbosity, Format, ...)
#define SF_SCREEN_LOG(Verbosity, Time, Color, Format, ...)

#endif