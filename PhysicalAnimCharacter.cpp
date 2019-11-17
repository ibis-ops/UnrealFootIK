// Copyright Sean Woodward (C) All Rights Reserved.

#include "PhysicalAnimCharacter.h"

#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"

#include "Engine.h"

//////////////////////////////////////////////////////////////////////////
// APhysicalAnimCharacter

APhysicalAnimCharacter::APhysicalAnimCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	//Create a camera boom (pulls in towards the player if there is a collision)
	//CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	//CameraBoom->SetupAttachment(RootComponent);
	//CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	//FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	
	FollowCamera->bUsePawnControlRotation = true; // Camera does not rotate relative to arm
	FollowCamera->SetupAttachment()
	CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	PhysicsAnimation = CreateDefaultSubobject<UPhysicalAnimationComponent>(TEXT("PhysicsAnimation"));

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

void APhysicalAnimCharacter::BeginPlay()
{
	Super::BeginPlay();

	FName BoneName = FName("head");
	PhysicsAnimation->SetSkeletalMeshComponent(GetMesh());
	PhysicsAnimation->ApplyPhysicalAnimationProfileBelow(BoneName, FName("Physics"), true, true);
	GetMesh()->SetAllBodiesBelowSimulatePhysics(BoneName, true, false);
}


void APhysicalAnimCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (FMath::IsNearlyZero(GetVelocity().Size()))
	{
		UpdateIK(DeltaTime);
	}
}

// Taking in Deltatime and calling simulation functions 
void APhysicalAnimCharacter::UpdateIK(float DeltaTime)
{
	TraceFoot(LeftFootSocket, LeftFootOffset, LeftFootRotation, DeltaTime);
	TraceFoot(RightFootSocket, RightFootOffset, RightFootRotation, DeltaTime);
	UpdateHip(DeltaTime);
	UpdateFootEffector(LeftEffectorLocation, LeftFootOffset, DeltaTime);
	UpdateFootEffector(RightEffectorLocation, RightFootOffset, DeltaTime);
}

void APhysicalAnimCharacter::TraceFoot(FName SocketName, float& OutOffset, FRotator& OutRotation, float DeltaTime)
{
	FVector SocketLocation = GetMesh()->GetSocketLocation(SocketName);
	FVector ActorLocation = GetActorLocation();

	FVector Start = FVector(SocketLocation.X, SocketLocation.Y, ActorLocation.Z);
	FVector End = FVector(SocketLocation.X, SocketLocation.Y, ActorLocation.Z - CapsuleHalfHeight - TraceDistance);
	
	FHitResult Hit;
	FCollisionQueryParams CollisionParams;
	CollisionParams.bTraceComplex = true;
	CollisionParams.AddIgnoredActor(this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, CollisionParams))
	{
		OutOffset = (Hit.Location - Hit.TraceEnd).Size() - TraceDistance + AdjustOffset;
		FRotator NewRotation = FRotator(-FMath::RadiansToDegrees(FMath::Atan2(Hit.Normal.X, Hit.Normal.Z)), 0.0f, FMath::RadiansToDegrees(FMath::Atan2(Hit.Normal.Y, Hit.Normal.Z)));
		OutRotation = FMath::RInterpTo(OutRotation, NewRotation, DeltaTime, FootInterpSpeed);
	}
	else
	{
		OutOffset = 0;
	}
}

void APhysicalAnimCharacter::UpdateHip(float DeltaTime)
{
	float NewOffset = FMath::Min(FMath::Min(LeftFootOffset, RightFootOffset), 0.0f);
	NewOffset = (NewOffset < 0) ? NewOffset : 0;
	HipOffset = FMath::FInterpTo(HipOffset, NewOffset, DeltaTime, HipInterpSpeed);

	float NewCapsuleHalfHeight = CapsuleHalfHeight - FMath::Abs(HipOffset) / 2.0f;
	GetCapsuleComponent()->SetCapsuleHalfHeight(FMath::FInterpTo(GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), NewCapsuleHalfHeight, DeltaTime, HipInterpSpeed));
}

void APhysicalAnimCharacter::UpdateFootEffector(float& OutEffectorLocation, float FootOffset, float DeltaTime)
{
	OutEffectorLocation = FMath::FInterpTo(OutEffectorLocation, FootOffset - HipOffset, DeltaTime, FootInterpSpeed);
}

//////////////////////////////////////////////////////////////////////////
// Input

void APhysicalAnimCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &APhysicalAnimCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APhysicalAnimCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &APhysicalAnimCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &APhysicalAnimCharacter::LookUpAtRate);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &APhysicalAnimCharacter::OnResetVR);
	//DEBUG
	PlayerInputComponent->BindAction("CameraChange", IE_Pressed, this, &APhysicalAnimCharacter::CameraChangeTest);

	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &APhysicalAnimCharacter::AttackStart);
	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &APhysicalAnimCharacter::AttackStop);
}

void APhysicalAnimCharacter::CameraChangeTest()
{
	Log(ELogLevel::INFO, __FUNCTION__);
}

void APhysicalAnimCharacter::AttackStart()
{
	Log(ELogLevel::INFO,  __FUNCTION__);
}

void APhysicalAnimCharacter::AttackStop()
{
	Log(ELogLevel::INFO, __FUNCTION__);
}

void APhysicalAnimCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}


void APhysicalAnimCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void APhysicalAnimCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void APhysicalAnimCharacter::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void APhysicalAnimCharacter::MoveRight(float Value)
{
	if ( (Controller != NULL) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

void APhysicalAnimCharacter::Log(ELogLevel LogLevel, FString Message)
{
	Log(LogLevel, Message, ELogOutput::ALL);
}

void APhysicalAnimCharacter::Log(ELogLevel LogLevel, FString Message, ELogOutput LogOutput)
{
	// only print when screen is selected and the GEngine object is available
	if ((LogOutput == ELogOutput::ALL || LogOutput == ELogOutput::SCREEN) && GEngine)
	{
		// default color
		FColor LogColor = FColor::Cyan;
		// flip the color based on the type
		switch (LogLevel)
		{
		case ELogLevel::TRACE:
			LogColor = FColor::Green;
			break;
		case ELogLevel::DEBUG:
			LogColor = FColor::Cyan;
			break;
		case ELogLevel::INFO:
			LogColor = FColor::White;
			break;
		case ELogLevel::WARNING:
			LogColor = FColor::Yellow;
			break;
		case ELogLevel::ERROR:
			LogColor = FColor::Red;
			break;
		default:
			break;
		}
		// print the message and leave it on screen ( 4.5f controls the duration )
		GEngine->AddOnScreenDebugMessage(-1, 4.5f, LogColor, Message);
	}

	if (LogOutput == ELogOutput::ALL || LogOutput == ELogOutput::OUTPUT_LOG)
	{
		// flip the message type based on error level
		switch (LogLevel)
		{
		case ELogLevel::TRACE:
			UE_LOG(LogTemp, VeryVerbose, TEXT("%s"), *Message);
			break;
		case ELogLevel::DEBUG:
			UE_LOG(LogTemp, Verbose, TEXT("%s"), *Message);
			break;
		case ELogLevel::INFO:
			UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
			break;
		case ELogLevel::WARNING:
			UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
			break;
		case ELogLevel::ERROR:
			UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
			break;
		default:
			UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
			break;
		}
	}
}