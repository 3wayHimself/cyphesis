// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2006 Alistair Riddoch
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA


#include "CorePropertyManager.h"

#include "server/Juncture.h"
#include "server/ServerRouting.h"
#include "server/TeleportProperty.h"

#include "rulesets/LineProperty.h"
#include "rulesets/OutfitProperty.h"
#include "rulesets/SolidProperty.h"
#include "rulesets/StatusProperty.h"
#include "rulesets/StatisticsProperty.h"
#include "rulesets/TerrainModProperty.h"
#include "rulesets/TerrainProperty.h"
#include "rulesets/TransientProperty.h"
#include "rulesets/BBoxProperty.h"
#include "rulesets/BiomassProperty.h"
#include "rulesets/BurnSpeedProperty.h"
#include "rulesets/DecaysProperty.h"
#include "rulesets/MindProperty.h"
#include "rulesets/InternalProperties.h"
#include "rulesets/SpawnProperty.h"
#include "rulesets/AreaProperty.h"
#include "rulesets/VisibilityProperty.h"
#include "rulesets/SuspendedProperty.h"
#include "rulesets/TasksProperty.h"
#include "rulesets/EntityProperty.h"
#include "rulesets/SpawnerProperty.h"
#include "rulesets/ImmortalProperty.h"
#include "rulesets/RespawningProperty.h"
#include "rulesets/DefaultLocationProperty.h"
#include "rulesets/DomainProperty.h"
#include "rulesets/LimboProperty.h"
#include "rulesets/ModeProperty.h"
#include "rulesets/PropelProperty.h"
#include "rulesets/DensityProperty.h"
#include "rulesets/AngularFactorProperty.h"
#include "rulesets/GeometryProperty.h"
#include "rulesets/QuaternionProperty.h"
#include "rulesets/Vector3Property.h"

#include "common/Eat.h"
#include "common/Burn.h"
#include "common/Teleport.h"

#include "common/types.h"
#include "common/Inheritance.h"
#include "common/PropertyFactory_impl.h"

#include "common/debug.h"

#include <Atlas/Objects/Operation.h>

#include <iostream>

using Atlas::Message::Element;
using Atlas::Message::ListType;
using Atlas::Message::MapType;
using Atlas::Objects::Root;

static const bool debug_flag = false;

template<typename T>
void CorePropertyManager::installBaseProperty(const std::string & type_name,
                                              const std::string & parent)
{
    installFactory(type_name,
                   atlasType(type_name, parent, true),
                   new PropertyFactory<Property<T>>);
}

template<typename PropertyT>
void CorePropertyManager::installProperty(const std::string & type_name,
                                          const std::string & parent)
{
    installFactory(type_name,
                   atlasType(type_name, parent),
                   new PropertyFactory<PropertyT>);
}

template<typename PropertyT>
void CorePropertyManager::installProperty(const std::string & parent)
{
    this->installProperty<PropertyT>(PropertyT::property_name, parent);
}

template<typename PropertyT>
void CorePropertyManager::installProperty()
{
    this->installProperty<PropertyT>(PropertyT::property_name, PropertyT::property_atlastype);
}


CorePropertyManager::CorePropertyManager()
{
    // Core types, for inheritance only generally.
    installBaseProperty<int>("int", "root_type");
    installBaseProperty<double>("float", "root_type");
    installBaseProperty<std::string>("string", "root_type");
    installBaseProperty<ListType>("list", "root_type");
    installBaseProperty<MapType>("map", "root_type");

    installProperty<Property<double>>("stamina", "float");
    installProperty<ModeProperty>();
    installProperty<LineProperty>("coords", "list");
    installProperty<LineProperty>("points", "list");
    //installProperty<Property<IdList> >("start_intersections", "list");
    //installProperty<Property<IdList> >("end_intersections", "list");
    installProperty<DecaysProperty>("decays", "string");
    installProperty<OutfitProperty>("outfit", "map");
    installProperty<SolidProperty>("solid", "int");
    installProperty<SimpleProperty>("simple", "int");
    installProperty<StatusProperty>("status", "float");
    installProperty<BiomassProperty>("biomass", "float");
    installProperty<BurnSpeedProperty>("burn_speed", "float");
    installProperty<TransientProperty>("transient", "float");
    installProperty<Property<double> >("food", "float");
    installProperty<Property<double> >("mass", "float");
    installProperty<BBoxProperty>("bbox", "list");
    installProperty<MindProperty>("mind", "map");
    installProperty<SetupProperty>("init", "int");
    installProperty<TickProperty>("ticks", "float");
    installProperty<StatisticsProperty>("statistics", "map");
    installProperty<SpawnProperty>("spawn", "map");
    installProperty<AreaProperty>("area", "map");
    installProperty<VisibilityProperty>("visibility", "float");
    installProperty<TerrainModProperty>();
    installProperty<TerrainProperty>("terrain", "map");
    installProperty<TeleportProperty>("linked", "string");
    installProperty<SuspendedProperty>("suspended", "int");
    installProperty<TasksProperty>("tasks", "map");
    installProperty<EntityProperty>("right_hand_wield", "string");
    installProperty<SpawnerProperty>("spawner", "map");
    installProperty<ImmortalProperty>("immortal", "int");
    installProperty<RespawningProperty>("respawning", "string");
    installProperty<DefaultLocationProperty>("default_location", "int");
    installProperty<DomainProperty>("domain", "string");
    installProperty<LimboProperty>("limbo", "int");
    installProperty<PropelProperty>();
    installProperty<DensityProperty>();
    /**
     * Friction is used by the physics system. 0 is no friction, 1 is full friction.
     * This is for "sliding", see "friction-roll" and "friction-spin".
     */
    installProperty<Property<double>>("friction", "float");
    /**
     * Friction for rolling is used by the physics system. 0 is no friction, 1 is full friction.
     */
    installProperty<Property<double>>("friction_roll", "float");
    /**
     * Friction for spinning is used by the physics system. 0 is no friction, 1 is full friction.
     */
    installProperty<Property<double>>("friction_spin", "float");

    installProperty<AngularFactorProperty>();
    installProperty<GeometryProperty>();

    /**
     * Vertical offset to use when entity is planted, and adjusted to the height of the terrain.
     */
    installProperty<Property<double>>("planted-offset", "float");

    /**
     * Vertical scaled offset to use when entity is planted, and adjusted to the height of the terrain.
     * The resulting offset is a product of this value and the height of the entity.
     */
    installProperty<Property<double>>("planted-scaled-offset", "float");

    /**
     * The rotation applied to the entity when it's planted.
     */
    installProperty<QuaternionProperty>("planted-rotation", QuaternionProperty::property_atlastype);
    /**
     * The current extra rotation applied to the entity.
     * This is closely matched with "planted-rotation" to keep track of when the entity has the planted rotation applied and not.
     */
    installProperty<QuaternionProperty>("active-rotation", QuaternionProperty::property_atlastype);

    /**
     * Used for things that grows, to limit the size.
     */
    installProperty<Vector3Property>("maxsize", Vector3Property::property_atlastype);

    /**
     * Specifies how much the entity is allowed to step onto things when moving, as a factor of the entity's height.
     */
    installProperty<Property<double>>("step_factor", "float");

    /**
     * Specifies a mesh, model or mapping to use for client side presentation.
     */
    installProperty<Property<std::string> >("present", "string");

    /**
     * The max speed in meters per second (m/s) when moving over ground.
     */
    installProperty<Property<double>>("speed-ground", "float");
    /**
     * The max speed in meters per second (m/s) when moving in water.
     */
    installProperty<Property<double>>("speed-water", "float");
    /**
     * The max speed in meters per second (m/s) when flying.
     */
    installProperty<Property<double>>("speed-flight", "float");
    /**
     * The max speed in meters per second (m/s) when jumping.
     */
    installProperty<Property<double>>("speed-jump", "float");

    /**
     * If set to 1 the entity is a body of water, i.e. either an Ocean (if no bbox) or a lake/pond (if a bbox).
     */
    installProperty<Property<int>>("water_body", "int");

}

int CorePropertyManager::installFactory(const std::string & type_name,
                                        const Root & type_desc,
                                        PropertyKit * factory)
{
    Inheritance & i = Inheritance::instance();
    if (i.addChild(type_desc) == 0) {
        return -1;
    }

    PropertyManager::installFactory(type_name, factory);

    return 0;
}

PropertyBase * CorePropertyManager::addProperty(const std::string & name,
                                                int type)
{
    assert(!name.empty());
    assert(name != "objtype");
    PropertyBase * p = 0;
    PropertyFactoryDict::const_iterator I = m_propertyFactories.find(name);
    if (I == m_propertyFactories.end()) {
        switch (type) {
          case Element::TYPE_INT:
            p = new Property<int>;
            break;
          case Element::TYPE_FLOAT:
            p = new Property<double>;
            break;
          case Element::TYPE_STRING:
            p = new Property<std::string>;
            break;
          default:
            p = new SoftProperty;
            break;
        }
    } else {
        p = I->second->newProperty();
    }
    debug(std::cout << name << " property found. " << std::endl << std::flush;);
    return p;
}
