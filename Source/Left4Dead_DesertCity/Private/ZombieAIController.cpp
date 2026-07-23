#include "ZombieAIController.h"

#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Kismet/GameplayStatics.h"
#include "ZombieCharacter.h"
#include "ZombieCrowdFollowingComponent.h"
#include "ZombieManager.h"

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
		SightConfig->LoseSightRadius = sightRadius + 500.0f;
		SightConfig->PeripheralVisionAngleDegrees = 60.0f;
		SightConfig->SetMaxAge(5.0f);
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

void AZombieAIController::ApplyStatsRow(const FZombieStatsRow& StatsRow)
{
	sightRadius = StatsRow.SightRadius;
	hearRadius = FMath::Max(StatsRow.HearRadius, MinimumHearingRange);
	loseSightRadiusOffset = StatsRow.LoseSightRadiusOffset;
	peripheralVisionAngleDegrees = StatsRow.PeripheralVisionAngleDegrees;
	sightMaxAge = StatsRow.SightMaxAge;

	if (SightConfig)
	{
		SightConfig->SightRadius = sightRadius;
		SightConfig->LoseSightRadius = sightRadius + loseSightRadiusOffset;
		SightConfig->PeripheralVisionAngleDegrees = peripheralVisionAngleDegrees;
		SightConfig->SetMaxAge(sightMaxAge);
	}

	if (HearingConfig)
	{
		HearingConfig->HearingRange = hearRadius;
	}

	if (ZombiePerceptionComponent)
	{
		ZombiePerceptionComponent->RequestStimuliListenerUpdate();
	}
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
		const AActor* PlayerActor = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Actor != PlayerActor && !Actor->ActorHasTag("Player"))
		{
			return;
		}

		SetAIMode(EZombieAIMode::Quality);
		ZombieChar->QualityMove(Actor);
		return;
	}

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		if (const AZombieManager* Manager = Cast<AZombieManager>(ZombieChar->GetOwner());
			Manager && Manager->IsZombieMoveFocusActive())
		{
			return;
		}

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

	if (bEnable)
	{
		ZombiePerceptionComponent->RequestStimuliListenerUpdate();
	}
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
		CrowdFollowComp->SetCrowdSimulationState(ECrowdSimulationState::Enabled);
		CrowdFollowComp->SuspendCrowdSteering(false);
		CrowdFollowComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Good);
	}
	else
	{
		CrowdFollowComp->SuspendCrowdSteering(true);
		CrowdFollowComp->SetCrowdSimulationState(ECrowdSimulationState::Disabled);
		CrowdFollowComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Low);
	}
}

void AZombieAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (AZombieCharacter* ZombieChar = Cast<AZombieCharacter>(GetPawn()))
	{
		if (ZombieChar->IsZombieDeactivated())
		{
			return;
		}

		if (CurrentMode == EZombieAIMode::Simple)
		{
			ZombieChar->OnSimpleMoveFinished(Result.Code);
		}
		else
		{
			ZombieChar->OnQualityMoveFinished(Result.Code);
		}
	}
}
