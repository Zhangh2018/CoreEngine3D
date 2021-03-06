//
//  ObjectGroup.h
//  Game(iOS)
//
//  Created by Jody McAdams on 12/27/11.
//  Copyright (c) 2011 Jody McAdams. All rights reserved.
//

#ifndef __ObjectGroup_h
#define __ObjectGroup_h

#include "../MathTypes.h"
#include "../GraphicsTypes.h"

#include "../CoreGameObject.h"
#include "../Game.h"
#include "CollisionBox.h"

#define ObjectGroup_MAX_OBJECTS 16

class ObjectGroup: public CoreGameObject
{
public:
	virtual void ProcessMessage(u32 message);	//Pass in a hash value

	virtual bool SpawnInit(void* pSpawnStruct);
	virtual void SetPosition(const vec3* pPosition);
	
	bool ObjectsAreContained(CollisionBox* pContainer);
	
private:	

	CoreObjectHandle m_objectHandles[ObjectGroup_MAX_OBJECTS];
	u32 m_numObjects;
};

void ObjectGroup_Init();

#endif
