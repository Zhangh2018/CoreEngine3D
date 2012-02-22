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

#include "zlib/zlib.h"
#include "base64.h"
#include "Hash.h"
#include "Box2DDebugDraw.h"

#include "CoreObjects/CoreObjectFactories.h"

#if defined (PLATFORM_WIN)
#include <direct.h>
#include <stdlib.h>
#endif

Game* GAME = NULL;

const MaterialSettings g_Game_BlobShadowSettings =
{
	GL_LINEAR,//GLuint			textureFilterMode;
	GL_CLAMP_TO_EDGE,//GLuint			wrapModeU;
	GL_CLAMP_TO_EDGE,//GLuint			wrapModeV;
    MT_TextureOnlySimple,//RenderMaterial	renderMaterial;
    RenderFlagDefaults_CardWithAlpha,//u32				renderFlags;
	true,
};

ItemArtDescription g_Game_BlobShadowDesc =
{
	"ArtResources/Textures/shadowblob.png",//const char*		textureFileName;
	ImageType_PNG,//ImageType		imageType;
	0,//GLuint			textureHandle;
	&g_Game_BlobShadowSettings,//MaterialSettings	materialSettings;
	&g_Square1x1_modelData//ModelData*		pModelData;
};


#if defined (PLATFORM_IOS) || defined (PLATFORM_ANDROID)
void Game::SetTouchIndexIsLinked(s32 index, bool isLinked)
{
	m_touchIsLinked[index] = false;
}
#endif


void Game::ResetCamera()
{
	m_camLerpTimer = -1.0f;
}


bool Game::Init()
{
	m_paused = false;
	
	m_camLerpTimer = -1.0f;
	
	m_Box2D_pWorld = NULL;
	m_Box2D_pContactListener = NULL;
	m_Box2D_pDebugDraw = NULL;
	
	for(int i=0; i<NumLevelLayers; ++i)
	{
		m_layers[i].tiles = NULL;
		CopyVec3(&m_layers[i].position,&vec3_zero);
	}
	
	//Register the common models people will use
	GLRENDERER->RegisterModel(&g_Square1x1_modelData);
	GLRENDERER->RegisterModel(&g_Square_Tiled_2_modelData);
	GLRENDERER->RegisterModel(&g_Square_Tiled_4_modelData);
	GLRENDERER->RegisterModel(&g_Square_Tiled_8_modelData);
	GLRENDERER->RegisterModel(&g_Square_Tiled_16_modelData);
	GLRENDERER->RegisterModel(&g_Square_Tiled_32_modelData);

#if defined (PLATFORM_WIN)
	char currentPath[_MAX_PATH];
	//GetCurrentDirectory(_MAX_PATH,currentPath);
	GetModuleFileName(0,currentPath,_MAX_PATH);
	std::string pathString(currentPath);

	s32 lastSlashIndex = 0;
	for(u32 i=0; i<pathString.size(); ++i)
	{
		if(pathString[i] == '\\')
		{
			lastSlashIndex = i;
		}
	}

	m_path = pathString.substr(0,lastSlashIndex+1);

	OutputDebugString(m_path.c_str());
#endif

	m_currSongSource = 0;
	m_currSongBuffer = 0;

	m_numLoadedArtDescriptions = 0;
	m_numArtDescriptionsToLoadTexturesFor = 0;
	m_numLoadedSoundDescriptions = 0;
	m_numSoundDescriptionsToLoadWavsFor = 0;
	m_ui_numButtons = 0;
	m_numBreakables = 0;
	
	m_numSongsInPlaylist = 0;
	m_currSongID = -1;
	
#if defined (PLATFORM_IOS)
	m_pTouchInput = [[[TouchInputIOS alloc]init:&m_deviceInputState]retain];
	//HACK:
	[m_pTouchInput SetAccelerometerIsUsed:YES];
#endif
	
	m_pCoreAudioOpenAL = new CoreAudioOpenAL;
	m_pCoreAudioOpenAL->Init();
	
#if defined (PLATFORM_OSX) || defined (PLATFORM_IOS)
	m_pAudioPlayer = nil;
#endif

#if defined (PLATFORM_IOS)
	AVAudioSession *session = [AVAudioSession sharedInstance];
	
	NSError* error;
	[session setCategory:AVAudioSessionCategorySoloAmbient error:&error];
#endif
	
	m_coreObjectManager = new CoreObjectManager;
	
	GAME = this;

	return true;
}


void Game::CleanUp()
{
	if(m_Box2D_pWorld != NULL)
	{
		delete m_Box2D_pWorld;
	}
	
	if(m_Box2D_pContactListener != NULL)
	{
		delete m_Box2D_pContactListener;
	}
	
	for(int i=0; i<NumLevelLayers; ++i)
	{
		if(m_layers[i].tiles != NULL)
		{
			delete[] m_layers[i].tiles;
		}
	}
	
	for(u32 i=0; i<m_numTileSetDescriptions; ++i)
	{
		TileSetDescription* pDesc = &m_tileSetDescriptions[i];
		GLRENDERER->DeleteTexture(&pDesc->loadedTextureID);
	}
	
	delete m_pCoreAudioOpenAL;
	delete m_coreObjectManager;
}


void Game::Update(f32 timeElapsed)
{
#if defined (PLATFORM_OSX) || defined(PLATFORM_WIN)
	
	//35 is the 'P' key
	if(m_keyboardState.buttonState[35] == CoreInput_ButtonState_Began)
	{
		m_paused = !m_paused;
		
		//TODO: change how this works if you want to do
		//fancy UI effects that won't draw any more because
		//you paused the graphics
		GLRENDERER->paused = m_paused;
	}
	
	//Update controls
	
	for(u32 i=0; i<MOUSESTATE_MAX_MOUSEBUTTONS; ++i)
	{
		const CoreInput_ButtonState mouseState = m_mouseState.buttonState[i];

		if(mouseState == CoreInput_ButtonState_Began)
		{
			m_mouseState.buttonState[i] = CoreInput_ButtonState_Held;
		}
		else if(mouseState == CoreInput_ButtonState_Ended)
		{
			m_mouseState.buttonState[i] = CoreInput_ButtonState_None;
		}
	}
	
	//Save last position for sleep detection
	CopyVec2(&m_mouseState.lastPosition,&m_mouseState.position);
	
	for(u32 i=0; i<256; ++i)
	{
		const CoreInput_ButtonState keyState = m_keyboardState.buttonState[i];
		
		if(keyState == CoreInput_ButtonState_Began)
		{
			m_keyboardState.buttonState[i] = CoreInput_ButtonState_Held;
		}
		else if(keyState == CoreInput_ButtonState_Ended)
		{
			m_keyboardState.buttonState[i] = CoreInput_ButtonState_None;
		}
	}
#endif
	
	//If the game is paused, we can bust out at this point
	if(m_paused)
	{
		return;
	}
	
	UpdateBreakables(timeElapsed);

	if(m_camLerpTimer > 0.0f)
	{
		m_camLerpTimer -= timeElapsed;
		if(m_camLerpTimer < 0.0f)
		{
			m_camLerpTimer = 0.0f;
		}
		
		LerpVec3(&m_camPos,&m_desiredCamPos,&m_startCamPos,m_camLerpTimer/m_camLerpTotalTime);
	}
	
	if(m_Box2D_pWorld != NULL)
	{
		m_Box2D_pWorld->Step(timeElapsed, 5, 5);
		
#ifdef _DEBUG
		m_Box2D_pWorld->DrawDebugData();
#endif
	}

	//Lazy so constantly load new resources
	//It can't be THAT bad
	LoadItemArt();
	LoadItemSounds();
}


s32 Game::AddSongToPlaylist(const char* songFilenameMP3)
{
	const u32 songID = m_numSongsInPlaylist;
	
	m_songPlaylist[songID] = new char[strlen(songFilenameMP3+1)];
	strcpy(m_songPlaylist[songID], songFilenameMP3);
	
	++m_numSongsInPlaylist;
	
	return songID;
}


std::string Game::GetPathToFile(const char* filename)
{
#if defined (PLATFORM_OSX) || defined (PLATFORM_IOS)
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

	NSString* fileString = [NSString stringWithCString:filename encoding:NSUTF8StringEncoding];
	NSString *fullPath = [[NSBundle mainBundle] pathForResource:[fileString lastPathComponent] ofType:nil inDirectory:[fileString stringByDeletingLastPathComponent]];
	
	std::string pathString([fullPath UTF8String]);
	
	[pool drain];
	
	return pathString;
#endif

#if defined (PLATFORM_WIN)
	return m_path + std::string(filename);
#endif
}


void Game::PlaySongByID(s32 songID, f32 volume, bool isLooping)
{
	if(songID >= (s32)m_numSongsInPlaylist)
	{
		return;
	}
	
	if(m_currSongID == songID)
	{
		return;
	}
	
#if defined (PLATFORM_OSX) || defined (PLATFORM_IOS)
	[m_pAudioPlayer stop];
	[m_pAudioPlayer release];
	
	NSString* fileString = [NSString stringWithCString:m_songPlaylist[songID] encoding:NSUTF8StringEncoding];
	NSString *fullPath = [[NSBundle mainBundle] pathForResource:[fileString lastPathComponent] ofType:nil inDirectory:[fileString stringByDeletingLastPathComponent]];
	NSURL *soundURL = [NSURL fileURLWithPath:fullPath];
	
	//m_pAudioPlayer = CreateAudioPlayer(fullPath,@"",YES,1.0f);
	NSError* error;
	m_pAudioPlayer = [[AVAudioPlayer alloc] initWithContentsOfURL:soundURL error:&error];
	m_pAudioPlayer.volume = volume;
	m_pAudioPlayer.numberOfLoops = isLooping?-1:0;
	
	[m_pAudioPlayer play];
#endif

#if defined (PLATFORM_WIN)
	/*if(m_currSongSource != 0)
	{
		OPENALAUDIO->DeleteSoundSource(&m_currSongSource);
	}

	if(m_currSongBuffer != 0)
	{
		OPENALAUDIO->DeleteSoundBuffer(&m_currSongBuffer);
	}

	std::string filePath = GetPathToFile(m_songPlaylist[songID]);
	OPENALAUDIO->CreateSoundBufferFromFile(filePath.c_str(),&m_currSongBuffer);
	m_currSongSource = OPENALAUDIO->CreateSoundSourceFromBuffer(m_currSongBuffer);
	OPENALAUDIO->PlaySoundSource(m_currSongSource,1.0f,1.0f,true);*/
#endif
	
	m_currSongID = songID;
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


void Game::UpdateButtons(TouchState touchState, vec2 *pTouchPosBegin, vec2* pTouchPosCurr)
{
	for(u32 i=0; i<m_ui_numButtons; ++i)
	{
		if(touchState != TouchState_None)
		{
			m_ui_buttons[i].PressButton(touchState, pTouchPosBegin, pTouchPosCurr);
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
            return true;
        }
    }
    
    return false;
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
		const bool loadedATexture = GLRENDERER->LoadTexture(pCurrArtDesc->textureFileName, pCurrArtDesc->imageType, &pCurrArtDesc->textureHandle, pMaterialSettings->textureFilterMode, pMaterialSettings->wrapModeU, pMaterialSettings->wrapModeV,pMaterialSettings->flipY);
		
		//If we loaded a texture, we should force a resort for optimum performance
		if(loadedATexture == true)
		{
			GLRENDERER->ForceRenderablesNeedSorting_Normal();
		}
        
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


//SOUND

//Checks if the art will be loaded next time LoadItemArt gets called
bool Game::WillSoundDescriptionBeLoaded(ItemSoundDescription* pSoundDesc)
{
    for(u32 i=0; i<m_numSoundDescriptionsToLoadWavsFor; ++i)
    {
        ItemSoundDescription* pCurrSoundDesc = m_pSoundDescriptionsToLoadWavsFor[i];
        if(pSoundDesc == pCurrSoundDesc)
        {
            return true;
        }
    }
    
    return false;
}


//Call many times to prepare sound to be loaded later
void Game::AddItemSound(ItemSoundDescription* pSoundDescription)
{
    //Make sure this description is not already in the list
    for(u32 i=0; i<m_numSoundDescriptionsToLoadWavsFor; ++i)
    { 
        if(m_pSoundDescriptionsToLoadWavsFor[i] == pSoundDescription)
        {
            return;
        }
    }
    
    //It wasn't in the list, so add it
    m_pSoundDescriptionsToLoadWavsFor[m_numSoundDescriptionsToLoadWavsFor] = pSoundDescription;
    ++m_numSoundDescriptionsToLoadWavsFor;
}


//Call after all your sound has been added to the list of things to load gunna die
//You can call this multiple times if you want and nothing bad will happen
void Game::LoadItemSounds()
{
    //Dump old sounds that don't need to be loaded
    for(u32 i=0; i<m_numLoadedSoundDescriptions; ++i)
    {
        ItemSoundDescription* pCurrSoundDesc = m_pLoadedSoundDescriptions[i];
        if(m_numSoundDescriptionsToLoadWavsFor == 0
		   || WillSoundDescriptionBeLoaded(pCurrSoundDesc) == false)
        {
			OPENALAUDIO->DeleteSoundBuffer(&pCurrSoundDesc->soundBufferID);
        }
    }
	
    //Reset the loaded description count to 0
    m_numLoadedSoundDescriptions = 0;
	
    //Load unloaded level item textures
    //This list has only unique entries
    for(u32 i=0; i<m_numSoundDescriptionsToLoadWavsFor; ++i)
    {
		ItemSoundDescription* pCurrSoundDesc = m_pSoundDescriptionsToLoadWavsFor[i];
	
		OPENALAUDIO->CreateSoundBufferFromFile(pCurrSoundDesc->soundFileName, &pCurrSoundDesc->soundBufferID);
		
        //Store the descriptions we've loaded
		//Some of them were already loaded but now we can keep track!
        m_pLoadedSoundDescriptions[m_numLoadedSoundDescriptions] = pCurrSoundDesc;
        ++m_numLoadedSoundDescriptions;
    }
}


//Call this at init, then call AddArtDescriptionToLoad many times.
//The art you're not using anymore will get deleted when you call LoadItemArt
void Game::ClearItemSounds()
{
	m_numSoundDescriptionsToLoadWavsFor = 0;
}


//Just deletes all the loaded art PERIOD
//You might never have to call this
void Game::DeleteAllItemSounds()
{
	//Dump old sounds that don't need to be loaded
    for(u32 i=0; i<m_numLoadedSoundDescriptions; ++i)
    {
        ItemSoundDescription* pCurrSoundDesc = m_pLoadedSoundDescriptions[i];
		OPENALAUDIO->DeleteSoundBuffer(&pCurrSoundDesc->soundBufferID);
    }
	
	m_numLoadedSoundDescriptions = 0;
	m_numSoundDescriptionsToLoadWavsFor = 0;
}


void Game::UpdateBreakables(f32 timeElapsed)
{
	//TODO: this is probably BAD
	
	//Delete old breakables
	for(u32 i=0; i<m_numBreakables; )
    {
        Breakable* pCurrBreakable = &m_updatingBreakables[i];
        
        //Kill off old breakables
        if(pCurrBreakable->lifeTimer < 0.0f)
        {
			Breakable* pLastBreakable = &m_updatingBreakables[m_numBreakables-1];
			
			//Get handles to both renderables
			RenderableGeometry3D* pCurrGeom = (RenderableGeometry3D*)COREOBJECTMANAGER->GetObjectByHandle(pCurrBreakable->handleRenderable);
			//Delete the current geom because it's getting overwritten
			pCurrGeom->Uninit();

			if(m_numBreakables > 1)
			{
				//Overwrite the breakable
				*pCurrBreakable = *pLastBreakable;
			}
			
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
        
		RenderableGeometry3D* pCurrGeom = (RenderableGeometry3D*)COREOBJECTMANAGER->GetObjectByHandle(pCurrBreakable->handleRenderable);
		
		pCurrBreakable->lifeTimer -= timeElapsed;
		
        const f32 breakableAlpha = ClampF(pCurrBreakable->lifeTimer/0.15f,0.0f,1.0f);
        ScaleVec4(&pCurrBreakable->diffuseColor,&pCurrBreakable->diffuseColorStart,breakableAlpha);
        
        BreakableData* pData = pCurrBreakable->pBreakableData;
        
        pCurrBreakable->currSpinAngle += pCurrBreakable->spinSpeed*timeElapsed;
        
        
        vec3* pBreakablePos = mat4f_GetPos(pCurrGeom->worldMat);
        
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
        
        mat4f_LoadScaledZRotation_IgnoreTranslation(pCurrGeom->worldMat, pCurrBreakable->currSpinAngle, radius);
        
										
		//Have to relink up the uniform values because they're basicaly gone
		//TODO: make this better
		pCurrGeom->material.uniqueUniformValues[0] = (u8*)&pCurrBreakable->texcoordOffset;
		pCurrGeom->material.uniqueUniformValues[1] = (u8*)&pCurrBreakable->diffuseColor;

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
	
	RenderableGeometry3D* pRenderable = NULL;
	pCurrBreakable->handleRenderable = GLRENDERER->CreateRenderableGeometry3D_Normal(&pRenderable);
	
	GLRENDERER->InitRenderableGeometry3D(pRenderable, pArtDesc->pModelData, pMaterial->renderMaterial, &pArtDesc->textureHandle, NULL, renderLayer, View_0, pMaterial->renderFlags|RenderFlag_Visible);
	pRenderable->material.uniqueUniformValues[0] = (u8*)&pCurrBreakable->texcoordOffset;
	pRenderable->material.uniqueUniformValues[1] = (u8*)&pCurrBreakable->diffuseColor;
	
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


CoreObjectHandle Game::CreateRenderableTile(s32 tileID, TileSetDescription* pDesc, RenderableGeometry3D** pGeom, RenderLayer renderLayer, RenderMaterial material, vec2* pOut_texCoordOffset, bool usesViewMatrix)
{
	f32 tileMat[16];
	mat4f_LoadScale(tileMat, (f32)m_tiledLevelDescription.tileDisplaySizeX);
	
	CoreObjectHandle hRenderable = GLRENDERER->CreateRenderableGeometry3D_Normal(pGeom);

	u32 baseFlag = usesViewMatrix ? RenderFlagDefaults_2DTexture_AlphaBlended_UseView:RenderFlagDefaults_2DTexture_AlphaBlended;
	
	GLRENDERER->InitRenderableGeometry3D(*pGeom, pDesc->pModelData, material, &pDesc->loadedTextureID, tileMat, renderLayer, View_0, baseFlag|RenderFlag_Visible);
	
	const s32 tileID_X = tileID%pDesc->numTextureTilesX;
	const s32 tileID_Y = tileID/pDesc->numTextureTilesX;
	
	if(pOut_texCoordOffset != NULL)
	{
		pOut_texCoordOffset->x = (f32)tileID_X/(f32)pDesc->numTextureTilesX;
		pOut_texCoordOffset->y = (f32)tileID_Y/(f32)pDesc->numTextureTilesY;
	}
	
	return hRenderable;
}


void Game::UpdateTiledLevelPosition(vec3* pPosition)
{
	vec3 position;
	ScaleVec3(&position,pPosition,-1.0f);
	
	const s32 distCheckRightAdd = GLRENDERER->screenWidth_points+m_tiledLevelDescription.halfTileSizeX;
	
	for(s32 i=0; i<NumLevelLayers; ++i)
	{
#ifndef _DEBUG
		if(i == (s32)LevelLayer_Collision)
		{
			continue;
		}
#endif
		if(i == (s32)LevelLayer_CameraExtents)
		{
			continue;
		}

		Layer* pCurrLayer = &m_layers[i];
		if(pCurrLayer->tiles == NULL)
		{
			continue;
		}
		
		const RenderLayer renderLayer = (RenderLayer)(RenderLayer_Background0+i);
		const RenderMaterial renderMaterial = pCurrLayer->material;
		
		//If this is the collision layer, it should move at the same rate as the main layer
		const s32 adjustedIndex = (i==(s32)LevelLayer_Main0 || i==(s32)LevelLayer_Collision || i==(s32)LevelLayer_TileObjectArt)?(s32)LevelLayer_Main1:i;
		const s32 scrollIndex = 1+(s32)LevelLayer_Main1-adjustedIndex;	//TODO: index into an array of values maybe

		//pCurrLayer->position.x -= timeElapsed*(f32)(scrollIndex*scrollIndex*scrollIndex)*scrollSpeed;
		ScaleVec3(&pCurrLayer->position,&position,1.0f/(f32)scrollIndex);

		const s32 width = pCurrLayer->numTilesX;
		const s32 height = pCurrLayer->numTilesY;
		
		//If it's the TileObjectArt layer, just update the uniforms
		if(i == (s32)LevelLayer_TileObjectArt)
		{
			for(int y=0; y<height; ++y)
			{
				for(int x=0; x<width; ++x)
				{
					Tile* pTile = &ARRAY2D(pCurrLayer->tiles, x, y, width);
					if(pTile->tileID == -1)
					{
						continue;
					}

					RenderableGeometry3D* pCurrRenderable = (RenderableGeometry3D*)COREOBJECTMANAGER->GetObjectByHandle(pTile->hRenderable);
					if(pCurrRenderable)
					{
						//TODO: do something better than this if possible
						pCurrRenderable->material.uniqueUniformValues[0] = (u8*)&pTile->texCoordOffset;
						
						if(renderMaterial == MT_TextureAndFogColorWithTexcoordOffset
						   || renderMaterial == MT_TextureAndFogColorWithMipMapBlurWithTexcoordOffset)
						{
							pCurrRenderable->material.uniqueUniformValues[1] = (u8*)&pCurrLayer->fogColor;
						}
						
						if(renderMaterial == MT_TextureAndFogColorWithMipMapBlurWithTexcoordOffset)
						{
							pCurrRenderable->material.uniqueUniformValues[2] = (u8*)&pCurrLayer->blurAmount;
						}
					}
				}
			}
		}
		//If it's any other layer, do the whole delete/create tiles thing
		else
		{
			for(int y=0; y<height; ++y)
			{
				const s32 tileBasePosY = y*m_tiledLevelDescription.tileDisplaySizeY+m_tiledLevelDescription.halfTileSizeY;
				
				for(int x=0; x<width; ++x)
				{
					Tile* pTile = &ARRAY2D(pCurrLayer->tiles, x, y, width);
					if(pTile->tileID == -1)
					{
						continue;
					}
					
					const s32 tileBasePosX = x*m_tiledLevelDescription.tileDisplaySizeX+m_tiledLevelDescription.halfTileSizeX;
					
					
					if(-pCurrLayer->position.x > tileBasePosX+m_tiledLevelDescription.halfTileSizeX
					   || -pCurrLayer->position.x+distCheckRightAdd < tileBasePosX)
					{
						if(pTile->hRenderable != INVALID_COREOBJECT_HANDLE)
						{
							RenderableGeometry3D* pCurrRenderable = (RenderableGeometry3D*)COREOBJECTMANAGER->GetObjectByHandle(pTile->hRenderable);
							pCurrRenderable->Uninit();
							pTile->hRenderable = INVALID_COREOBJECT_HANDLE;
						}
					}
					else
					{
						RenderableGeometry3D* pCurrRenderable;
						
						if(pTile->hRenderable == INVALID_COREOBJECT_HANDLE)
						{
							pTile->hRenderable = CreateRenderableTile(pTile->tileID,pTile->pDesc,&pCurrRenderable,renderLayer,renderMaterial,&pTile->texCoordOffset,false);
						}
						else
						{
							pCurrRenderable = (RenderableGeometry3D*)COREOBJECTMANAGER->GetObjectByHandle(pTile->hRenderable);
						}
						
						if(pCurrRenderable == NULL)
						{
							continue;
						}
						
						vec3* pCurrPos = mat4f_GetPos(pCurrRenderable->worldMat);
						pCurrPos->x = 0.5f+tileBasePosX+((s32)pCurrLayer->position.x);
						pCurrPos->y = 0.5f+tileBasePosY+((s32)pCurrLayer->position.y);

						if(pTile->isVisible)
						{
							pCurrRenderable->material.flags |= RenderFlag_Visible;
							
							//TODO: do something better than this if possible
							pCurrRenderable->material.uniqueUniformValues[0] = (u8*)&pTile->texCoordOffset;
							
							if(renderMaterial == MT_TextureAndFogColorWithTexcoordOffset
							   || renderMaterial == MT_TextureAndFogColorWithMipMapBlurWithTexcoordOffset)
							{
								pCurrRenderable->material.uniqueUniformValues[1] = (u8*)&pCurrLayer->fogColor;
							}
							
							if(renderMaterial == MT_TextureAndFogColorWithMipMapBlurWithTexcoordOffset)
							{
								pCurrRenderable->material.uniqueUniformValues[2] = (u8*)&pCurrLayer->blurAmount;
							}
						}
						else
						{
							pCurrRenderable->material.flags &= ~RenderFlag_Visible;
						}
					}
				}
			}
		}
		
		
	}
}

const vec3* Game::GetCameraPosition()
{
	return &m_camPos;
}

//use with caution
void Game::SetCameraPosition(const vec3* pCamPos, f32 lerpTime)
{
	if(lerpTime == 0.0f)
	{
		CopyVec3(&m_camPos,pCamPos);
	}
	else
	{
		m_camLerpTimer = lerpTime;
		m_camLerpTotalTime = lerpTime;
		
		CopyVec3(&m_desiredCamPos,pCamPos);
		CopyVec3(&m_startCamPos,&m_camPos);
	}
}

#if defined (PLATFORM_IOS) || defined (PLATFORM_ANDROID)
DeviceInputState* Game::GetDeviceInputState()
{
	return &m_deviceInputState;
}
#endif

void Game::GetTileIndicesFromScreenPosition(const vec2* pPosition, u32* pOut_X, u32* pOut_Y)
{
	*pOut_X = (pPosition->x+m_camPos.x)/m_tiledLevelDescription.tileDisplaySizeX;
	*pOut_Y = (pPosition->y+m_camPos.y)/m_tiledLevelDescription.tileDisplaySizeY;
}

void Game::GetTileIndicesFromPosition(const vec2* pPosition, u32* pOut_X, u32* pOut_Y)
{
	*pOut_X = (pPosition->x)/m_tiledLevelDescription.tileDisplaySizeX;
	*pOut_Y = (pPosition->y)/m_tiledLevelDescription.tileDisplaySizeY;
}


void Game::GetPositionFromTileIndices(s32 index_X, s32 index_Y, vec3* pOut_position)
{
	pOut_position->x = index_X * m_tiledLevelDescription.tileDisplaySizeX + m_tiledLevelDescription.halfTileSizeX;
	pOut_position->y = index_Y * m_tiledLevelDescription.tileDisplaySizeY + m_tiledLevelDescription.halfTileSizeY;
	pOut_position->z = 0.0f;
}


s32 Game::GetCollisionFromTileIndices(s32 index_X, s32 index_Y)
{
	Layer* pLayer = &m_layers[LevelLayer_Collision];
	
	if(index_X < 0 || index_X > pLayer->numTilesX-1
	   || index_Y < 0 || index_Y > pLayer->numTilesY-1)
	{
		return -1;
	}
	
	Tile* pTile = &ARRAY2D(pLayer->tiles, index_X, index_Y, pLayer->numTilesX);
	
	return pTile->tileID;
}


f32 Game::GetTileSize()
{
	return m_tiledLevelDescription.tileDisplaySizeX;
}


f32 Game::GetHalfTileSize()
{
	return m_tiledLevelDescription.halfTileSizeX;
}


f32 Game::GetPixelsPerMeter()
{
	return m_pixelsPerMeter;
}


void Game::ConstrainCameraToTiledLevel()
{
	Layer* pMainLayer = &m_layers[LevelLayer_CameraExtents];
	
	if(pMainLayer->pLevelData == NULL)
	{
		pMainLayer = &m_layers[LevelLayer_Main1];
	}
	
	const f32 tileSize = GetTileSize();
	
	const f32 maxCameraY = tileSize*0.5f*pMainLayer->numTilesY-GLRENDERER->screenHeight_points;
	if(m_camPos.y > maxCameraY)
	{
		m_camPos.y = maxCameraY;
	}

	const f32 maxCameraX = tileSize*pMainLayer->numTilesX-GLRENDERER->screenWidth_points;
	if(m_camPos.x > maxCameraX)
	{
		m_camPos.x = maxCameraX;
	}
	 
	const f32 minCameraX = 0.0f;
	if(m_camPos.x < minCameraX)
	{
		m_camPos.x = minCameraX;
	}
	
	const f32 minCameraY = 0.0f;
	if(m_camPos.y < minCameraY)
	{
		m_camPos.y = minCameraY;
	}
	
	
}


void Game::ConvertTileID(s32* p_InOut_tileID, TileSetDescription** pOut_tileDesc)
{
	s32 tileID = *p_InOut_tileID;
	tileID &= 0x00FFFFFF;	//Remove the flags
	//Now tileID is a tile index
	
	//Now find the tileset it belongs to
	u32 maxTilesetIndex = 1;
	TileSetDescription* pFoundDesc = &m_tileSetDescriptions[0];
	
	for(s32 tilesetIDX=0; tilesetIDX<m_numTileSetDescriptions; ++tilesetIDX)
	{
		TileSetDescription* pDesc = &m_tileSetDescriptions[tilesetIDX];
		
		if(tileID >= pDesc->firstTileIndex)
		{
			if(pDesc->firstTileIndex >= maxTilesetIndex)
			{
				pFoundDesc = pDesc;
				maxTilesetIndex = pDesc->firstTileIndex;
			}
		}
	}
	
	//Save the tileset description in the tile.
	//It makes it easier to load the tile later.
	*pOut_tileDesc = pFoundDesc;
	
	//Now subtract the highest tileset index from the tileID
	tileID -= maxTilesetIndex;
	
	*p_InOut_tileID = tileID;
}


Layer* Game::GetLayer(LevelLayer layer)
{
	return &m_layers[layer];
}


SpawnableEntity* Game::GetSpawnableEntityByTiledUniqueID(u32 tiledUniqueID)
{
	for(u32 i=0; i<m_numSpawnableEntities; ++i)
	{
		SpawnableEntity* pEnt = &m_spawnableEntities[i];
		if(pEnt->tiledUniqueID == tiledUniqueID)
		{
			return pEnt;
		}
	}
	
	return NULL;
}


void Game::ToggleTileVisibility(LevelLayer levelLayer,u32 tileIndex_X,u32 tileIndex_Y,bool isVisible)
{
	Tile* pTile = &ARRAY2D(m_layers[levelLayer].tiles, tileIndex_X, tileIndex_Y, m_layers[levelLayer].numTilesX);
	if(pTile != NULL && pTile->tileID != -1)
	{
		pTile->isVisible = isVisible;
	}
}


void Game::Box2D_Init(bool continuousPhysicsEnabled, bool allowObjectToSleep)
{
	m_Box2D_pDebugDraw = new Box2DDebugDraw;
	m_Box2D_pDebugDraw->SetFlags(0xFFFFFFFF);
	
	b2Vec2 gravity;
	gravity.Set(0.0f, 10.0f);

	m_Box2D_pWorld = new b2World(gravity);
	m_Box2D_pWorld->SetDebugDraw(m_Box2D_pDebugDraw);
	
	m_Box2D_pWorld->SetContinuousPhysics(continuousPhysicsEnabled);
	m_Box2D_pWorld->SetAllowSleeping(false);
	b2BodyDef bodyDef;
	m_Box2D_pGroundBody = m_Box2D_pWorld->CreateBody(&bodyDef);
	
	m_Box2D_pContactListener = new Box2DContactListener;
	
	m_Box2D_pWorld->SetContactListener(m_Box2D_pContactListener);
}


b2World* Game::Box2D_GetWorld()
{
	return m_Box2D_pWorld;
}

void Game::Box2D_SetContactListener(b2ContactListener* pContactListener)
{
	m_Box2D_pWorld->SetContactListener(pContactListener);
}

b2Body* Game::Box2D_GetGroundBody()
{
	return m_Box2D_pGroundBody;
}

void Game::Box2D_SetGravity(f32 x, f32 y)
{
	m_Box2D_pWorld->SetGravity(b2Vec2(x,y));
}

bool Game::LoadTiledLevel(std::string& path, std::string& filename, u32 tileWidthPixels, f32 tileSizeMeters)
{
	m_pixelsPerMeter = (f32)tileWidthPixels/tileSizeMeters;
	
	const f32 halfTileSize = tileSizeMeters*0.5f;
	
	m_numSpawnableEntities = 0;

	std::string filenameWithPath(path+filename);
	
    pugi::xml_document doc;
	
	pugi::xml_parse_result result = doc.load_file(GetPathToFile(filenameWithPath.c_str()).c_str());
	
	if(result)
	{
		COREDEBUG_PrintDebugMessage("Parsing map file was successful!\n");

		pugi::xml_node map = doc.child("map");
		
		const u32 mapTileSizeX = atoi(map.attribute("tilewidth").value());
		//const u32 mapTileSizeY = atoi(map.attribute("tileheight").value());
		
		m_tiledLevelDescription.tileDisplaySizeX = tileWidthPixels;
		m_tiledLevelDescription.tileDisplaySizeY = tileWidthPixels;
		m_tiledLevelDescription.halfTileSizeX = m_tiledLevelDescription.tileDisplaySizeX/2;
		m_tiledLevelDescription.halfTileSizeY = m_tiledLevelDescription.tileDisplaySizeY/2;
		
		const f32 unitConversionScale = (f32)tileWidthPixels/(f32)mapTileSizeX;
		
		m_numTileSetDescriptions = 0;
		for (pugi::xml_node layer = map.child("tileset"); layer; layer = layer.next_sibling("tileset"),++m_numTileSetDescriptions)
		{
			TileSetDescription* pDesc = &m_tileSetDescriptions[m_numTileSetDescriptions];

			const char* descName = layer.attribute("name").value();
			const size_t descNameSize = strlen(descName);
			pDesc->name = new char[descNameSize+1];
			strcpy(pDesc->name, descName);
			
			pDesc->firstTileIndex = atoi(layer.attribute("firstgid").value());
			pDesc->tileSizeX = atoi(layer.attribute("tilewidth").value());
			pDesc->tileSizeY = atoi(layer.attribute("tileheight").value());
			
			pugi::xml_node textureNode = layer.child("image");
			
			const char* textureName = textureNode.attribute("source").value();
			const size_t textureNameSize = strlen(textureName);
			pDesc->textureFileName = new char[textureNameSize+1];
			strcpy(pDesc->textureFileName, textureName);
			
			pDesc->textureSizeX = atoi(textureNode.attribute("width").value());
			pDesc->textureSizeY = atoi(textureNode.attribute("height").value());
			
			//Load the tile texture into memory
			std::string textureFileName(pDesc->textureFileName);
			std::string texFilenameWithPath(path+textureFileName);
			
			pDesc->loadedTextureID = 0;
			GLRENDERER->LoadTexture(texFilenameWithPath.c_str(), ImageType_PNG, &pDesc->loadedTextureID, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,true);
			
			//Do some useful calculations
			
			pDesc->numTextureTilesX = pDesc->textureSizeX/pDesc->tileSizeX;
			pDesc->numTextureTilesY = pDesc->textureSizeY/pDesc->tileSizeY;
			
			pDesc->halfTexelOffset = 0.5f/pDesc->textureSizeX;

			switch(pDesc->numTextureTilesX)
			{
				case 2:
				{
					pDesc->pModelData = &g_Square_Tiled_2_modelData;
					
					break;
				}
				case 4:
				{
					pDesc->pModelData = &g_Square_Tiled_4_modelData;
					
					break;
				}
				case 8:
				{
					pDesc->pModelData = &g_Square_Tiled_8_modelData;
					
					break;
				}
				case 16:
				{
					pDesc->pModelData = &g_Square_Tiled_16_modelData;
					
					break;
				}
				case 32:
				{
					pDesc->pModelData = &g_Square_Tiled_32_modelData;
					
					break;
				}
			}
		}
		
		for (pugi::xml_node layer = map.child("layer"); layer; layer = layer.next_sibling("layer"))
		{
			const char* layerName = layer.attribute("name").value();

			s32* pData = NULL;

			const u32 width = atoi(layer.attribute("width").value());
			const u32 height = atoi(layer.attribute("height").value());

			COREDEBUG_PrintDebugMessage("\nLayer %s, width: %d, height, %d",layerName,width,height);
			
			LevelLayer currLayer = LevelLayer_Invalid;
			
			if(strcmp(layerName, "Main0") == 0)
			{
				currLayer = LevelLayer_Main0;
			}
			if(strcmp(layerName, "Main1") == 0)
			{
				currLayer = LevelLayer_Main1;
			}
			else if(strcmp(layerName, "Parallax0") == 0)
			{
				currLayer = LevelLayer_Parallax0;
			}
			else if(strcmp(layerName, "Parallax1") == 0)
			{
				currLayer = LevelLayer_Parallax1;
			}
			else if(strcmp(layerName, "Parallax2") == 0)
			{
				currLayer = LevelLayer_Parallax2;
			}
			else if(strcmp(layerName, "Parallax3") == 0)
			{
				currLayer = LevelLayer_Parallax3;
			}
			else if(strcmp(layerName, "Parallax4") == 0)
			{
				currLayer = LevelLayer_Parallax4;
			}
			else if(strcmp(layerName, "Collision") == 0)
			{
				currLayer = LevelLayer_Collision;
			}
			else if(strcmp(layerName, "TileObjectArt") == 0)
			{
				currLayer = LevelLayer_TileObjectArt;
			}
			else if(strcmp(layerName, "CameraExtents") == 0)
			{
				currLayer = LevelLayer_CameraExtents;
			}
			
			if(currLayer == LevelLayer_Invalid)
			{
				COREDEBUG_PrintDebugMessage("Invalid Layer: Skipping...");

				continue;
			}
			
			Layer* pCurrLayer = &m_layers[currLayer];
			
			const u32 numTiles = width*height;
			
			const vec3* pClearColor = GLRENDERER->GetClearColor();
			
			switch(currLayer)
			{
				case LevelLayer_Parallax4:
				{
					pCurrLayer->material = MT_TextureOnlyWithTexcoordOffset;
					
					/*pCurrLayer->material = MT_TextureAndFogColorWithTexcoordOffset;
					
					CopyVec3((vec3*)&pCurrLayer->fogColor,pClearColor);
					pCurrLayer->fogColor.w = 0.95f;
					pCurrLayer->blurAmount = 0.95f;*/
					
					break;
				}
				case LevelLayer_Parallax3:
				{
					pCurrLayer->material = MT_TextureOnlyWithTexcoordOffset;
					
					/*pCurrLayer->material = MT_TextureAndFogColorWithTexcoordOffset;
					
					CopyVec3((vec3*)&pCurrLayer->fogColor,pClearColor);
					pCurrLayer->fogColor.w = 0.8f;
					pCurrLayer->blurAmount = 0.8f;*/
					
					break;
				}
				case LevelLayer_Parallax2:
				{
					pCurrLayer->material = MT_TextureOnlyWithTexcoordOffset;
					
					/*pCurrLayer->material = MT_TextureAndFogColorWithTexcoordOffset;
					
					CopyVec3((vec3*)&pCurrLayer->fogColor,pClearColor);
					pCurrLayer->fogColor.w = 0.6f;
					pCurrLayer->blurAmount = 0.6f;*/
					
					break;
				}
				case LevelLayer_Parallax1:
				{
					pCurrLayer->material = MT_TextureOnlyWithTexcoordOffset;
					
					/*pCurrLayer->material = MT_TextureAndFogColorWithTexcoordOffset;
					
					CopyVec3((vec3*)&pCurrLayer->fogColor,pClearColor);
					pCurrLayer->fogColor.w = 0.4f;
					pCurrLayer->blurAmount = 0.4f;*/

					break;
				}
				case LevelLayer_Parallax0:
				{
					pCurrLayer->material = MT_TextureOnlyWithTexcoordOffset;
					
					/*pCurrLayer->material = MT_TextureAndFogColorWithTexcoordOffset;
					
					CopyVec3((vec3*)&pCurrLayer->fogColor,pClearColor);
					pCurrLayer->fogColor.w = 0.2f;*/

					break;
				}
				case LevelLayer_TileObjectArt:
				case LevelLayer_Collision:
				case LevelLayer_Main0:
				case LevelLayer_Main1:
				{
					pCurrLayer->material = MT_TextureOnlyWithTexcoordOffset;
					
					break;
				}
				default:
				{
					break;
				}
			}
	
			switch(currLayer)
			{
				case LevelLayer_Parallax4:
				case LevelLayer_Parallax3:
				case LevelLayer_Parallax2:
				case LevelLayer_Parallax1:
				case LevelLayer_Parallax0:
				case LevelLayer_Main0:
				case LevelLayer_Main1:
				case LevelLayer_Collision:
				case LevelLayer_TileObjectArt:
				case LevelLayer_CameraExtents:
				{
					pCurrLayer->numTilesX = width;
					pCurrLayer->numTilesY = height;
					
					pData = new s32[numTiles];
					
					pCurrLayer->pLevelData = pData;
					
					CopyVec3(&pCurrLayer->position,&vec3_zero);

					break;
				}
				default:
				{
					break;
				}
			}
			
			if(pData == NULL)
			{
				//If we have no data to write into, we might as well skip
				COREDEBUG_PrintDebugMessage("This layer will not be saved. Skipping...");

				continue;
			}
			
			pugi::xml_node data = layer.child("data");
			//std::cout << "data: " << data.first_child().value() << '\n';
			
			char* dataToDecode = (char*)data.first_child().value();
			
			const int BUFFER_SIZE = 4096;
			
			unsigned char decodedData[BUFFER_SIZE];
			
			size_t outputLength = base64_decode(dataToDecode,decodedData, BUFFER_SIZE);

			COREDEBUG_PrintDebugMessage("base64_decode...");
			
			const u32 dataSize = numTiles*sizeof(u32);
			unsigned long bufferSize = dataSize;
			
			switch(uncompress((Bytef*)pData, &bufferSize, (Bytef*)decodedData, outputLength))
			{
				case Z_OK:
				{
					COREDEBUG_PrintDebugMessage("ZLIB uncompress was successful!\n");
					
					break;
				}
				case Z_MEM_ERROR:
				{
					COREDEBUG_PrintDebugMessage("ZLIB ERROR: Uncompress failed due to MEMORY ERROR.  Exiting program...\n");
					return false;
				}
				case Z_BUF_ERROR:
				{
					COREDEBUG_PrintDebugMessage("ZLIB ERROR: Uncompress failed due to BUFFER ERROR.  Exiting program...\n");
					return false;
				}
				case Z_DATA_ERROR:
				{
					COREDEBUG_PrintDebugMessage("ZLIB ERROR: Uncompress failed due to DATA ERROR.  Exiting program...\n");
					return false;
				}
			}
			
			//Allocate an array of tiles
			pCurrLayer->tiles = new Tile[width*height];
			
			//Set all the tile indices up
			for(u32 y=0; y<height; ++y)
			{
				for(u32 x=0; x<width; ++x)
				{
					Tile* pTile = &ARRAY2D(pCurrLayer->tiles, x, y, width);
					pTile->hRenderable = INVALID_COREOBJECT_HANDLE;

					pTile->tileID = ARRAY2D(pData, x, y, width);
					pTile->isVisible = true;
					ConvertTileID(&pTile->tileID, &pTile->pDesc);
				}
			}
			
			//Create collision if Box2D is enabled
			//Collision!
			if(m_Box2D_pWorld != NULL
			   && currLayer == LevelLayer_Collision)
			{
				for(u32 y=0; y<height; ++y)
				{
					for(u32 x=0; x<width; ++x)
					{
						Tile* pTile = &ARRAY2D(pCurrLayer->tiles, x, y, width);
						if(pTile->tileID == -1)
						{
							continue;
						}
						
						vec3 pos;
						GetPositionFromTileIndices(x, y, &pos);
						
						b2BodyDef bodyDef;
						bodyDef.type = b2_staticBody;
						
						b2FixtureDef fixtureDef;
						fixtureDef.density = 1;
						b2PolygonShape polygonShape;
						polygonShape.SetAsBox(halfTileSize,halfTileSize);
						fixtureDef.shape = &polygonShape;
						fixtureDef.filter.categoryBits = 1 << CollisionFilter_Ground;
						fixtureDef.filter.maskBits = 0xFFFF;
						
						bodyDef.position.Set(pos.x/m_pixelsPerMeter, pos.y/m_pixelsPerMeter);
						
						b2Body* pBody = m_Box2D_pWorld->CreateBody(&bodyDef);
						pBody->CreateFixture(&fixtureDef);
					}
				}
			}
			
			//Create the renderables for all the object art tiles
			if(currLayer == LevelLayer_TileObjectArt)
			{
				for(u32 y=0; y<height; ++y)
				{
					for(u32 x=0; x<width; ++x)
					{
						Tile* pTile = &ARRAY2D(pCurrLayer->tiles, x, y, width);
						if(pTile->tileID == -1)
						{
							continue;
						}
						
						RenderableGeometry3D* pGeom;
						pTile->hRenderable = CreateRenderableTile(pTile->tileID,pTile->pDesc,&pGeom,RenderLayer_AlphaBlended2,MT_TextureOnlyWithTexcoordOffset,&pTile->texCoordOffset,true);
					}
				}
			}
		}
		
		for (pugi::xml_node layer = map.child("objectgroup"); layer; layer = layer.next_sibling("objectgroup"))
		{
			const char* layerName = layer.attribute("name").value();

			COREDEBUG_PrintDebugMessage("Layer: %s",layerName);
			
			for (pugi::xml_node object = layer.child("object"); object; object = object.next_sibling("object"))
			{
				SpawnableEntity* pCurrEnt = &m_spawnableEntities[m_numSpawnableEntities];
				pCurrEnt->pObject = NULL;
				
				pCurrEnt->tiledUniqueID = atoi(object.attribute("uniqueID").value());
				
				const f32 x = (f32)atoi(object.attribute("x").value())*unitConversionScale;
				const f32 y = (f32)atoi(object.attribute("y").value())*unitConversionScale;
				
				pCurrEnt->position.x = x;
				pCurrEnt->position.y = y;
				
				f32 width;
				f32 height;
				
				pugi::xml_attribute gidAttrib = object.attribute("gid");
				if(gidAttrib.empty() == false)
				{
					pCurrEnt->tileID = gidAttrib.as_int();
					ConvertTileID(&pCurrEnt->tileID, &pCurrEnt->pDesc);
					
					width = GAME->GetTileSize();
					height = GAME->GetTileSize();
					
					//TODO: this might be wrong.  Will need more testing
					pCurrEnt->position.x += width/2;
					pCurrEnt->position.y -= height/2;
					
					GetTileIndicesFromPosition((vec2*)&pCurrEnt->position, &pCurrEnt->tileIndexX, &pCurrEnt->tileIndexY);
				}
				else
				{
					pCurrEnt->tileID = -1;
					pCurrEnt->pDesc = NULL;
					
					width = (f32)atoi(object.attribute("width").value())*unitConversionScale;
					height = (f32)atoi(object.attribute("height").value())*unitConversionScale;
					
					pCurrEnt->position.x += width/2.0f;
					pCurrEnt->position.y += height/2.0f;
					
					pCurrEnt->tileIndexX = 0;
					pCurrEnt->tileIndexY = 0;
				}
				
				pCurrEnt->position.z = 0.0f;
			
				const char* typeString = object.attribute("type").value();
				pCurrEnt->type = Hash(typeString);
				
				pCurrEnt->scale.x = width;
				pCurrEnt->scale.y = height;
				
				//Find properties of the object
				//TODO: not do this horrible thing
				pCurrEnt->pProperties = object.child("properties");
				
				const u32 scriptObjectType = Hash("ScriptObject");
				const u32 collisionBoxType = Hash("CollisionBox");
				const u32 objectGroupType = Hash("ObjectGroup");
				const u32 soundPlayerType = Hash("SoundPlayerType");
				const u32 tileAffectorType = Hash("TileAffector");
				
				if(pCurrEnt->type == scriptObjectType)
				{
					pCurrEnt->pObject = g_Factory_ScriptObject.CreateObject(pCurrEnt->type);
				}
				else if(pCurrEnt->type == collisionBoxType)
				{
					pCurrEnt->pObject = g_Factory_CollisionBox.CreateObject(pCurrEnt->type);
				}
				else if(pCurrEnt->type == objectGroupType)
				{
					pCurrEnt->pObject = g_Factory_ObjectGroup.CreateObject(pCurrEnt->type);
				}
				else if(pCurrEnt->type == tileAffectorType)
				{
					pCurrEnt->pObject = g_Factory_TileAffector.CreateObject(pCurrEnt->type);
				}
				else if(pCurrEnt->type == soundPlayerType)
				{
					pCurrEnt->pObject = g_Factory_SoundPlayer.CreateObject(pCurrEnt->type);
				}
				else
				{
					pCurrEnt->pObject = this->CreateObject(pCurrEnt->type);
				}
				
				
				
				++m_numSpawnableEntities;
			}
		}
		
		
		//All the objects have been created, now initialize them!
		for(u32 i=0; i<m_numSpawnableEntities; ++i)
		{
			SpawnableEntity* pEnt = &m_spawnableEntities[i];
			if(pEnt->pObject != NULL)
			{
				pEnt->pObject->SpawnInit(pEnt);
			}
		}
		
		//All the objects have been created, now do the post init!
		for(u32 i=0; i<m_numSpawnableEntities; ++i)
		{
			SpawnableEntity* pEnt = &m_spawnableEntities[i];
			if(pEnt->pObject != NULL)
			{
				pEnt->pObject->PostSpawnInit(pEnt);
			}
		}
	}
	else
	{
		COREDEBUG_PrintDebugMessage("Failed to load level file.  FILE NOT FOUND!");
	}
	
	
	return true;
}
