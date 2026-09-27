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
	 * 未解析到时回退至 Owner 根组件。乘员仅继承挂载点的位置与偏航——
	 * 胶囊不支持俯仰/翻滚，挂载点自身的旋转与缩放不写入乘员根组件。
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "引力奇点座位组件",
		meta = (DisplayName = "挂载点", UseComponentPicker, AllowedClasses = "/Script/Engine.ArrowComponent")
	)
	FComponentReference MountPoint{};

	/**
	 * 退出点组件引用。
	 *
	 * 指向 Owner 上某个 ArrowComponent，箭头位置即离座时乘员的根组件（胶囊中心）落点，
	 * 箭头偏航决定乘员朝向；未解析到时乘员原地落位。
	 * 落点需位于载具碰撞体之外且与站立时胶囊中心等高，否则乘员会在碰撞体内反复寻地失败。
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "引力奇点座位组件",
		meta = (DisplayName = "退出点", UseComponentPicker, AllowedClasses = "/Script/Engine.ArrowComponent")
	)
	FComponentReference ExitPoint{};

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

	/** 缓存的退出点组件，由 ExitPoint 解析填充 */
	TWeakObjectPtr<USceneComponent> CachedExitPoint = nullptr;

	/** 当前占用座位的 Actor（复制） */
	UPROPERTY(ReplicatedUsing = OnRep_Occupant)
	TWeakObjectPtr<AActor> Occupant = nullptr;

#pragma endregion

public:
#pragma region Constructors

	/** 默认构造函数，启用组件复制并保持 Tick 关闭（纯事件驱动） */
	USingularisSeatComponent();

#pragma endregion

#pragma region ActorComponent Interface

	/** 解析 MountPoint 组件引用并缓存挂载点 */
	virtual void BeginPlay() override;

	/** 注册 Occupant 为复制属性 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region API

	/**
	 * 座位是否被占用。
	 *
	 * @return 存在有效乘员时返回 true。
	 */
	UFUNCTION(
		BlueprintPure,
		Category = "引力奇点座位组件|API",
		meta = (DisplayName = "Occupied")
	)
	bool Occupied() const { return Occupant.IsValid(); }

	/**
	 * 获取当前占用座位的 Actor。
	 *
	 * @return 当前乘员，无人入座时返回 nullptr。
	 */
	UFUNCTION(
		BlueprintPure,
		Category = "引力奇点座位组件|API",
		meta = (DisplayName = "GetOccupant")
	)
	AActor* GetOccupant() const { return Occupant.Get(); }

	/**
	 * 让指定 Actor 入座。
	 *
	 * 服务器权威操作。写入 Occupant 复制属性，由 ApplyOccupant 响应式应用附加与移动副作用，
	 * 座位已被占用或入参非法时静默忽略。
	 *
	 * @param Actor 要入座的 Actor。
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "引力奇点座位组件|API",
		meta = (DisplayName = "入座")
	)
	void Sit(AActor* Actor);

	/**
	 * 让当前乘员离座。
	 *
	 * 服务器权威操作。清空 Occupant 复制属性，由 ApplyOccupant 响应式分离并把乘员直立落位到退出点，
	 * 座位空闲时静默忽略。
	 */
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

	/**
	 * Occupant 复制回调。
	 *
	 * 客户端收到复制值时调用 ApplyOccupant 应用附加与移动副作用。
	 *
	 * @param OldOccupant 复制前的旧乘员。
	 */
	UFUNCTION()
	void OnRep_Occupant(TWeakObjectPtr<AActor> OldOccupant) const;

#pragma endregion

#pragma region Internal Function

	/**
	 * 设置 Occupant 复制属性并应用副作用。
	 *
	 * 服务器权威，幂等。先快照旧值，再赋值新值，最后调用 ApplyOccupant。
	 *
	 * @param Actor 新的乘员，传入 nullptr 表示离座。
	 */
	void SetOccupant(AActor* Actor);

	/**
	 * 应用 Occupant 变化到附加与移动表现。
	 *
	 * 入座分支：关闭碰撞 → 冻结角色移动 → 附加到挂载点 → 广播 OnSeatOccupied。
	 * 离座分支：分离 → 恢复碰撞 → 恢复移动 → 广播 OnSeatVacated。
	 *
	 * @param OldOccupant 变化前的旧乘员，用于离座分支的清理与广播。
	 */
	void ApplyOccupant(AActor* OldOccupant) const;

#pragma endregion
};
