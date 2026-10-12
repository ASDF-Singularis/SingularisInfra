#include "Components/SingularisSeatComponent.h"

#include <Components/PrimitiveComponent.h>
#include <Components/SceneComponent.h>
#include <GameFramework/Character.h>
#include <GameFramework/CharacterMovementComponent.h>
#include <Net/UnrealNetwork.h>

USingularisSeatComponent::USingularisSeatComponent()
{
	SetIsReplicatedByDefault(true);

	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	bAutoActivate = true;
}

void USingularisSeatComponent::BeginPlay()
{
	Super::BeginPlay();

	// 1) 缓存挂载点与退出点
	if (AActor* Owner = GetOwner())
	{
		CachedMountPoint = Cast<USceneComponent>(MountPoint.GetComponent(Owner));
		CachedExitPoint = Cast<USceneComponent>(ExitPoint.GetComponent(Owner));
	}
}

void USingularisSeatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USingularisSeatComponent, Occupant);
}

void USingularisSeatComponent::Sit(AActor* Actor)
{
	if (Occupied() || !IsValid(Actor)) return;

	SetOccupant(Actor);
}

void USingularisSeatComponent::Stand()
{
	if (!Occupied()) return;

	SetOccupant(nullptr);
}

void USingularisSeatComponent::OnRep_Occupant(const TWeakObjectPtr<AActor> OldOccupant) const
{
	ApplyOccupant(OldOccupant.Get());
}

void USingularisSeatComponent::SetOccupant(AActor* Actor)
{
	// 1) 服务器权威检查
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority()) return;

	// 2) 幂等性检查，若状态未变更则直接返回
	if (Actor == Occupant) return;

	// 3) 捕获旧状态后写入新状态
	AActor* OldOccupant = Occupant.Get();
	Occupant = Actor;

	// 4) 响应式编程：服务器应用副作用
	ApplyOccupant(OldOccupant);
}

void USingularisSeatComponent::ApplyOccupant(AActor* OldOccupant) const
{
	if (Occupant.IsValid())
	{
		// 1) 获取根组件
		USceneComponent* Root = Occupant->GetRootComponent();
		if (!IsValid(Root)) return;

		// 2) 挂起角色移动：停止速度、清空移动模式并停用 Tick。
		//    CMC 拥有根组件变换且强制胶囊直立（俯仰/翻滚归零），只要它仍在 Tick，
		//    就会与附件关系逐帧对抗并被客户端预测/位置修正打断，表现为模型脱离载具与抽搐
		if (const ACharacter* Character = Cast<ACharacter>(Occupant.Get()))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
				Movement->DisableMovement();
				Movement->bIgnoreClientMovementErrorChecksAndCorrection = true;
				Movement->SetComponentTickEnabled(false);
			}
		}

		// 3) 完全关闭碰撞：QueryOnly 仍参与查询，会与载具互相阻断并诱发抖动与穿模
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Root))
			PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// 4) 附加到载具本体（挂载点的父级）并沿用其插槽，挂载点仅作为定位标记：
		//    乘员相对插槽的变换与挂载点完全一致，从而同步其全部旋转（含俯仰/翻滚），缩放保持自身值；
		//    不焊接物理体，避免载具翻滚时与 Chaos 求解器互相拉扯
		USceneComponent* AttachParent = CachedMountPoint.IsValid()
			                                ? CachedMountPoint->GetAttachParent()
			                                : IsValid(GetOwner())
			                                ? GetOwner()->GetRootComponent()
			                                : nullptr;

		if (IsValid(AttachParent))
		{
			const FName AttachSocket = CachedMountPoint.IsValid() ? CachedMountPoint->GetAttachSocketName() : NAME_None;
			Occupant->AttachToComponent(
				AttachParent,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				AttachSocket
			);

			// 对齐到标记：挂载点与乘员相对同一插槽，二者相对变换一致即世界位姿一致
			if (CachedMountPoint.IsValid())
				Root->SetRelativeLocationAndRotation(
					CachedMountPoint->GetRelativeLocation(),
					CachedMountPoint->GetRelativeRotation()
				);
		}

		// 5) 广播入座事件
		OnSeatOccupied.Broadcast(Occupant.Get());
	}
	else
	{
		if (!IsValid(OldOccupant)) return;

		// 1) 获取根组件
		USceneComponent* Root = OldOccupant->GetRootComponent();
		if (!IsValid(Root)) return;

		// 2) 分离，保留世界变换
		OldOccupant->DetachFromActor(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));

		// 3) 解析落点：优先使用退出点，未配置退出点时原地落位
		FVector ExitLocation = OldOccupant->GetActorLocation();
		FRotator ExitRotation = OldOccupant->GetActorRotation();

		if (CachedExitPoint.IsValid())
		{
			ExitLocation = CachedExitPoint->GetComponentLocation();
			ExitRotation = CachedExitPoint->GetComponentRotation();
		}

		// 4) 直立落位：胶囊无法表达俯仰/翻滚，仅保留偏航，避免倾斜落座导致寻地失败
		OldOccupant->SetActorLocationAndRotation(
			ExitLocation,
			FRotator(0.0, ExitRotation.Yaw, 0.0),
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		// 5) 恢复碰撞
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Root))
			PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// 6) 恢复角色移动：先唤醒再置回行走模式，并清空附加期间残留的加速度与速度
		if (const ACharacter* Character = Cast<ACharacter>(OldOccupant))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->SetComponentTickEnabled(true);
				Movement->bIgnoreClientMovementErrorChecksAndCorrection = false;
				Movement->SetMovementMode(MOVE_Walking);
				Movement->StopMovementImmediately();
			}
		}

		// 7) 广播离座事件
		OnSeatVacated.Broadcast(OldOccupant);
	}
}
