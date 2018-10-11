// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "MyGameEngine.generated.h"

/**
 * 
 */
UCLASS()
class REPLAYMOVEMENT_API UMyGameEngine : public UGameEngine
{
	GENERATED_BODY()
	
public:
	virtual bool Experimental_ShouldPreDuplicateMap(const FName MapName) const override;
	
};
