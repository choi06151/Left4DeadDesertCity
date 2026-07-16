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
	SetZombieWaveIndex(CurrentWaveIndex);
	SchedulePoolWarmup();
}

void AZombieManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PoolWarmupTimerHandle);
	Super::EndPlay(EndPlayReason);
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

	const int32 InitialPoolCount = FMath::Min(InitialPooledZombieCount, MaxPooledZombieCount);
	for (int32 i = 0; i < InitialPoolCount; i++)
	{
		if (!TrySpawnPooledZombieHidden())
		{
			break;
		}
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
		// DisableZombieActor clears every response for pooling, so restore the
		// active collision profile before showing or launching the zombie again.
		Capsule->SetCollisionProfileName(TEXT("Pawn"));
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionObjectType(ECC_GameTraceChannel3);
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
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
	CurrentSpawnedZombieCount = GetActiveZombieCount();

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
	CurrentSpawnedZombieCount = GetActiveZombieCount();
	MaintainZombiePopulation();
}

void AZombieManager::SetZombieWaveIndex(int32 WaveIndex)
{
	CurrentWaveIndex = FMath::Clamp(WaveIndex, 0, 6);
	DesiredActiveZombieCount = ResolveDesiredActiveZombieCount(CurrentWaveIndex);
	MaintainZombiePopulation();
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

FVector AZombieManager::GetSurroundSlotLocation(
	const AZombieCharacter* Zombie,
	const FVector& TargetCenter) const
{
	if (!Zombie)
	{
		return TargetCenter;
	}

	int32 ZombieIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Zombies.Num(); ++Index)
	{
		if (Zombies[Index].Get() == Zombie)
		{
			ZombieIndex = Index;
			break;
		}
	}

	if (ZombieIndex == INDEX_NONE)
	{
		return TargetCenter;
	}

	const int32 SlotsPerRing = FMath::Max(1, SurroundSlotsPerRing);
	const int32 RingIndex = ZombieIndex / SlotsPerRing;
	const float Radius = FMath::Max(0.0f, SurroundBaseRadius + RingIndex * SurroundRingSpacing);

	// Golden-angle distribution keeps even the first few active pool entries
	// spread around the full circle while preserving a stable slot per zombie.
	constexpr float GoldenAngleRadians = PI * (3.0f - 2.2360679775f);
	const float Angle = ZombieIndex * GoldenAngleRadians;
	const FVector DesiredLocation = TargetCenter + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Radius;

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation ProjectedLocation;
		if (NavSys->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, SurroundNavProjectionExtent))
		{
			return ProjectedLocation.Location;
		}
	}

	return DesiredLocation;
}

void AZombieManager::MaintainZombiePopulation()
{
	const int32 TargetActiveZombieCount = FMath::Min(DesiredActiveZombieCount, Zombies.Num());
	CurrentSpawnedZombieCount = GetActiveZombieCount();

	while (CurrentSpawnedZombieCount < TargetActiveZombieCount)
	{
		const int32 ActiveCountBeforeSpawn = CurrentSpawnedZombieCount;
		SpawnZombie();

		if (CurrentSpawnedZombieCount <= ActiveCountBeforeSpawn)
		{
			break;
		}
	}
}

int32 AZombieManager::GetActiveZombieCount() const
{
	int32 ActiveZombieCount = 0;

	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		const AZombieCharacter* Zombie = ZombiePtr.Get();
		if (Zombie && !Zombie->IsHidden())
		{
			++ActiveZombieCount;
		}
	}

	return ActiveZombieCount;
}

int32 AZombieManager::ResolveDesiredActiveZombieCount(int32 WaveIndex) const
{
	switch (WaveIndex)
	{
	case 0:
		return 0;
	case 1:
		return 5;
	default:
		return 15 + ((WaveIndex - 2) * 5);
	}
}

void AZombieManager::SchedulePoolWarmup()
{
	if (Zombies.Num() >= MaxPooledZombieCount || PoolWarmupInterval <= 0.0f)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		PoolWarmupTimerHandle,
		this,
		&AZombieManager::WarmupZombiePool,
		PoolWarmupInterval,
		true);
}

void AZombieManager::WarmupZombiePool()
{
	if (Zombies.Num() >= MaxPooledZombieCount)
	{
		GetWorldTimerManager().ClearTimer(PoolWarmupTimerHandle);
		return;
	}

	const int32 BatchCount = FMath::Max(1, PoolWarmupBatchSize);
	for (int32 Index = 0; Index < BatchCount && Zombies.Num() < MaxPooledZombieCount; ++Index)
	{
		if (!TrySpawnPooledZombieHidden())
		{
			GetWorldTimerManager().ClearTimer(PoolWarmupTimerHandle);
			return;
		}
	}

	MaintainZombiePopulation();
}

bool AZombieManager::TrySpawnPooledZombieHidden()
{
	if (!ZombieClassSlot)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector InvisibleLocation(0.0f, 0.0f, -10000.0f);
	const FRotator SpawnRotation = FRotator::ZeroRotator;

	AZombieCharacter* NewZombie = World->SpawnActor<AZombieCharacter>(
		ZombieClassSlot,
		InvisibleLocation,
		SpawnRotation,
		SpawnParams);

	if (!NewZombie)
	{
		return false;
	}

	Zombies.Add(NewZombie);
	NewZombie->OnMoveCompleted.AddDynamic(this, &AZombieManager::OnZombieArrived);
	NewZombie->SetActorHiddenInGame(true);
	NewZombie->SetActorEnableCollision(false);
	return true;
}
