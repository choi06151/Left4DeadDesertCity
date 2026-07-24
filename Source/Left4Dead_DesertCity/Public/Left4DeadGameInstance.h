#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Left4DeadGameInstance.generated.h"

UCLASS()
class LEFT4DEAD_DESERTCITY_API ULeft4DeadGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void OnStart() override;

private:
	void ApplyPackagedWindowDefaults();
};
