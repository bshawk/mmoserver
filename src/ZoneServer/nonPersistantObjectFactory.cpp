				   /*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2008 The swgANH Team

---------------------------------------------------------------------------------------
*/

#include "NonPersistantObjectFactory.h"
#include "ObjectFactory.h"
#include "Item.h"
#include "CampRegion.h"
#include "ObjectFactoryCallback.h"
#include "DatabaseManager/Database.h"
#include "DatabaseManager/DatabaseResult.h"
#include "DatabaseManager/DataBinding.h"
#include "LogManager/LogManager.h"
#include "WorldManager.h"
#include "ResourceManager.h"
#include "Utils/utils.h"
#include "DraftSchematic.h"
#include "Utils/typedefs.h"
#include "StructureManager.h"
#include "CampTerminal.h"
#include "Instrument.h"
#include <assert.h>

//=============================================================================

//bool							NonPersistantObjectFactory::mInsFlag    = false;
NonPersistantObjectFactory*		NonPersistantObjectFactory::mSingleton  = NULL;

//======================================================================================================================

NonPersistantObjectFactory* NonPersistantObjectFactory::Instance(void)
{
	if (!mSingleton)
	{
		mSingleton = new NonPersistantObjectFactory(WorldManager::getSingletonPtr()->getDatabase());
	}
	return mSingleton;
}

//=============================================================================

// This constructor prevents the default constructor to be used, as long as the constructor is kept private.

NonPersistantObjectFactory::NonPersistantObjectFactory() : FactoryBase(NULL)
{
}

//=============================================================================

NonPersistantObjectFactory::NonPersistantObjectFactory(Database* database) : FactoryBase(database)
{
	mDatabase	= database;
	mId			= 0x0000100000000000;
	 _setupDatabindings();
}

//=============================================================================

NonPersistantObjectFactory::~NonPersistantObjectFactory()
{
	_destroyDatabindings();
	
}

//=============================================================================

void NonPersistantObjectFactory::handleDatabaseJobComplete(void* ref,DatabaseResult* result)
{
	NonPersistantQueryContainerBase* asyncContainer = reinterpret_cast<NonPersistantQueryContainerBase*>(ref);

	switch(asyncContainer->mQueryType)
	{
		case NonPersistantItemFactoryQuery_MainData:
		{
			Item* item = asyncContainer->mItem;
			_createItem(result,item);
			
			if(item->getLoadState() == LoadState_Attributes)
			{
				QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,ItemFactoryQuery_Attributes,asyncContainer->mClient);
				asContainer->mObject = item;

				mDatabase->ExecuteSqlAsync(this,asContainer,"SELECT attributes.name,item_family_attribute_defaults.attribute_value,attributes.internal"
															 " FROM item_family_attribute_defaults"
															 " INNER JOIN attributes ON (item_family_attribute_defaults.attribute_id = attributes.id)"
															 " WHERE item_family_attribute_defaults.item_type_id = %u ORDER BY item_family_attribute_defaults.attribute_order",item->getItemType());
			}
		}
		break;

		case ItemFactoryQuery_Attributes:
		{
			
			_buildAttributeMap(asyncContainer->mObject,result);

			if(asyncContainer->mObject->getLoadState() == LoadState_Loaded)
				asyncContainer->mOfCallback->handleObjectReady(asyncContainer->mObject,asyncContainer->mClient);
				
		}
		break;

		default:break;
	}

	mQueryContainerPool.free(asyncContainer);
}

//////////////////////////////////////////////////////////////////////////////////
//creates a non persistant tangible object
//loads the necessary blueprints from db
//////////////////////////////////////////////////////////////////////////////////
void NonPersistantObjectFactory::createTangible(ObjectFactoryCallback* ofCallback, uint32 familyId,uint32 typeId,uint64 parentId,Anh_Math::Vector3 position,string customName,DispatchClient* client)
//gObjectFactory->requestNewDefaultItem
//(this,11,1320,entertainer->getId(),99,Anh_Math::Vector3(),"");
{
	//items.maxCondition,items.damage,items.dynamicint32 "
	//get blueprint out of the db
	Item* newItem;
	if (familyId == ItemFamily_Instrument)
	{
		newItem = new (Instrument);
	}
	else
	{
		newItem = new (Item);
	}
	newItem->mPosition = position;
	newItem->setItemFamily(familyId);
	newItem->setItemType(typeId);
	newItem->setParentId(parentId);
	newItem->setCustomName(customName.getAnsi());
	newItem->setMaxCondition(100);
	newItem->setTangibleGroup(TanGroup_Item);
	newItem->setId(getId());

	int8 sql[256];
	sprintf(sql,"SELECT item_types.object_string,item_types.stf_name,item_types.stf_file,item_types.stf_detail_name, item_types.stf_detail_file FROM item_types WHERE item_types.id = '%u'",typeId);
	mDatabase->ExecuteSqlAsync(this,new(mQueryContainerPool.ordered_malloc()) NonPersistantQueryContainerBase(ofCallback,NonPersistantItemFactoryQuery_MainData,client,newItem),sql);
	
	//asyncContainer->mOfCallback->handleObjectReady(item,asyncContainer->mClient);
}

/////////////////////////////////////////////////////////////////////////////////
//we create a camp at the given coordinates together with all the necessary additions as found in structure_item_template
//

TangibleObject* NonPersistantObjectFactory::spawnTangible(StructureItemTemplate* placableTemplate, uint64 parentId,Anh_Math::Vector3 position,string customName, PlayerObject* player)
{
	
	//we dont set the types here as we are factually placing statics / and or items / terminals
	//but we need to revisit this when dealing with high level camps
	//in which case we have to do separate create functions for terminals crafting stations and so on
	TangibleObject* tangible = new(TangibleObject);
	
	tangible->mPosition = position;
	tangible->mPosition.mX += placableTemplate->mPosition.mX;
	tangible->mPosition.mY += placableTemplate->mPosition.mY;
	tangible->mPosition.mZ += placableTemplate->mPosition.mZ;
	
	tangible->mDirection = placableTemplate->mDirection.mX;
	tangible->mDirection.mY = placableTemplate->mDirection.mY;
	tangible->mDirection.mZ = placableTemplate->mDirection.mZ;

	tangible->mDirection.mW = placableTemplate->dw;

	tangible->setName(placableTemplate->name.getAnsi());
	tangible->setNameFile(placableTemplate->file.getAnsi());

	tangible->setParentId(parentId);
	tangible->setCustomName(customName.getAnsi());
	tangible->setMaxCondition(100);

	tangible->setTangibleGroup(placableTemplate->tanType);
	
	//see above notes
	tangible->setTangibleType(TanType_None);

	tangible->setId(this->getId());

	tangible->setModelString(placableTemplate->structureObjectString);

	//create it in the world
	tangible->mDirection = player->mDirection;
	//tangible->setTypeOptions(0xffffffff);

	gWorldManager->addObject(tangible);
				
	//gWorldManager->initPlayersInRange(itemObject);
	//TangibleObject* tangible = dynamic_cast<TangibleObject*>(object);
	
	PlayerObjectSet*			inRangePlayers	= player->getKnownPlayers();
	PlayerObjectSet::iterator	it				= inRangePlayers->begin();
	while(it != inRangePlayers->end())
	{
		PlayerObject* targetObject = (*it);
		gMessageLib->sendCreateTangible(tangible,targetObject);
		targetObject->addKnownObjectSafe(tangible);
		tangible->addKnownObjectSafe(targetObject);
		++it;
	}

	gMessageLib->sendCreateTangible(tangible,player);
	player->addKnownObjectSafe(tangible);
	tangible->addKnownObjectSafe(player);
	gMessageLib->sendDataTransform(tangible);

	return(tangible);

}

CampTerminal* NonPersistantObjectFactory::spawnTerminal(StructureItemTemplate* placableTemplate, uint64 parentId,Anh_Math::Vector3 position,string customName, PlayerObject* player, StructureDeedLink*	deedData)
{
	
	//we dont set the types here as we are factually placing statics / and or items / terminals
	//but we need to revisit this when dealing with high level camps
	//in which case we have to do separate create functions for terminals crafting stations and so on
	CampTerminal* terminal = new(CampTerminal);
	
	terminal->mPosition = position;
	terminal->mPosition.mX += placableTemplate->mPosition.mX;
	terminal->mPosition.mY += placableTemplate->mPosition.mY;
	terminal->mPosition.mZ += placableTemplate->mPosition.mZ;

	terminal->mDirection.mX = placableTemplate->mDirection.mX;
	terminal->mDirection.mY = placableTemplate->mDirection.mY;
	terminal->mDirection.mZ = placableTemplate->mDirection.mZ;

	terminal->mDirection.mW = placableTemplate->dw;

	terminal->setName(deedData->stf_name.getAnsi());
	terminal->setNameFile(deedData->stf_file.getAnsi());

	
	terminal->setParentId(parentId);
	terminal->setCustomName(customName.getAnsi());
	terminal->setMaxCondition(100);

	terminal->setTangibleGroup(placableTemplate->tanType);
	
	//see above notes
	terminal->setTangibleType(TanType_CampTerminal);

	terminal->setId(this->getId());

	//tangible->setOwner(player->getId());
	
	//gLogger->logMsgF("place %s",MSG_HIGH,placableTemplate->structureObjectString.getAnsi());
	terminal->setModelString(placableTemplate->structureObjectString);

	//create it in the world
	//tangible->mDirection = player->mDirection;
	
	gWorldManager->addObject(terminal);
				
	//gWorldManager->initPlayersInRange(itemObject);
	//TangibleObject* tangible = dynamic_cast<TangibleObject*>(object);
	
	PlayerObjectSet*			inRangePlayers	= player->getKnownPlayers();
	PlayerObjectSet::iterator	it				= inRangePlayers->begin();
	while(it != inRangePlayers->end())
	{
		PlayerObject* targetObject = (*it);
		gMessageLib->sendCreateTangible(terminal,targetObject);
		targetObject->addKnownObjectSafe(terminal);
		terminal->addKnownObjectSafe(targetObject);
		++it;
	}

	gMessageLib->sendCreateTangible(terminal,player);
	player->addKnownObjectSafe(terminal);
	terminal->addKnownObjectSafe(player);
	gMessageLib->sendDataTransform(terminal);

	return(terminal);

}


/////////////////////////////////////////////////////////////////////////////////
//we create a camp at the given coordinates together with all the necessary additions as found in structure_item_template
//


void NonPersistantObjectFactory::_createItem(DatabaseResult* result,Item* item)
{
	

	uint64 count = result->getRowCount();
	assert(count == 1);

	result->GetNextRow(mItemBinding,item);

	item->setLoadState(LoadState_Attributes);

}


void NonPersistantObjectFactory::_setupDatabindings()
{

	mItemBinding = mDatabase->CreateDataBinding(4);

	mItemBinding->addField(DFT_bstring,offsetof(Item,mModel),256,0);
	mItemBinding->addField(DFT_bstring,offsetof(Item,mName),64,1);
	mItemBinding->addField(DFT_bstring,offsetof(Item,mNameFile),64,2);
	mItemBinding->addField(DFT_bstring,offsetof(Item,mDetailFile),64,4);

}

void NonPersistantObjectFactory::_destroyDatabindings()
{
	mDatabase->DestroyDataBinding(mItemBinding);
	
}

void NonPersistantObjectFactory::requestObject(ObjectFactoryCallback* ofCallback,uint64 id,uint16 subGroup,uint16 subType,DispatchClient* client)
{
}

