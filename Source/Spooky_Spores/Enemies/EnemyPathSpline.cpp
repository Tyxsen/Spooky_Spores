#include "Enemies/EnemyPathSpline.h"
#include "Components/SplineComponent.h"

// Sets default values
AEnemyPathSpline::AEnemyPathSpline()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	PathSplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("PathSplineComponent"));
	SetRootComponent(PathSplineComponent);
}
