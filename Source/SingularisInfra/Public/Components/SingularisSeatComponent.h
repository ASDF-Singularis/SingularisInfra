#pragma once

#include <CoreMinimal.h>
#include <Components/ActorComponent.h>

#include "SingularisSeatComponent.generated.h"

class USceneComponent;

#pragma region 委托签名

/** 入座时广播，携带入座的 Actor 引用 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSeatOccupiedSignature, AActor*, Occupant);

/** 离座时广播，携带离座的旧 Actor 引用 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSeatVacatedSignature, AActor*, OldOccupant);

#pragma endregion

/**
 * 引力奇点座位组件。
 *
 * Server Authority State Synchronization and Reactive Side Effects 设计模式典范：
 * 服务器权威维护 Occupant 复制状态，入座与离座的附加副作用统一经 ApplyOccupant 响应式执行。
 */
UCLASS(
	Blueprintable,
	BlueprintType,
	ClassGroup = ("Singularis"),
	meta = (BlueprintSpawnableComponent, DisplayName = "引力奇点座位组件")
)
class SINGULARISINFRA_API USingularisSeatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region Parameter

	/**
	 * 挂载点组件引用。
	 *
	 * 指向 Owner 上某个 ArrowComponent，用于确定乘员的附加父级与插槽；
	 * 未解析到时回退至 Owner 根组件。
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "引力奇点座位组件",
		meta = (DisplayName = "挂载点", UseComponentPicker, AllowedClasses = "/Script/Engine.ArrowComponent")
	)
	FComponentReference MountPoint{};

#pragma endregion

#pragma region Event Dispatcher

	/** 入座时广播 */
	UPROPERTY(
		BlueprintAssignable,
		Category = "引力奇点座位组件|事件分发器",
		meta = (DisplayName = "入座时")
	)
	FOnSeatOccupiedSignature OnSeatOccupied{};

	/** 离座时广播 */
	UPROPERTY(
		BlueprintAssignable,
		Category = "引力奇点座位组件|事件分发器",
		meta = (DisplayName = "离座时")
	)
	FOnSeatVacatedSignature OnSeatVacated{};

#pragma endregion

private:
#pragma region State

	/** 缓存的挂载点组件，由 MountPoint 解析填充 */
	TWeakObjectPtr<USceneComponent> CachedMountPoint = nullptr;

	/** 当前占用座位的 Actor（复制） */
	UPROPERTY(ReplicatedUsing = OnRep_Occupant)
	TWeakObjectPtr<AActor> Occupant = nullptr;

#pragma endregion

public:
#pragma region Constructors

	USingularisSeatComponent();

#pragma endregion

#pragma region ActorComponent Interface

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region API

	/** 座位是否被占用 */
	UFUNCTION(
		BlueprintPure,
		Category = "引力奇点座位组件|API",
		meta = (DisplayName = "Occupied")
	)
	bool Occupied() const { return Occupant.IsValid(); }

	/** 获取当前占用座位的 Actor */
	UFUNCTION(
		BlueprintPure,
		Category = "引力奇点座位组件|API",
		meta = (DisplayName = "GetOccupant")
	)
	AActor* GetOccupant() const { return Occupant.Get(); }

	/**
	 * 让指定 Actor 入座。
	 *
	 * 仅服务器权威有效；座位已被占用或入参非法时静默忽略。
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "引力奇点座位组件|API",
		meta = (DisplayName = "入座")
	)
	void Sit(AActor* Actor);

	/** 让当前乘员离座 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "引力奇点座位组件|API",
		meta = (DisplayName = "离座")
	)
	void Stand();

#pragma endregion

private:
#pragma region Response

	/** Occupant 复制回调 */
	UFUNCTION()
	void OnRep_Occupant(TWeakObjectPtr<AActor> OldOccupant) const;

#pragma endregion

#pragma region Internal Function

	/** 设置 Occupant 复制状态并应用副作用 */
	void SetOccupant(AActor* Actor);

	/** 将占用状态变化应用到附加与移动表现 */
	void ApplyOccupant(AActor* OldOccupant) const;

#pragma endregion
};
