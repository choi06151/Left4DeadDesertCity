// Fill out your copyright notice in the Description page of Project Settings.

#include "ZombieManager.h"
#include "ZombieAIController.h"
#include "ZombieCharacter.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	const FVector InvisibleLocation(0.f, 0.f, -10000.f);
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
    if (!Player || Zombies.Num() == 0) return;

    // 1. 풀에서 현재 쉬고 있는(Hidden) 좀비 찾기
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

    if (!TargetZombie) return;

    const FVector PlayerLocation = Player->GetActorLocation();
    FVector SpawnLocation = FVector::ZeroVector; // 초기값을 제로로 설정하여 성공 여부 판별
    bool bSpatialQuerySuccess = false;

    if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
	    // 2D 평면상에서 15m ~ 25m 사이의 균일한 랜덤 좌표 오프셋 계산 (VRand 오차 수정)
    	const float RandomRange = FMath::FRandRange(3500.f, 5500.f);
    	const FVector2D RandomCirclePoint = FMath::RandPointInCircle(RandomRange);
    	const FVector RandomOffset = FVector(RandomCirclePoint.X, RandomCirclePoint.Y, 0.f);

    	const FVector DesiredLocation = PlayerLocation + RandomOffset;
    	FNavLocation ProjectedLocation;

    	for (int32 Try = 0; Try < 10; ++Try)
    	{
    		// 수직 오차 검사 반경을 500에서 300 정도로 좁혀서 위아래 층간 간섭 최소화
    		if (NavSys->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, FVector(500.f, 500.f, 300.f)))
    		{
    			SpawnLocation = ProjectedLocation.Location;
    			bSpatialQuerySuccess = true;

    			// 위에서 아래로 바닥 검증
    			FHitResult Hit;

    			const FVector TraceStart = SpawnLocation + FVector(0.f, 0.f, 0.f);
    			const FVector TraceEnd   = SpawnLocation - FVector(0.f, 0.f, 50000.f);

    			FCollisionQueryParams Params;
    			Params.bTraceComplex = true;      // 메시의 실제 표면 기준
    			Params.AddIgnoredActor(TargetZombie);

    			bool bHit = GetWorld()->LineTraceSingleByChannel(
					Hit,
					TraceStart,
					TraceEnd,
					ECC_WorldStatic,
					Params
				);

    			

    			// 실제 바닥 위치로 보정
    			SpawnLocation = Hit.Location;

    			if (!bHit)
    			{
    				UE_LOG(LogTemp, Warning, TEXT("스폰 위치 검증 실패 - 바닥을 찾지 못함"));
    			
    			}
			    else
			    {
				    break;
			    }
    		}
    	}
    
    	
    }

    // 내비메시 검색에 완전히 실패했다면, 플레이어와 겹치게 강제 소환하는 대신 이번 루프는 안전하게 스킵
    if (!bSpatialQuerySuccess)
    {
       UE_LOG(LogTemp, Warning, TEXT("좀비 스폰 위치의 NavMesh 투영에 실패했습니다."));
       return;
    }

    // 발바닥 정렬을 위한 캡슐 높이 보정
    if (UCapsuleComponent* Capsule = TargetZombie->GetCapsuleComponent())
    {
       SpawnLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
    }  
    
    // 위치 설정 및 액터 활성화
    TargetZombie->SetActorLocationAndRotation(SpawnLocation, FRotator::ZeroRotator);
    TargetZombie->SetActorHiddenInGame(false);
    TargetZombie->SetActorEnableCollision(true);
    
    // 캐릭터 무브먼트 초기화 (공중 부양 및 속도 꼬임 방지)
    if (UCharacterMovementComponent* MoveComp = TargetZombie->GetCharacterMovement())
    {
       MoveComp->Velocity = FVector::ZeroVector;
       MoveComp->SetMovementMode(MOVE_Walking);
    }
	
	TargetZombie->ZombieActivateSet();

    // AI 기동 및 돌격 명령
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
	Zombie->SetActorLocation(FVector(0.f, 0.f, -10000.f));
}

void AZombieManager::OnZombieArrived(AZombieCharacter* ArrivedZombie)
{
	if (!ArrivedZombie || !Player)
	{
		return;
	}

	const float Distance = FVector::Dist(ArrivedZombie->GetActorLocation(), Player->GetActorLocation());

	if (Distance <= 1000.f)
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
