// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000,2001 Alistair Riddoch

#include "ServerRouting_methods.h"
#include "Persistance.h"
#include "CommServer.h"
#include "Account.h"
#include "Lobby.h"

#include <common/debug.h>

static bool debug_flag = false;

using Atlas::Message::Object;

ServerRouting::ServerRouting(CommServer & server, const std::string & ruleset,
                             const std::string & name) :
        commServer(server), svrRuleset(ruleset), svrName(name), opSerialNo(0),
        world(*new WorldRouter(*this)), lobby(*new Lobby())
{
    setId(name);
    idDict[getId()] = this;
    Account * adm = Persistance::loadAdminAccount();
    addObject(adm);
    adm->world=&world;
}

ServerRouting::~ServerRouting()
{
    idDict.erase(getId());
    idDict.erase(world.getId());
    dict_t::const_iterator I = idDict.begin();
    for(; I != idDict.end(); I++) {
        debug(cout << "Del " << I->second->getId() << endl << flush;);
        delete I->second;
    }
    delete &world;
    delete &lobby;
}

void ServerRouting::addToObject(Object::MapType & omap) const
{
    omap["server"] = "cyphesis";
    omap["ruleset"] = svrRuleset;
    omap["name"] = svrName;
    Object::ListType plist(1, "server");
    omap["parents"] = plist;
    omap["clients"] = commServer.numClients();
    omap["uptime"] = world.upTime();
    if (Persistance::restricted) {
        omap["restricted"] = "true";
    }
    
    // We could add all sorts of stats here, but I don't know exactly what yet.
}
