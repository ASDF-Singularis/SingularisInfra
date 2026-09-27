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

	if (AActor* Owner = GetOwner())
		CachedMountPoint = Cast<USceneComponent>(MountPoint.GetComponent(Owner));
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

	// 2) 幂等性
	if (Actor == Occupant) return;

	// 3) 设置复制副作用状态
	AActor* OldOccupant = Occupant.Get();
	Occupant = Actor;

	// 4) 服务器应用副作用
	//    响应式编程
	ApplyOccupant(OldOccupant);
}

void USingularisSeatComponent::ApplyOccupant(AActor* OldOccupant) const
{
	if (Occupant.IsValid())
	{
		// 1) 获取根组件
		USceneComponent* Root = Occupant->GetRootComponent();
		if (!IsValid(Root)) return;

		// 2) 关闭碰撞，避免乘员与载具互相推挤
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Root))
			PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// 3) 冻结角色移动
		if (const ACharacter* Character = Cast<ACharacter>(Occupant.Get()))
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
				Movement->DisableMovement();

		// 4) 附加 - 根据挂载点确定附加父级和插槽
		const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);

		if (CachedMountPoint.IsValid())
		{
			// 有骨骼：挂载点挂在 SkeletalMeshComponent 的某个 Socket/Bone 上 → 乘员附加到同一骨骼
			// 无骨骼：挂载点挂在普通 SceneComponent 上 → 乘员附加到同一父级
			USceneComponent* AttachParent = CachedMountPoint->GetAttachParent();
			const FName AttachSocket = CachedMountPoint->GetAttachSocketName();

			if (IsValid(AttachParent))
			{
				Occupant->AttachToComponent(AttachParent, AttachmentRules, AttachSocket);
				// 附加后匹配挂载点相对父级的变换，使乘员精确位于挂载点标记的位置
				Root->SetRelativeTransform(CachedMountPoint->GetRelativeTransform());
			}
		}
		else if (IsValid(GetOwner()) && IsValid(GetOwner()->GetRootComponent()))
		{
			// 无挂载点时回退到附加至 Owner 根组件
			Occupant->AttachToComponent(GetOwner()->GetRootComponent(), AttachmentRules);
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

		// 2) 分离
		const FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
		OldOccupant->DetachFromActor(DetachRules);

		// 3) 恢复碰撞
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Root))
			PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// 4) 恢复角色移动
		if (const ACharacter* Character = Cast<ACharacter>(OldOccupant))
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
				Movement->SetMovementMode(MOVE_Walking);

		// 5) 广播离座事件
		OnSeatVacated.Broadcast(OldOccupant);
	}
}
