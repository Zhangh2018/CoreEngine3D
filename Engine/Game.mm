//
//  Game.cpp
//  InfiniSurv(OSX)
//
//  Created by Jody McAdams on 11/27/11.
//  Copyright (c) 2011 Jody McAdams. All rights reserved.
//

#include "Game.h"
#include "MathUtil.h"
#include "matrix.h"

Game* GAME = NULL;

void Game::Init()
{
	m_numLoadedArtDescriptions = 0;
	m_numArtDescriptionsToLoadTexturesFor = 0;
	m_ui_numButtons = 0;
	m_numBreakables = 0;
	
#if defined (PLATFORM_IOS)
	m_pTouchInput = [[[TouchInputIOS alloc]init:&m_deviceInputState]retain];
#endif
	
	GAME = this;
}

void Game::Update(f32 timeElapsed)
{
	UpdateBreakables(timeElapsed);
}


CoreUI_Button* Game::AddUIButton(u32 width, u32 height, CoreUI_AttachSide attachSide, s32 offsetX, s32 offsetY, u32* textureHandle, s32 value, void (*callback)(s32))
{
	if(m_ui_numButtons == GAME_MAX_BUTTONS)
	{
		return NULL;
	}
	
	CoreUI_Button* pButton = &m_ui_buttons[m_ui_numButtons];
	pButton->Init(width, height, attachSide, offsetX, offsetY, textureHandle, value, callback);
	
	++m_ui_numButtons;
	
	return pButton;
}

void Game::ClearAllButtons()
{
	m_ui_numButtons = 0;
}


void Game::UpdateButtons(TouchState touchState, vec2 *pTouchPos)
{
	for(u32 i=0; i<m_ui_numButtons; ++i)
	{
		if(touchState != TouchState_None)
		{
			m_ui_buttons[i].PressButton(touchState, pTouchPos);
		}
	}
}


//Checks if the art will be loaded next time LoadItemArt gets called
bool Game::WillArtDescriptionBeLoaded(ItemArtDescription* pArtDesc)
{
    for(u32 i=0; i<m_numArtDescriptionsToLoadTexturesFor; ++i)
    {
        ItemArtDescription* pCurrArtDesc = m_pArtDescriptionsToLoadTexturesFor[i];
        if(pArtDesc == pCurrArtDesc)
        {
            return YES;
        }
    }
    
    return NO;
}


//Call many times to prepare art to be loaded later
void Game::AddItemArt(ItemArtDescription* pArtDescription)
{
    //Make sure this description is not already in the list
    for(u32 i=0; i<m_numArtDescriptionsToLoadTexturesFor; ++i)
    { 
        if(m_pArtDescriptionsToLoadTexturesFor[i] == pArtDescription)
        {
            return;
        }
    }
    
    //It wasn't in the list, so add it
    m_pArtDescriptionsToLoadTexturesFor[m_numArtDescriptionsToLoadTexturesFor] = pArtDescription;
    ++m_numArtDescriptionsToLoadTexturesFor;
}


//Call after all your art has been added to the list of things to load gunna die
//You can call this multiple times if you want and nothing bad will happen
void Game::LoadItemArt()
{
    //Dump old textures that don't need to be loaded
    for(u32 i=0; i<m_numLoadedArtDescriptions; ++i)
    {
        ItemArtDescription* pCurrArtDesc = m_pLoadedArtDescriptions[i];
        if(m_numArtDescriptionsToLoadTexturesFor == 0
		   || WillArtDescriptionBeLoaded(pCurrArtDesc) == false)
        {
            GLRENDERER->DeleteTexture(&pCurrArtDesc->textureHandle);
        }
    }
	
    //Reset the loaded description count to 0
    m_numLoadedArtDescriptions = 0;
	
    //Load unloaded level item textures
    //This list has only unique entries
    for(u32 i=0; i<m_numArtDescriptionsToLoadTexturesFor; ++i)
    {
		ItemArtDescription* pCurrArtDesc = m_pArtDescriptionsToLoadTexturesFor[i];
		const MaterialSettings* pMaterialSettings = pCurrArtDesc->materialSettings;
		
		//This will load the texture but if it's already loaded, it will do nothing
		GLRENDERER->LoadTexture(pCurrArtDesc->textureFileName, pCurrArtDesc->imageType, &pCurrArtDesc->textureHandle, pMaterialSettings->textureFilterMode, pMaterialSettings->wrapModeU, pMaterialSettings->wrapModeV,pMaterialSettings->flipY);
        
        //Store the descriptions we've loaded
		//Some of them were already loaded but now we can keep track!
        m_pLoadedArtDescriptions[m_numLoadedArtDescriptions] = pCurrArtDesc;
        ++m_numLoadedArtDescriptions;
    }
}


//Call this at init, then call AddArtDescriptionToLoad many times.
//The art you're not using anymore will get deleted when you call LoadItemArt
void Game::ClearItemArt()
{
	m_numArtDescriptionsToLoadTexturesFor = 0;
}


//Just deletes all the loaded art PERIOD
//You might never have to call this
void Game::DeleteAllItemArt()
{
	//Dump old textures that don't need to be loaded
    for(u32 i=0; i<m_numLoadedArtDescriptions; ++i)
    {
        ItemArtDescription* pCurrArtDesc = m_pLoadedArtDescriptions[i];
		GLRENDERER->DeleteTexture(&pCurrArtDesc->textureHandle);
    }
	
	m_numLoadedArtDescriptions = 0;
	m_numArtDescriptionsToLoadTexturesFor = 0;
}


void Game::UpdateBreakables(f32 timeElapsed)
{
	//Delete old breakables
	for(u32 i=0; i<m_numBreakables; )
    {
        Breakable* pCurrBreakable = &m_updatingBreakables[i];
        
        //Kill off old breakables
        if(pCurrBreakable->lifeTimer < 0.0f)
        {
			Breakable* pLastBreakable = &m_updatingBreakables[m_numBreakables-1];
			
			if(m_numBreakables > 1)
			{
				*pCurrBreakable = *pLastBreakable;
				
				RenderableObject3D* pRenderable = &pCurrBreakable->renderable;
				pRenderable->geom.pWorldMat = pRenderable->worldMat;
				pRenderable->geom.material.uniqueUniformValues[0] = (u8*)&pCurrBreakable->texcoordOffset;
				pRenderable->geom.material.uniqueUniformValues[1] = (u8*)&pCurrBreakable->diffuseColor;
			}
			
			GLRENDERER->RemoveRenderableGeometry3DFromList(&pLastBreakable->renderable.geom);
			
			//printf("  Deleted breakable...\n");
			
			--m_numBreakables;
        }
		else
		{
			++i;
		}
	}
	
	//Update breakables
    for(u32 i=0; i<m_numBreakables; ++i)
    {
        Breakable* pCurrBreakable = &m_updatingBreakables[i];
        
		pCurrBreakable->lifeTimer -= timeElapsed;
		
        const f32 breakableAlpha = ClampF(pCurrBreakable->lifeTimer/0.15f,0.0f,1.0f);
        ScaleVec4(&pCurrBreakable->diffuseColor,&pCurrBreakable->diffuseColorStart,breakableAlpha);
        
        BreakableData* pData = pCurrBreakable->pBreakableData;
        
        pCurrBreakable->currSpinAngle += pCurrBreakable->spinSpeed*timeElapsed;
        
        
        vec3* pBreakablePos = mat4f_GetPos(pCurrBreakable->renderable.worldMat);
        
        pCurrBreakable->velocity.y -= pData->pSettings->gravity*timeElapsed;
        AddScaledVec3_Self(pBreakablePos,&pCurrBreakable->velocity,timeElapsed);
        
        vec3 velNorm;
        TryNormalizeVec3(&velNorm,&pCurrBreakable->velocity);
        
        const f32 maxZForScale = 40.0f;
        const f32 maxZScale = 3.5f;
        
        f32 radius = pData->radius;
        if(pData->scaleWithZ)
        {
            const f32 zScaleT = MinF(pBreakablePos->z/maxZForScale,1.0f);
            radius = Lerp(pData->radius,maxZScale,zScaleT);
        }
        
        mat4f_LoadScaledZRotation_IgnoreTranslation(pCurrBreakable->renderable.worldMat, pCurrBreakable->currSpinAngle, radius);
        
		//Bouncing disabled for now
		
        /*if(pData->pSettings->doesBounce && pBreakablePos->y <= 0.0f)
        {
            pBreakablePos->y = 0.0f;
            
            const f32 dotProd = -DotVec3(&g_GameBox_normal_Floor,&velNorm);
            //printf("DotProd: %f\n",dotProd);
            
            const f32 finalDamp = Lerp(1.0f,pDesc->pSettings->bounceDamping,dotProd);
            //printf("Damping mult: %f\n",finalDamp);
            ScaleVec3_Self(&pCurrBreakable->velocity,finalDamp);
            
            pCurrBreakable->velocity.y *= -1.0f;
        }*/
    }
}


void Game::SpawnBreakable(BreakableData* pData, const vec3* pPosition, const vec3* pDirection, u32 breakableIndex, const vec4* diffuseColor, RenderLayer renderLayer)
{
	if(m_numBreakables == GAME_MAX_BREAKABLES)
	{
		return;
	}
	
	//printf("Spawned breakable!\n");
	
	Breakable* pCurrBreakable = &m_updatingBreakables[m_numBreakables];
	
	pCurrBreakable->pBreakableData = pData;
	ItemArtDescription* pArtDesc = &pData->itemArt;
	const MaterialSettings* pMaterial = pArtDesc->materialSettings;
	
	//[self PlaySoundByFilename:pCurrBreakable->pBreakableDescription->breakSoundName:pPosition:0.0f:FALSE];
	
	RenderableObject3D* pRenderable = &pCurrBreakable->renderable;

	GLRENDERER->InitRenderableObject3D(pRenderable, pArtDesc->pModelData, pMaterial->renderMaterial, &pArtDesc->textureHandle, NULL, renderLayer, View_0, pMaterial->renderFlags|RenderFlag_Visible);
	pRenderable->geom.material.uniqueUniformValues[0] = (u8*)&pCurrBreakable->texcoordOffset;
	pRenderable->geom.material.uniqueUniformValues[1] = (u8*)&pCurrBreakable->diffuseColor;
	GLRENDERER->AddRenderableObject3DToList(pRenderable);
	
	f32 radius = pData->radius;
	
	mat4f_LoadScale(pRenderable->worldMat, radius);
	
	vec3* pPos = mat4f_GetPos(pRenderable->worldMat);
	CopyVec3(pPos, pPosition);
	
	const f32 speed = rand_FloatRange(pData->pSettings->moveSpeedMin, pData->pSettings->moveSpeedMax);
	ScaleVec3(&pCurrBreakable->velocity,pDirection,speed);
	const f32 spinSpeed = rand_FloatRange(pData->pSettings->spinSpeedMin, pData->pSettings->spinSpeedMax);
	pCurrBreakable->spinSpeed = spinSpeed*((rand_FloatRange(0.0f, 1.0f) > 0.5f) ? -1.0f : 1.0f);
	pCurrBreakable->currSpinAngle = 0.0f;
	pCurrBreakable->lifeTimer = pData->pSettings->lifetime;
	
	CopyVec4(&pCurrBreakable->diffuseColor,diffuseColor);
	CopyVec4(&pCurrBreakable->diffuseColorStart,diffuseColor);
	
	switch (breakableIndex)
	{
		case 0:
		{
			SetVec2(&pCurrBreakable->texcoordOffset, 0.0f, 0.0f);
			break;
		}
		case 1:
		{
			SetVec2(&pCurrBreakable->texcoordOffset, 0.5f, 0.0f);
			break;
		}
		case 2:
		{
			SetVec2(&pCurrBreakable->texcoordOffset, 0.0f, 0.5f);
			break;
		}
		case 3:
		{
			SetVec2(&pCurrBreakable->texcoordOffset, 0.5f, 0.5f);
			break;
		}
		default:
		{
			break;
		}
	}
	
	++m_numBreakables;
}


