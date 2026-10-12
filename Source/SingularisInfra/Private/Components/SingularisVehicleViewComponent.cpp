#include "Components/SingularisVehicleViewComponent.h"

#include <GameFramework/Pawn.h>
#include <GameFramework/PlayerController.h>
#include <GameFramework/SpringArmComponent.h>

USingularisVehicleViewComponent::USingularisVehicleViewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	bAutoActivate = true;
}

void USingularisVehicleViewComponent::BeginPlay()
{
	Super::BeginPlay();

	// 1) 缓存相机臂
	if (AActor* Owner = GetOwner())
		CachedBoom = Cast<USpringArmComponent>(CameraBoom.GetComponent(Owner));
}

void USingularisVehicleViewComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bVehicleRelative || !CachedBoom.IsValid()) return;

	// 1) 仅写入局部偏移：世界旋转由相机臂从乘员附加链（载具）继承而来
	CachedBoom->SetRelativeRotation(FRotator(LookPitch, LookYaw, 0.0));
}

void USingularisVehicleViewComponent::SetVehicleRelative(bool bEnable)
{
	// 1) 幂等性检查，若状态未变更则直接返回
	if (bVehicleRelative == bEnable) return;

	bVehicleRelative = bEnable;

	USpringArmComponent* Boom = CachedBoom.Get();
	if (!IsValid(Boom))
	{
		SetComponentTickEnabled(bEnable);
		OnViewModeChanged.Broadcast(bEnable);
		return;
	}

	if (bEnable)
	{
		// 2) 快照并替换相机臂旋转驱动方式：解除世界控制旋转，改为继承载具全轴旋转
		bSavedUsePawnControlRotation = Boom->bUsePawnControlRotation;
		bSavedInheritPitch = Boom->bInheritPitch;
		bSavedInheritYaw = Boom->bInheritYaw;
		bSavedInheritRoll = Boom->bInheritRoll;

		Boom->bUsePawnControlRotation = false;
		Boom->bInheritPitch = true;
		Boom->bInheritYaw = true;
		Boom->bInheritRoll = true;

		// 3) 以当前局部旋转初始化偏移，避免切换瞬间视角跳变
		const FRotator Current = Boom->GetRelativeRotation();
		LookPitch = FMath::Clamp(Current.Pitch, PitchMin, PitchMax);
		LookYaw = Current.Yaw;
	}
	else
	{
		// 2) 复位局部偏移并还原相机臂原有设置
		Boom->SetRelativeRotation(FRotator::ZeroRotator);
		Boom->bUsePawnControlRotation = bSavedUsePawnControlRotation;
		Boom->bInheritPitch = bSavedInheritPitch;
		Boom->bInheritYaw = bSavedInheritYaw;
		Boom->bInheritRoll = bSavedInheritRoll;

		LookPitch = 0.0f;
		LookYaw = 0.0f;
	}

	// 4) 仅在载具相对模式下运行 Tick
	SetComponentTickEnabled(bEnable);

	// 5) 广播模式切换
	OnViewModeChanged.Broadcast(bEnable);
}

void USingularisVehicleViewComponent::AddLookYaw(float Value)
{
	if (bVehicleRelative)
	{
		LookYaw += Value * YawSensitivity;
		return;
	}

	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
			Controller->AddYawInput(Value);
	}
}

void USingularisVehicleViewComponent::AddLookPitch(float Value)
{
	if (bVehicleRelative)
	{
		LookPitch = FMath::Clamp(LookPitch + Value * PitchSensitivity, PitchMin, PitchMax);
		return;
	}

	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
			Controller->AddPitchInput(Value);
	}
}
