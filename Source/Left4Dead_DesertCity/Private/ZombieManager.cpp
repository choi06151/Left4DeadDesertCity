#include "ZombieManager.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "ZombieAIController.h"
#include "ZombieCharacter.h"

AZombieManager::AZombieManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AZombieManager::BeginPlay()
{
	Super::BeginPlay();

	Player = UGameplayStatics::GetPlayerPawn(this, 0);
	InitializeZombies();
}

void AZombieManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AZombieManager::InitializeZombies()
{
	if (!ZombieClassSlot)
	{
		UE_LOG(LogTemp, Error, TEXT("ZombieClassSlot is not assigned on ZombieManager."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector InvisibleLocation(0.0f, 0.0f, -10000.0f);
	const FRotator SpawnRotation = FRotator::ZeroRotator;

	for (int32 i = 0; i < SpawnEnableZombieCount; i++)
	{
		AZombieCharacter* NewZombie = World->SpawnActor<AZombieCharacter>(
			ZombieClassSlot,
			InvisibleLocation,
			SpawnRotation,
			SpawnParams);

		if (!NewZombie)
		{
			continue;
		}

		Zombies.Add(NewZombie);
		NewZombie->OnMoveCompleted.AddDynamic(this, &AZombieManager::OnZombieArrived);
		NewZombie->SetActorHiddenInGame(true);
		NewZombie->SetActorEnableCollision(false);
		CurrentSpawnedZombieCount++;
	}
}

void AZombieManager::SpawnZombie()
{
	if (!Player || Zombies.Num() == 0)
	{
		return;
	}

	AZombieCharacter* TargetZombie = nullptr;
	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		AZombieCharacter* Zombie = ZombiePtr.Get();
		if (Zombie && Zombie->IsHidden())
		{
			TargetZombie = Zombie;
			break;
		}
	}

	if (!TargetZombie)
	{
		return;
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	FVector SpawnLocation = FVector::ZeroVector;
	bool bSpatialQuerySuccess = false;

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		for (int32 Try = 0; Try < SpawnMaxAttempts; ++Try)
		{
			const float RandomAngleRadians = FMath::FRandRange(0.0f, 2.0f * PI);
			const float RandomDistance = FMath::FRandRange(SpawnMinDistance, SpawnMaxDistance);
			const FVector2D RandomDirection(FMath::Cos(RandomAngleRadians), FMath::Sin(RandomAngleRadians));
			const FVector DesiredLocation = PlayerLocation + FVector(RandomDirection.X * RandomDistance, RandomDirection.Y * RandomDistance, 0.0f);

			FNavLocation ProjectedLocation;
			if (!NavSys->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, FVector(500.0f, 500.0f, 300.0f)))
			{
				continue;
			}

			FHitResult Hit;
			const FVector TraceStart = ProjectedLocation.Location + FVector(0.0f, 0.0f, 1000.0f);
			const FVector TraceEnd = ProjectedLocation.Location - FVector(0.0f, 0.0f, 50000.0f);

			FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieSpawnTrace), true);
			Params.AddIgnoredActor(TargetZombie);

			const bool bHit = GetWorld()->LineTraceSingleByChannel(
				Hit,
				TraceStart,
				TraceEnd,
				ECC_WorldStatic,
				Params
			);

			if (!bHit)
			{
				continue;
			}

			SpawnLocation = Hit.Location;

			if (!IsSpawnLocationValid(SpawnLocation, TargetZombie))
			{
				continue;
			}

			bSpatialQuerySuccess = true;
			break;
		}
	}

	if (!bSpatialQuerySuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Zombie spawn validation failed after %d attempts."), SpawnMaxAttempts);
		return;
	}

	if (UCapsuleComponent* Capsule = TargetZombie->GetCapsuleComponent())
	{
		SpawnLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
	}

	TargetZombie->SetActorLocationAndRotation(SpawnLocation, FRotator::ZeroRotator);
	TargetZombie->SetActorHiddenInGame(false);
	TargetZombie->SetActorEnableCollision(true);

	if (UCharacterMovementComponent* MoveComp = TargetZombie->GetCharacterMovement())
	{
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Walking);
		MoveComp->SetComponentTickEnabled(true);
	}

	TargetZombie->ZombieActivateSet();
	TargetZombie->LaunchTowardActor(Player);

	if (AZombieAIController* AICon = Cast<AZombieAIController>(TargetZombie->GetController()))
	{
		AICon->SetAIMode(EZombieAIMode::Simple);
		TargetZombie->SimpleMove(PlayerLocation);
	}
}

void AZombieManager::DeSpawnZombie(AZombieCharacter* Zombie)
{
	if (!Zombie)
	{
		return;
	}

	Zombie->SetActorHiddenInGame(true);
	Zombie->SetActorEnableCollision(false);
	Zombie->SetActorLocation(FVector(0.0f, 0.0f, -10000.0f));
}

void AZombieManager::OnZombieArrived(AZombieCharacter* ArrivedZombie)
{
	if (!ArrivedZombie || !Player)
	{
		return;
	}

	const float Distance = FVector::Dist(ArrivedZombie->GetActorLocation(), Player->GetActorLocation());

	if (Distance <= ArrivedZombie->QualityEnterDistance)
	{
		if (AZombieAIController* AICon = Cast<AZombieAIController>(ArrivedZombie->GetController()))
		{
			AICon->SetAIMode(EZombieAIMode::Quality);
		}

		ArrivedZombie->QualityMove(Player);
		return;
	}

	ArrivedZombie->SimpleMove(Player->GetActorLocation());
}

bool AZombieManager::IsSpawnLocationValid(const FVector& CandidateLocation, AZombieCharacter* TargetZombie) const
{
	if (!Player)
	{
		return false;
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	const FVector ToSpawn = CandidateLocation - PlayerLocation;
	const float DistanceToPlayer = ToSpawn.Size2D();
	if (DistanceToPlayer < SpawnMinDistance || DistanceToPlayer > SpawnMaxDistance)
	{
		return false;
	}

	return IsSpawnLocationReachableToPlayer(CandidateLocation);
}

bool AZombieManager::IsSpawnLocationReachableToPlayer(const FVector& CandidateLocation) const
{
	if (!Player || !GetWorld())
	{
		return false;
	}

	const UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(
		GetWorld(),
		CandidateLocation,
		Player->GetActorLocation(),
		Player
	);

	return NavPath && NavPath->IsValid() && !NavPath->IsPartial();
}
