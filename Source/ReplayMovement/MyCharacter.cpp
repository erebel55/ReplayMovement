// Fill out your copyright notice in the Description page of Project Settings.

#include "MyCharacter.h"


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
		// ETHAN TEST
		/*IConsoleVariable* CVar3 = IConsoleManager::Get().FindConsoleVariable(TEXT("p.ReplayUseInterpolation"));
		if (CVar3)
		{
			CVar3->Set(false);
		}*/

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

