// Fill out your copyright notice in the Description page of Project Settings.

#include "MyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

// Sets default values
AMyCharacter::AMyCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (GetWorld() && GetWorld()->DemoNetDriver)
	{
		APlayerController* const LocalPC = GetWorld()->GetFirstPlayerController();
		if (LocalPC)
		{
			LocalPC->SetViewTarget(this);
		}
	}

	// Start recording the replay
	if (IsLocallyControlled())
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("demo.UseAdaptiveReplayUpdateFrequency"));
		if (CVar)
		{
			CVar->Set(false);
		}
		const int32 DemoRecordHz = 60;
		IConsoleVariable* CVar2 = IConsoleManager::Get().FindConsoleVariable(TEXT("demo.RecordHz"));
		if (CVar2)
		{
			CVar2->Set(DemoRecordHz);
		}
		IConsoleVariable* CVar3 = IConsoleManager::Get().FindConsoleVariable(TEXT("p.ReplayUseInterpolation"));
		if (CVar3)
		{
			CVar3->Set(false);
		}

		UGameInstance* const GI = GetGameInstance();
		if (GI)
		{
			const FString KillcamReplayName = TEXT("_KillCam");
			TArray<FString> ReplayOptions;
			// Use the memory streamer
			ReplayOptions.Add("ReplayStreamerOverride=InMemoryNetworkReplayStreaming");
			// Don't spawn a separate spectator
			ReplayOptions.Add("SkipSpawnSpectatorController");

			GI->StartRecordingReplay(KillcamReplayName, KillcamReplayName, ReplayOptions);
		}
	}
}

// Called every frame
void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetWorld() && GetWorld()->DemoNetDriver && GetWorld()->IsPlayingReplay())
	{
		GEngine->AddOnScreenDebugMessage(-1, .1f, FColor::Red, FString::Printf(TEXT("Replay Time: %f"), GetWorld()->DemoNetDriver->DemoTotalTime - GetWorld()->DemoNetDriver->DemoCurrentTime));
	}

}

// Called to bind functionality to input
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &AMyCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AMyCharacter::MoveRight);
	PlayerInputComponent->BindAction("PlayReplay", IE_Pressed, this, &AMyCharacter::PlayReplay);

	//PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	//PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
}

void AMyCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AMyCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AMyCharacter::PlayReplay()
{
	UWorld* const TheWorld = GetWorld();

	FLevelCollection* const SourceCollection = TheWorld->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	if (SourceCollection)
	{
		// Hide the gameplay level
		SourceCollection->SetIsVisible(false);

		for (ULevel* Level : SourceCollection->GetLevels())
		{
			if (Level)
			{
				for (AActor* Actor : Level->Actors)
				{
					if (Actor)
					{
						for (UActorComponent* Comp : Actor->GetComponents())
						{
							// This will ensure the components are shown again
							Comp->MarkRenderStateDirty();
						}
					}
				}
			}
		}
	}

	FLevelCollection* const DuplicatedCollection = TheWorld->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels);
	if (DuplicatedCollection)
	{
		DuplicatedCollection->SetIsVisible(true);

		// Loop over the levels in this collection
		for (ULevel* Level : DuplicatedCollection->GetLevels())
		{
			if (Level)
			{
				while (!Level->bIsVisible)
				{
					// Add the level to the world
					TheWorld->AddToWorld(Level);
				}

				// Loop over all of the actors in this level
				for (AActor* Actor : Level->Actors)
				{
					if (Actor)
					{
						for (UActorComponent* Comp : Actor->GetComponents())
						{
							// This will ensure the components are shown again
							Comp->MarkRenderStateDirty();
						}
					}
				}
			}
		}
	}

	UGameInstance* const GI = GetGameInstance();
	if (TheWorld && GI)
	{
		if (TheWorld->DemoNetDriver)
		{
			TArray<FString> ReplayOptions;
			ReplayOptions.Add("ReplayStreamerOverride=InMemoryNetworkReplayStreaming");
			ReplayOptions.Add("LevelPrefixOverride=1");

			GI->PlayReplay(TEXT("_KillCam"), nullptr, ReplayOptions);

			TheWorld->DemoNetDriver->bIsLocalReplay = true;
			TheWorld->DemoNetDriver->GotoTimeInSeconds(1.f);
		}
	}
}

void AMyCharacter::OnRep_ReplayLastTransformUpdateTimeStamp()
{
	ReplicatedServerLastTransformUpdateTimeStamp = ReplayLastTransformUpdateTimeStamp;
}

void AMyCharacter::PreReplicationForReplay(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	// NOTE: purposely not calling Super since that is old behavior and we are using 4.22 stuff here

	// If this is a replay, we save out certain values we need to runtime to do smooth interpolation
	// We'll be able to look ahead in the replay to have these ahead of time for smoother playback
	FCharacterReplaySample ReplaySample;

	const UWorld* World = GetWorld();
	UCharacterMovementComponent* const MyCharacterMovement = GetCharacterMovement();

	// If this is a client-recorded replay, use the mesh location and rotation, since these will always
	// be smoothed - unlike the actor position and rotation.
	const USkeletalMeshComponent* const MeshComponent = GetMesh();
	if (MeshComponent && World && World->IsRecordingClientReplay())
	{
		FNetworkPredictionData_Client_Character const* const ClientNetworkPredicationData = MyCharacterMovement->GetPredictionData_Client_Character();
		if ((Role == ROLE_SimulatedProxy) && ClientNetworkPredicationData)
		{
			ReplaySample.Location = GetActorLocation() + ClientNetworkPredicationData->MeshTranslationOffset;
			if (MyCharacterMovement->NetworkSmoothingMode == ENetworkSmoothingMode::Exponential)
			{
				ReplaySample.Rotation = GetActorRotation() + ClientNetworkPredicationData->MeshRotationOffset.Rotator();
			}
			else
			{
				ReplaySample.Rotation = ClientNetworkPredicationData->MeshRotationOffset.Rotator();
			}
		}
		else
		{
			// Remove the base transform from the mesh's transform, since on playback the base transform
			// will be stored in the mesh's RelativeLocation and RelativeRotation.
			const FTransform BaseTransform(GetBaseRotationOffset(), GetBaseTranslationOffset());
			const FTransform MeshRootTransform = BaseTransform.Inverse() * MeshComponent->GetComponentTransform();

			ReplaySample.Location = MeshRootTransform.GetLocation();
			ReplaySample.Rotation = MeshRootTransform.GetRotation().Rotator();
		}

		// On client replays, our view pitch will be set to 0 as by default we do not replicate
		// pitch for owners, just for simulated. So instead push our rotation into the sampler
		if (Controller != nullptr && Role == ROLE_AutonomousProxy && GetNetMode() == NM_Client)
		{
			SetRemoteViewPitch(Controller->GetControlRotation().Pitch);
		}
	}
	else
	{
		ReplaySample.Location = GetActorLocation();
		ReplaySample.Rotation = GetActorRotation();
	}

	ReplaySample.Velocity = GetVelocity();
	ReplaySample.Acceleration = MyCharacterMovement->GetCurrentAcceleration();
	ReplaySample.RemoteViewPitch = RemoteViewPitch;

	if (World)
	{
		if (World->DemoNetDriver)
		{
			ReplaySample.Time = World->DemoNetDriver->DemoCurrentTime;
		}

		ReplayLastTransformUpdateTimeStamp = World->GetTimeSeconds();
	}

	FBitWriter Writer(0, true);
	Writer << ReplaySample;

	ChangedPropertyTracker.SetExternalData(Writer.GetData(), Writer.GetNumBits());
}

void AMyCharacter::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AMyCharacter, ReplayLastTransformUpdateTimeStamp, COND_ReplayOnly);

	// This is needed to override the condition in Character (can be removed in 4.22)
	DOREPLIFETIME_CHANGE_CONDITION(AActor, ReplicatedMovement, COND_SimulatedOrPhysics);
}