/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2014 The SWG:ANH Team
---------------------------------------------------------------------------------------
Use of this source code is governed by the GPL v3 license that can be found
in the COPYING file or at http://www.gnu.org/licenses/gpl-3.0.html

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
---------------------------------------------------------------------------------------
*/
#include "Zoneserver/Objects/PlayerStructureTerminal.h"
#include "ZoneServer/GameSystemManagers/Structure Manager/PlayerStructure.h"

#include "ZoneServer/Objects/Player Object/PlayerObject.h"
#include "ZoneServer/Objects/Tangible Object/TangibleObject.h"
#include "ZoneServer/GameSystemManagers/Structure Manager/HouseObject.h"

#include "ZoneServer/GameSystemManagers/UI Manager/UIManager.h"
#include "ZoneServer/WorldConfig.h"
#include "ZoneServer/WorldManager.h"
#include "MessageLib/MessageLib.h"
#include "DatabaseManager/Database.h"



//=============================================================================

PlayerStructureTerminal::PlayerStructureTerminal() : Terminal()
{
    mStructure = 0;
    setStatic(true);


}

void PlayerStructureTerminal::prepareCustomRadialMenu(CreatureObject* player, uint8 itemCount)
{
    RadialMenu* radial	= new RadialMenu();

    // any object with callbacks needs to handle those (received with menuselect messages) !
    radial->addItem(1,0,radId_examine,radAction_Default,"");
    radial->addItem(2,0,radId_serverTerminalManagement,radAction_ObjCallback,"@player_structure:management");
    radial->addItem(3,0,radId_serverTerminalPermissions,radAction_ObjCallback, "@player_structure:permissions");

    //test if the caller is on the permission list

    //radial->addItem(4,2,radId_serverTerminalManagementDestroy,radAction_ObjCallback,"@player_structure:permission_destroy ");//destroy

    HouseObject* house = dynamic_cast<HouseObject*>(gWorldManager->getObjectById(this->getStructure()));
    if(house)
    {
        radial->addItem(4,2,radId_serverTerminalManagementDestroy,radAction_ObjCallback,"@player_structure:permission_destroy ");//destroy
        radial->addItem(5,2,radId_serverTerminalManagementStatus,radAction_ObjCallback,"@player_structure:management_status");
        radial->addItem(6,2,radId_serverTerminalManagementPay,radAction_ObjCallback,"@player_structure:management_pay");
        radial->addItem(7,2,radId_serverTerminalManagementPrivacy,radAction_ObjCallback,"@player_structure:management_privacy");

        radial->addItem(8,3,radId_serverTerminalPermissionsAdmin,radAction_ObjCallback,"@player_structure:permission_admin");
        radial->addItem(9,3,radId_serverTerminalPermissionsBanned,radAction_ObjCallback,"@player_structure:permission_banned");
        radial->addItem(10,3,radId_serverTerminalPermissionsEnter,radAction_ObjCallback,"@player_structure:permission_enter");
    }

    RadialMenuPtr radialPtr(radial);
    mRadialMenu = radialPtr;

}

//=============================================================================

PlayerStructureTerminal::~PlayerStructureTerminal()
{

}

//=============================================================================

void PlayerStructureTerminal::handleObjectMenuSelect(uint8 messageType,Object* srcObject)
{
    PlayerObject* player = (PlayerObject*)srcObject;

    if ((!player) ||(!player->isConnected()))
    {
        return;
    }

    PlayerStructure* structure = dynamic_cast<PlayerStructure*>(gWorldManager->getObjectById(this->getStructure()));
    //this->getStructure()->toggleStateOn(PlayerStructureState_StartDestruction);
    if(!structure)
    {
        assert(false&&"PlayerStructureTerminal::handleObjectMenuSelect:: Panik!!! No structure");
        return;
    }

    switch(messageType)
    {
    case radId_serverTerminalPermissionsBanned:
    {
       PlayerStructure* structure = dynamic_cast<PlayerStructure*>(gWorldManager->getObjectById(this->getStructure()));
		
		if(structure->getAdminData().check_admin(player->getId()))	{
			structure->sendStructureBanList(player->getId());
		}
	}
    break;

    case radId_serverTerminalPermissionsEnter:
    {
		PlayerStructure* structure = dynamic_cast<PlayerStructure*>(gWorldManager->getObjectById(this->getStructure()));
		
		if(structure->getAdminData().check_admin(player->getId()))	{
			structure->sendStructureEntryList(player->getId());
		}

    }
    break;

    case radId_serverTerminalPermissionsAdmin:    {
		PlayerStructure* structure = dynamic_cast<PlayerStructure*>(gWorldManager->getObjectById(this->getStructure()));
		
		if(structure->getAdminData().check_admin(player->getId()))	{
			structure->sendStructureAdminList(player->getId());
		}
	
    }
    break;

    case radId_serverTerminalManagementPrivacy:
    {
		HouseObject* house = dynamic_cast<HouseObject*>(gWorldManager->getObjectById(this->getStructure()));
			
		if(!house)			{
			DLOG(info) << "StructureManager::processVerification : No Player Building ";
			return;
		}
			
		auto db = gWorldManager->getKernel()->GetDatabase();

		//set to private
		if(house->getPublic())			{
			std::stringstream sql;
			sql << "UPDATE " << db->galaxy() << ".houses h SET h.private = 0 WHERE h.ID = " << this->getStructure();
			db->executeSqlAsync(0,0,sql.str());

			house->setPublic(false);
			gMessageLib->SendSystemMessage(::common::OutOfBand("player_structure","structure_now_private"),player);
			gStructureManager->updateKownPlayerPermissions(house);
			return;
		}

		house->setPublic(true);
		gMessageLib->SendSystemMessage(::common::OutOfBand("player_structure","structure_now_public"),player);
			
		std::stringstream sql;
		sql << "UPDATE " << db->galaxy() << ".houses h SET h.private = 1 WHERE h.ID = " << this->getStructure();
		db->executeSqlAsync(0,0,sql.str());

		gStructureManager->updateKownPlayerPermissions(house);

    }
    break;

    case radId_serverTerminalManagementDestroy:
    {
        //one of the following states set???
        if((structure->checkStatesEither(PlayerStructureState_Destroy)))
        {
            //dont start structure destruction more than once
            return;
        }

        StructureAsyncCommand command;
        command.Command = Structure_Command_Destroy;
        command.PlayerId = player->getId();
        command.StructureId = this->getStructure();

        gStructureManager->checkNameOnPermissionList(this->getStructure(),player->GetCreature()->getId(),player->GetCreature()->getFirstName(),"ADMIN",command);

    }
    break;

    case radId_serverTerminalManagementStatus:
    {
        StructureAsyncCommand command;
        command.Command = Structure_Command_ViewStatus;
        command.PlayerId = player->getId();
        command.StructureId = this->getStructure();

        gStructureManager->checkNameOnPermissionList(this->getStructure(),player->GetCreature()->getId(),player->GetCreature()->getFirstName(),"ADMIN",command);


    }
    break;

    case radId_serverTerminalManagementPay:
    {
        StructureAsyncCommand command;
        command.Command = Structure_Command_PayMaintenance;
        command.PlayerId = player->getId();
        command.StructureId = this->getStructure();

        gStructureManager->checkNameOnPermissionList(this->getStructure(),player->GetCreature()->getId(),player->GetCreature()->getFirstName(),"ADMIN",command);

    }
    break;


    default:
        break;
    }

}


//=============================================================================

void PlayerStructureTerminal::handleUIEvent(uint32 action,int32 element,std::u16string inputStr,UIWindow* window, std::shared_ptr<WindowAsyncContainerCommand> AsyncContainer)
{
    // gLogger->log(LogManager::DEBUG,"InsuranceTerminal::handleUIEvent You are here!",MSG_NORMAL);

    if(window == NULL)
    {
        return;
    }

    PlayerObject* playerObject = window->getOwner(); // window owner

    if(playerObject == NULL || !playerObject->isConnected() || playerObject->getSamplingState() || playerObject->GetCreature()->isIncapacitated() || playerObject->GetCreature()->isDead())
    {
        return;
    }
    /*
    switch(window->getWindowType())
    {







    	default:
    	{
    	}
    	break;
    }
     */
    // gLogger->log(LogManager::DEBUG,"CloningTerminal::handleUIEvent You sure handled this UI-event!, Action = %d", action);
}
