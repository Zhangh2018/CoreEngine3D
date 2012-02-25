//
//  Box2DContactListener.cpp
//  Game(iOS)
//
//  Created by Jody McAdams on 2/19/12.
//  Copyright (c) 2012 Jody McAdams. All rights reserved.
//

#include "Box2DContactListener.h"

#include "CoreGameObject.h"

void Box2DContactListener::BeginContact(b2Contact* contact)
{
	CoreGameObject* pObjectA = (CoreGameObject*)contact->GetFixtureA()->GetBody()->GetUserData();
	
	CoreGameObject* pObjectB = (CoreGameObject*)contact->GetFixtureB()->GetBody()->GetUserData();

	Box2D_CollisionInfo collInfo;
	
	if(pObjectA != NULL)
	{
		collInfo.pCollider = pObjectB;
		pObjectA->CollisionResponseCallback(collInfo);
	}

	
	if(pObjectB != NULL)
	{
		collInfo.pCollider = pObjectA;
		pObjectB->CollisionResponseCallback(collInfo);
	}

}