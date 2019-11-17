# UnrealFootIK
FootIK
This needs to be also setup in blueprints for animations. 

In this case the IK methods know when the player has reached near zero velocity. Functions for IK are triggered with a  
DeltaTime check every frame with a very simple if check to reduce CPU usage. 

In the case that the character has reached near zero velocity the functions for both feet and joints are called.
Some reletively complicated Differential Calculus and 2d geometry is implemented to find the correct angle that the feet should
rest at relative to the angle of the floor.

This is combined with state machine animations however you want. I am not going to include the blueprint for this because the
code is the tricky part the animations are pretty easy to implement with a standard state machine. 


Here is a sample of most of what the logic is based on:


	FVector SocketLocation = GetMesh()->GetSocketLocation(SocketName);
	FVector ActorLocation = GetActorLocation();

	FVector Start = FVector(SocketLocation.X, SocketLocation.Y, ActorLocation.Z);
	FVector End = FVector(SocketLocation.X, SocketLocation.Y, ActorLocation.Z - CapsuleHalfHeight - TraceDistance);
	
	FHitResult Hit;
	FCollisionQueryParams CollisionParams;
	CollisionParams.bTraceComplex = true;
	CollisionParams.AddIgnoredActor(this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, CollisionParams))
	{
		OutOffset = (Hit.Location - Hit.TraceEnd).Size() - TraceDistance + AdjustOffset;
		FRotator NewRotation = FRotator(-FMath::RadiansToDegrees(FMath::Atan2(Hit.Normal.X, Hit.Normal.Z)), 0.0f, 			FMath::RadiansToDegrees(FMath::Atan2(Hit.Normal.Y, Hit.Normal.Z)));
		OutRotation = FMath::RInterpTo(OutRotation, NewRotation, DeltaTime, FootInterpSpeed);
	}
	else
	{
		OutOffset = 0;
	}
 
