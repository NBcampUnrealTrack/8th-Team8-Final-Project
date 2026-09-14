#include "SFRTSCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"

ASFRTSCameraPawn::ASFRTSCameraPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    CameraCollision = CreateDefaultSubobject<USphereComponent>(
        TEXT("CameraCollision")
    );

    SetRootComponent(CameraCollision);

    CameraCollision->InitSphereRadius(50.0f);
    CameraCollision->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly
    );
    CameraCollision->SetCollisionObjectType(ECC_Pawn);
    CameraCollision->SetCollisionResponseToAllChannels(
        ECR_Ignore
    );
    CameraCollision->SetCollisionResponseToChannel(
        ECC_GameTraceChannel2,
        ECR_Block
    );
    CameraCollision->SetGenerateOverlapEvents(false);
    CameraCollision->SetCanEverAffectNavigation(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("SceneRoot")
    );

    SceneRoot->SetupAttachment(CameraCollision);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(
        TEXT("SpringArm")
    );

    SpringArm->SetupAttachment(SceneRoot);
    SpringArm->TargetArmLength = InitialCameraDistance;

    SpringArm->SetRelativeRotation(
        FRotator(CameraPitch, 0.0f, 0.0f)
    );

    // 카메라 폰의 이동 경계만 사용한다.
    // SpringArm 충돌 검사를 켜면 경계에 닿을 때 붐 길이가 줄어들어
    // 특정 방향에서 카메라가 확대되는 현상이 발생한다.
    SpringArm->bDoCollisionTest = false;
    SpringArm->ProbeChannel = ECC_GameTraceChannel2;
    SpringArm->ProbeSize = 12.0f;
    SpringArm->bUsePawnControlRotation = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(
        TEXT("Camera")
    );

    Camera->SetupAttachment(
        SpringArm,
        USpringArmComponent::SocketName
    );

    Camera->bUsePawnControlRotation = false;

    FloatingPawnMovement =
        CreateDefaultSubobject<UFloatingPawnMovement>(
            TEXT("FloatingPawnMovement")
        );

    FloatingPawnMovement->MaxSpeed = CameraMoveSpeed;
    FloatingPawnMovement->Acceleration = 6000.0f;
    FloatingPawnMovement->Deceleration = 8000.0f;
}

void ASFRTSCameraPawn::BeginPlay()
{
    Super::BeginPlay();

    const float SafeMinimumDistance = FMath::Min(
        MinimumCameraDistance,
        MaximumCameraDistance
    );

    const float SafeMaximumDistance = FMath::Max(
        MinimumCameraDistance,
        MaximumCameraDistance
    );

    if (IsValid(SpringArm))
    {
        SpringArm->TargetArmLength = FMath::Clamp(
            InitialCameraDistance,
            SafeMinimumDistance,
            SafeMaximumDistance
        );

        SpringArm->SetRelativeRotation(
            FRotator(CameraPitch, 0.0f, 0.0f)
        );

        // 카메라 폰의 이동 경계만 사용하고 SpringArm은 길이를 유지한다.
        SpringArm->bDoCollisionTest = false;
        SpringArm->ProbeChannel = ECC_GameTraceChannel2;
    }

    if (IsValid(FloatingPawnMovement))
    {
        FloatingPawnMovement->MaxSpeed = CameraMoveSpeed;

        FloatingPawnMovement->SetUpdatedComponent(
            CameraCollision
        );
    }

    if (IsValid(CameraCollision))
    {
        // BP의 Collision Profile이 C++ 기본값을 덮어쓰는 경우를 방지한다.
        // 실제 Pawn Sweep과 SpringArm Trace 모두 CameraBoundary만 사용한다.
        CameraCollision->SetCollisionEnabled(
            ECollisionEnabled::QueryOnly
        );
        CameraCollision->SetCollisionObjectType(ECC_Pawn);
        CameraCollision->SetCollisionResponseToAllChannels(
            ECR_Ignore
        );
        CameraCollision->SetCollisionResponseToChannel(
            ECC_GameTraceChannel2,
            ECR_Block
        );
        CameraCollision->SetGenerateOverlapEvents(false);
    }
}

void ASFRTSCameraPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    HandleEdgeScroll();
    ClampCameraLocation();
}

void ASFRTSCameraPawn::SetupPlayerInputComponent(
    UInputComponent* PlayerInputComponent
)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInputComponent =
        Cast<UEnhancedInputComponent>(
            PlayerInputComponent
        );

    if (!IsValid(EnhancedInputComponent))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "SFRTSCameraPawn: "
                "EnhancedInputComponent를 찾지 못했습니다."
            )
        );

        return;
    }

    if (IsValid(CameraMoveAction))
    {
        EnhancedInputComponent->BindAction(
            CameraMoveAction,
            ETriggerEvent::Triggered,
            this,
            &ASFRTSCameraPawn::MoveCamera
        );
    }
    else
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "SFRTSCameraPawn: "
                "CameraMoveAction이 설정되지 않았습니다."
            )
        );
    }

    if (IsValid(CameraZoomAction))
    {
        EnhancedInputComponent->BindAction(
            CameraZoomAction,
            ETriggerEvent::Triggered,
            this,
            &ASFRTSCameraPawn::ZoomCamera
        );
    }
    else
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "SFRTSCameraPawn: "
                "CameraZoomAction이 설정되지 않았습니다."
            )
        );
    }
}

void ASFRTSCameraPawn::MoveCamera(
    const FInputActionValue& InputValue
)
{
    const FVector2D MovementInput =
        InputValue.Get<FVector2D>();

    ApplyCameraMovement(MovementInput);
}

void ASFRTSCameraPawn::ZoomCamera(
    const FInputActionValue& InputValue
)
{
    if (!IsValid(SpringArm))
    {
        return;
    }

    const float ZoomInput = InputValue.Get<float>();

    if (FMath::IsNearlyZero(ZoomInput))
    {
        return;
    }

    const float SafeMinimumDistance = FMath::Min(
        MinimumCameraDistance,
        MaximumCameraDistance
    );

    const float SafeMaximumDistance = FMath::Max(
        MinimumCameraDistance,
        MaximumCameraDistance
    );

    const float NewCameraDistance =
        SpringArm->TargetArmLength
        - ZoomInput * CameraZoomSpeed;

    SpringArm->TargetArmLength = FMath::Clamp(
        NewCameraDistance,
        SafeMinimumDistance,
        SafeMaximumDistance
    );
}

void ASFRTSCameraPawn::HandleEdgeScroll()
{
    if (!bEnableEdgeScroll)
    {
        return;
    }

    APlayerController* PlayerController =
        Cast<APlayerController>(GetController());

    if (!IsValid(PlayerController))
    {
        return;
    }

    int32 ViewportSizeX = 0;
    int32 ViewportSizeY = 0;

    PlayerController->GetViewportSize(
        ViewportSizeX,
        ViewportSizeY
    );

    if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
    {
        return;
    }

    float MousePositionX = 0.0f;
    float MousePositionY = 0.0f;

    const bool bHasMousePosition =
        PlayerController->GetMousePosition(
            MousePositionX,
            MousePositionY
        );

    if (!bHasMousePosition)
    {
        return;
    }

    if (
        MousePositionX < 0.0f ||
        MousePositionX > static_cast<float>(ViewportSizeX) ||
        MousePositionY < 0.0f ||
        MousePositionY > static_cast<float>(ViewportSizeY)
        )
    {
        return;
    }

    const float MaximumSafeThreshold =
        FMath::Min(
            static_cast<float>(ViewportSizeX),
            static_cast<float>(ViewportSizeY)
        ) * 0.45f;

    const float SafeThreshold = FMath::Clamp(
        EdgeScrollThreshold,
        1.0f,
        MaximumSafeThreshold
    );

    FVector2D EdgeMovementInput = FVector2D::ZeroVector;

    if (MousePositionX <= SafeThreshold)
    {
        EdgeMovementInput.X = -1.0f;
    }
    else if (
        MousePositionX >=
        static_cast<float>(ViewportSizeX) - SafeThreshold
        )
    {
        EdgeMovementInput.X = 1.0f;
    }

    if (MousePositionY <= SafeThreshold)
    {
        EdgeMovementInput.Y = 1.0f;
    }
    else if (
        MousePositionY >=
        static_cast<float>(ViewportSizeY) - SafeThreshold
        )
    {
        EdgeMovementInput.Y = -1.0f;
    }

    if (EdgeMovementInput.IsNearlyZero())
    {
        return;
    }

    EdgeMovementInput =
        EdgeMovementInput.GetSafeNormal()
        * EdgeScrollInputScale;

    ApplyCameraMovement(EdgeMovementInput);
}

void ASFRTSCameraPawn::ApplyCameraMovement(
    const FVector2D& MovementInput
)
{
    if (MovementInput.IsNearlyZero())
    {
        return;
    }

    const float InputStrength = FMath::Clamp(
        MovementInput.Size(),
        0.0f,
        1.0f
    );

    const FVector MovementDirection =
        FVector::ForwardVector * MovementInput.Y
        + FVector::RightVector * MovementInput.X;

    AddMovementInput(
        MovementDirection.GetSafeNormal(),
        InputStrength
    );
}

void ASFRTSCameraPawn::ClampCameraLocation()
{
    if (!bConstrainCameraToBounds)
    {
        return;
    }

    const float SafeMinimumX = FMath::Min(
        MinimumCameraBounds.X,
        MaximumCameraBounds.X
    );

    const float SafeMaximumX = FMath::Max(
        MinimumCameraBounds.X,
        MaximumCameraBounds.X
    );

    const float SafeMinimumY = FMath::Min(
        MinimumCameraBounds.Y,
        MaximumCameraBounds.Y
    );

    const float SafeMaximumY = FMath::Max(
        MinimumCameraBounds.Y,
        MaximumCameraBounds.Y
    );

    const FVector CurrentLocation = GetActorLocation();

    const FVector ClampedLocation(
        FMath::Clamp(
            CurrentLocation.X,
            SafeMinimumX,
            SafeMaximumX
        ),
        FMath::Clamp(
            CurrentLocation.Y,
            SafeMinimumY,
            SafeMaximumY
        ),
        CurrentLocation.Z
    );

    if (!CurrentLocation.Equals(ClampedLocation))
    {
        SetActorLocation(ClampedLocation, true);
    }
}

void ASFRTSCameraPawn::SetEdgeScrollEnabled(
    bool bEnabled
)
{
    bEnableEdgeScroll = bEnabled;
}

void ASFRTSCameraPawn::FocusOnWorldLocation(
    const FVector& WorldLocation
)
{
    FVector NewLocation =
        GetActorLocation();

    NewLocation.X =
        WorldLocation.X;

    NewLocation.Y =
        WorldLocation.Y;

    SetActorLocation(
        NewLocation,
        true
    );

    ClampCameraLocation();
}
