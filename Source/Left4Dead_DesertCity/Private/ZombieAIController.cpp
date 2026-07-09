// Fill out your copyright notice in the Description page of Project Settings.

#include "ZombieAIController.h"
#include "ZombieCharacter.h"
#include "ZombieCrowdFollowingComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"

AZombieAIController::AZombieAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UZombieCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	CurrentMode = EZombieAIMode::Simple;

	ZombiePerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("ZombiePerceptionComponent"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));

	if (ZombiePerceptionComponent)
	{
		SetPerceptionComponent(*ZombiePerceptionComponent);
	}

	if (SightConfig && ZombiePerceptionComponent)
	{
		SightConfig->SightRadius = sightRadius;
		SightConfig->LoseSightRadius = sightRadius + 500.f;
		SightConfig->PeripheralVisionAngleDegrees = 60.f;
		SightConfig->SetMaxAge(5.f);
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

		ZombiePerceptionComponent->ConfigureSense(*SightConfig);
		ZombiePerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	}

	if (HearingConfig && ZombiePerceptionComponent)
	{
		HearingConfig->HearingRange = hearRadius;
		HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
		HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
		HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;

		ZombiePerceptionComponent->ConfigureSense(*HearingConfig);
	}

	if (ZombiePerceptionComponent)
	{
		ZombiePerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
			this,
			&AZombieAIController::OnTargetPerceptionUpdated);
	}
}

void AZombieAIController::BeginPlay()
{
	Super::BeginPlay();

	SetPerceptionEnabled(false);
	SetCrowdAvoidanceEnabled(false);
}

void AZombieAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!bPerceptionEnabled || CurrentMode != EZombieAIMode::Simple)
	{
		return;
	}

	if (!Actor || !Stimulus.WasSuccessfullySensed())
	{
		return;
	}

	AZombieCharacter* ZombieChar = Cast<AZombieCharacter>(GetPawn());
	if (!ZombieChar)
	{
		return;
	}

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		SetAIMode(EZombieAIMode::Quality);
		ZombieChar->QualityMove(Actor);
		return;
	}

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		SetAIMode(EZombieAIMode::Simple);
		ZombieChar->SimpleMove(Stimulus.StimulusLocation);
	}
}

void AZombieAIController::SetPerceptionEnabled(bool bEnable)
{
	bPerceptionEnabled = bEnable;

	if (!ZombiePerceptionComponent)
	{
		return;
	}

	ZombiePerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), bEnable);
	ZombiePerceptionComponent->SetSenseEnabled(UAISense_Hearing::StaticClass(), bEnable);
}

void AZombieAIController::SetAIMode(EZombieAIMode NewMode)
{
	CurrentMode = NewMode;

	if (CurrentMode == EZombieAIMode::Quality)
	{
		SetPerceptionEnabled(false);
		SetCrowdAvoidanceEnabled(true);
	}
	else
	{
		SetCrowdAvoidanceEnabled(false);
	}
}

void AZombieAIController::SetCrowdAvoidanceEnabled(bool bEnable)
{
	UCrowdFollowingComponent* CrowdFollowComp = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent());
	if (!CrowdFollowComp)
	{
		return;
	}

	if (bEnable)
	{
		CrowdFollowComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Good);
	}
	else
	{
		CrowdFollowComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Low);
	}
}

void AZombieAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (CurrentMode == EZombieAIMode::Simple && Result.Code == EPathFollowingResult::Success)
	{
		if (AZombieCharacter* ZombieChar = Cast<AZombieCharacter>(GetPawn()))
		{
			ZombieChar->OnSimpleMoveTargetReached();
		}
	}
}
