// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2009 Alistair Riddoch
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

// $Id$

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Ruleset.h"

#include "EntityBuilder.h"
#include "EntityFactory.h"
#include "EntityRuleHandler.h"
#include "OpRuleHandler.h"
#include "ScriptFactory.h"
#include "TaskFactory.h"
#include "TaskRuleHandler.h"
#include "TaskScriptFactory.h"
#include "Persistence.h"
#include "Player.h"

#include "rulesets/MindFactory.h"

#include "common/log.h"
#include "common/debug.h"
#include "common/globals.h"
#include "common/const.h"
#include "common/Inheritance.h"
#include "common/AtlasFileLoader.h"
#include "common/compose.hpp"

#include <Atlas/Message/Element.h>
#include <Atlas/Objects/objectFactory.h>

#include <iostream>

#include <sys/types.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif // HAS_DIRENT_H

using Atlas::Message::Element;
using Atlas::Message::MapType;
using Atlas::Message::ListType;
using Atlas::Objects::Root;

using String::compose;

typedef std::map<std::string, Root> RootDict;

static const bool debug_flag = false;

Ruleset * Ruleset::m_instance = NULL;

void Ruleset::init()
{
    m_instance = new Ruleset(EntityBuilder::instance());
    m_instance->loadRules();
}


Ruleset::Ruleset(EntityBuilder * eb) : m_builder(eb),
                                       m_taskHandler(new TaskRuleHandler(eb)),
                                       m_entityHandler(new EntityRuleHandler(eb)),
                                       m_opHandler(new OpRuleHandler(eb))
{
}

Ruleset::~Ruleset()
{
}

int Ruleset::populateEntityFactory(const std::string & class_name,
                                   EntityKit * factory,
                                   const MapType & class_desc)
{
    // assert(class_name == class_desc->getId());
    // Establish whether this rule has an associated script, and
    // if so, use it.
    MapType::const_iterator J = class_desc.find("script");
    MapType::const_iterator Jend = class_desc.end();
    if (J != Jend && J->second.isMap()) {
        const MapType & script = J->second.asMap();
        J = script.find("name");
        if (J == script.end() || !J->second.isString()) {
            log(ERROR, compose("Entity \"%1\" script has no name.",
                               class_name));
            return -1;
        }
        const std::string & script_name = J->second.String();
        J = script.find("language");
        if (J == script.end() || !J->second.isString()) {
            log(ERROR, compose("Entity \"%1\" script has no language.",
                               class_name));
            return -1;
        }
        const std::string & script_language = J->second.String();
        if (script_language != "python") {
            log(ERROR, compose("Entity \"%1\" script has unknown language "
                               "\"%2\".", class_name, script_language));
            return -1;
        }
        std::string::size_type ptr = script_name.rfind(".");
        if (ptr == std::string::npos) {
            log(ERROR, compose("Entity \"%1\" python script has a bad class "
                               "name \"%2\".", class_name, script_name));
            return -1;
        }
        std::string script_package = script_name.substr(0, ptr);
        std::string script_class = script_name.substr(ptr + 1);
        if (factory->m_scriptFactory != 0) {
            if (factory->m_scriptFactory->package() != script_package) {
                delete factory->m_scriptFactory;
                factory->m_scriptFactory = 0;
            }
        }
        if (factory->m_scriptFactory == 0) {
            PythonScriptFactory * psf = new PythonScriptFactory(script_package,
                                                                script_class);
            if (psf->isOkay()) {
                factory->m_scriptFactory = psf;
            } else {
                delete psf;
                return -1;
            }
        }
    }

    // Establish whether this rule has an associated mind rule,
    // and handle it.
    J = class_desc.find("mind");
    if (J != Jend && J->second.isMap()) {
        const MapType & script = J->second.asMap();
        J = script.find("name");
        if (J != script.end() && J->second.isString()) {
            const std::string & mindType = J->second.String();
            // language is unused. might need it one day
            // J = script.find("language");
            // if (J != script.end() && J->second.isString()) {
                // const std::string & mindLang = J->second.String();
            // }
            MindFactory::instance()->addMindType(class_name, mindType);
        }
    }

    // Store the default attribute for entities create by this rule.
    J = class_desc.find("attributes");
    if (J != Jend && J->second.isMap()) {
        const MapType & attrs = J->second.asMap();
        MapType::const_iterator Kend = attrs.end();
        for (MapType::const_iterator K = attrs.begin(); K != Kend; ++K) {
            if (!K->second.isMap()) {
                log(ERROR, compose("Attribute description in rule %1 is not a "
                                   "map.", class_name));
                continue;
            }
            const MapType & attr = K->second.asMap();
            MapType::const_iterator L = attr.find("default");
            if (L != attr.end()) {
                // Store this value in the defaults for this class
                factory->m_classAttributes[K->first] = L->second;
                // and merge it with the defaults inherited from the parent
                factory->m_attributes[K->first] = L->second;
            }
        }
    }

    // Check whether it should be available to players as a playable character.
    J = class_desc.find("playable");
    if (J != Jend && J->second.isInt()) {
        Player::playableTypes.insert(class_name);
    }

    return 0;
}

int Ruleset::populateTaskFactory(const std::string & class_name,
                                 TaskKit * factory,
                                 const Root & class_desc,
                                 std::string & dependent,
                                 std::string & reason)
{
    // assert(class_name == class_desc->getId());

    // Establish that this rule has an associated script.
    Element script_attr;
    if (class_desc->copyAttr("script", script_attr) != 0 ||
        !script_attr.isMap()) {
        log(ERROR, compose("Task \"%1\" has no script.", class_name));
        return -1;
    }
    const MapType & script = script_attr.Map();

    MapType::const_iterator J = script.find("name");
    MapType::const_iterator script_end = script.end();
    if (J == script_end || !J->second.isString()) {
        log(ERROR, compose("Task \"%1\" script has no name.", class_name));
        return -1;
    }
    const std::string & script_name = J->second.String();

    J = script.find("language");
    if (J == script_end || !J->second.isString()) {
        log(ERROR, compose("Task \"%1\" script has no language.", class_name));
        return -1;
    }
    const std::string & script_language = J->second.String();

    if (script_language != "python") {
        log(ERROR, compose("Task \"%1\" script has unknown language \"%2\".",
                           class_name, script_language));
        return -1;
    }
    std::string::size_type ptr = script_name.rfind(".");
    if (ptr == std::string::npos) {
        log(ERROR, compose("Task \"%1\" python script has a bad class "
                           "name \"%2\".", class_name, script_name));
        return -1;
    }
    std::string script_package = script_name.substr(0, ptr);
    std::string script_class = script_name.substr(ptr + 1);

    Element activation_attr;
    if (class_desc->copyAttr("activation", activation_attr) != 0 ||
        !activation_attr.isMap()) {
        log(ERROR, compose("Task \"%1\" has no activation.", class_name));
        return -1;
    }
    const MapType & activation = activation_attr.Map();

    MapType::const_iterator act_end = activation.end();
    J = activation.find("tool");
    if (J == act_end || !J->second.isString()) {
        log(ERROR, compose("Task \"%1\" activation has no tool.", class_name));
        return -1;
    }
    const std::string & activation_tool = J->second.String();

    J = activation.find("operation");
    if (J == act_end || !J->second.isString()) {
        log(ERROR, compose("Task \"%1\" activation has no operation.",
                           class_name));
        return -1;
    }

    const std::string & activation_op = J->second.String();

    Inheritance & i = Inheritance::instance();

    J = activation.find("target");
    if (J != act_end) {
        if (!J->second.isString()) {
            log(ERROR, compose("Task \"%1\" activation has \"%2\" target.",
                               class_name,
                               Element::typeName(J->second.getType())));
            return -1;
        }
        const std::string & target_base = J->second.String();
        if (!i.hasClass(target_base)) {
            dependent = target_base;
            reason = compose("Task \"%1\" is activated on target \"%2\" "
                             "which does not exist.", class_name,
                             target_base);
            return 1;
        }
        factory->m_target = target_base;
    }

    if (!i.hasClass(activation_tool)) {
        dependent = activation_tool;
        reason = compose("Task \"%1\" is activated by tool \"%2\" which "
                         "does not exist.", class_name, activation_tool);
        return 1;
    }
    EntityKit * tool_factory = m_builder->getClassFactory(activation_tool);
    if (tool_factory == 0) {
        log(ERROR, compose("Task class \"%1\" is activated by tool \"%2\" "
                           "which is not an entity class.", class_name,
                           activation_tool));
        return -1;
    }

    if (!i.hasClass(activation_op)) {
        dependent = activation_op;
        reason = compose("Task \"%1\" is activated by operation \"%2\" "
                         "which does not exist.", class_name, 
                         activation_op);
        return 1;
    }

    if (factory->m_scriptFactory != 0) {
        if (factory->m_scriptFactory->package() != script_package) {
            delete factory->m_scriptFactory;
            factory->m_scriptFactory = 0;
        }
    }
    if (factory->m_scriptFactory == 0) {
        PythonTaskScriptFactory * ptsf = new PythonTaskScriptFactory(script_package,
                                                                     script_class);
        if (ptsf->isOkay()) {
            factory->m_scriptFactory = ptsf;
        } else {
            delete ptsf;
            return -1;
        }
    }

    // FIXME This does not check for or remove old activations for this
    // factory
    m_builder->addTaskActivation(activation_tool, activation_op, factory);

    MapType::iterator L = tool_factory->m_classAttributes.find("operations");
    if (L == tool_factory->m_classAttributes.end()) {
        tool_factory->m_classAttributes["operations"] = ListType(1, activation_op);
        tool_factory->m_attributes["operations"] = ListType(1, activation_op);
        tool_factory->updateChildren();
        tool_factory->updateChildrenProperties();
    } else {
        if (L->second.isList()) {
            ListType::const_iterator M = L->second.List().begin();
            for (; M != L->second.List().end() && *M != activation_op; ++M);
            if (M == L->second.List().end()) {
                L->second.List().push_back(activation_op);
                tool_factory->m_attributes[L->first] = L->second.List();
                tool_factory->updateChildren();
                tool_factory->updateChildrenProperties();
            }
        }
    }
    
    return 0;
}

int Ruleset::installTaskClass(const std::string & class_name,
                              const std::string & parent,
                              const Root & class_desc,
                              std::string & dependent,
                              std::string & reason)
{
    assert(class_name == class_desc->getId());

    if (m_builder->hasTask(class_name)) {
        log(ERROR, compose("Attempt to install task \"%1\" which is already "
                           "installed.", class_name));
        return -1;
    }
    
    TaskKit * factory = new TaskFactory(class_name);

    int ret = populateTaskFactory(class_name, factory, class_desc,
                                  dependent, reason);
    if (ret != 0) {
        delete factory;
        return ret;
    }
    m_builder->installTaskFactory(class_name, factory);

    Inheritance & i = Inheritance::instance();

    i.addChild(class_desc);

    return 0;
}

int Ruleset::installEntityClass(const std::string & class_name,
                                const std::string & parent,
                                const Root & class_desc,
                                std::string & dependent,
                                std::string & reason)
{
    assert(class_name == class_desc->getId());

    // Get the new factory for this rule
    EntityKit * parent_factory = m_builder->getClassFactory(parent);
    if (parent_factory == 0) {
        debug(std::cout << "class \"" << class_name
                        << "\" has non existant parent \"" << parent
                        << "\". Waiting." << std::endl << std::flush;);
        dependent = parent;
        reason = compose("Entity rule \"%1\" has parent \"%2\" which does "
                         "not exist.", class_name, parent);
        return 1;
    }
    EntityKit * factory = parent_factory->duplicateFactory();
    if (factory == 0) {
        log(ERROR,
            compose("Attempt to install rule \"%1\" which has parent \"%2\" "
                    "which cannot be instantiated", class_name, parent));
        return -1;
    }

    assert(factory->m_parent == parent_factory);

    // Copy the defaults from the parent. In populateEntityFactory this may be
    // overriden with the defaults for this class.
    factory->m_attributes = parent_factory->m_attributes;

    if (populateEntityFactory(class_name, factory,
                              class_desc->asMessage()) != 0) {
        delete factory;
        return -1;
    }

    debug(std::cout << "INSTALLING " << class_name << ":" << parent
                    << std::endl << std::flush;);

    // Install the factory in place.
    m_builder->installFactory(class_name, parent, factory, class_desc);

    factory->addProperties();

    // Add it as a child to its parent.
    parent_factory->m_children.insert(factory);

    return 0;
}

int Ruleset::installOpDefinition(const std::string & class_name,
                                 const std::string & parent,
                                 const Root & class_desc,
                                 std::string & dependent,
                                 std::string & reason)
{
    assert(class_name == class_desc->getId());

    Inheritance & i = Inheritance::instance();

    if (!i.hasClass(parent)) {
        debug(std::cout << "op_definition \"" << class_name
                        << "\" has non existant parent \"" << parent
                        << "\". Waiting." << std::endl << std::flush;);
        dependent = parent;
        reason = compose("Operation \"%1\" has parent \"%2\" which does "
                         "not exist.", class_name, parent);
        return 1;
    }

    Atlas::Objects::Root r = atlasOpDefinition(class_name, parent);

    if (i.addChild(class_desc) == 0) {
        return -1;
    }

    int op_no = Atlas::Objects::Factories::instance()->addFactory(class_name, &Atlas::Objects::generic_factory);
    i.opInstall(class_name, op_no);

    return 0;
}

int Ruleset::installRuleInner(const std::string & class_name,
                              const Root & class_desc,
                              std::string & dependent,
                              std::string & reason)
{
    assert(class_name == class_desc->getId());

    if (class_name.size() > consts::id_len) {
        log(ERROR, compose("Rule \"%1\" has name longer than %2 characters. "
                           "Skipping.", class_name, consts::id_len));
        return -1;
    }

    const std::string & objtype = class_desc->getObjtype();
    const std::list<std::string> & parents = class_desc->getParents();
    if (parents.empty()) {
        log(ERROR, compose("Rule \"%1\" has empty parents. Skipping.",
                           class_name));
        return -1;
    }
    const std::string & parent = parents.front();
    if (parent.empty()) {
        log(ERROR, compose("Rule \"%1\" has empty first parent. Skipping.",
                           class_name));
        return -1;
    }
    int ret = -1;
    if (m_opHandler->check(class_desc) == 0) {
        ret = m_opHandler->install(class_name, parent, class_desc,
                                   dependent, reason);
    } else if (m_taskHandler->check(class_desc) == 0) {
        ret = m_taskHandler->install(class_name, parent, class_desc,
                                     dependent, reason);
    } else if (m_entityHandler->check(class_desc) == 0) {
        ret = m_entityHandler->install(class_name, parent, class_desc,
                                       dependent, reason);
    } else {
        log(ERROR, compose("Rule \"%1\" has unknown objtype=\"%2\". Skipping.",
                           class_name, objtype));
        return -1;
    }

    return ret;
}

int Ruleset::installRule(const std::string & class_name,
                         const Root & class_desc)
{
    std::string dependent, reason;
    // Possibly we should report some types of failure here.
    int ret = installRuleInner(class_name, class_desc, dependent, reason);
    if (ret == 0 && database_flag) {
        Persistence * p = Persistence::instance();
        p->storeRule(class_desc, class_name);
    }
    return ret;
}

void Ruleset::installItem(const std::string & class_name,
                          const Root & class_desc)
{
    std::string dependent, reason;
    int ret = installRuleInner(class_name, class_desc, dependent, reason);
    if (ret != 0) {
        if (ret > 0) {
            waitForRule(class_name, class_desc, dependent, reason);
        }
        return;
    }

    // Install any rules that were waiting for this rule before they
    // could be installed
    RuleWaitList::iterator I = m_waitingRules.lower_bound(class_name);
    RuleWaitList::iterator Iend = m_waitingRules.upper_bound(class_name);
    RootDict readyRules;
    for (; I != Iend; ++I) {
        const std::string & wClassName = I->second.name;
        const Root & wClassDesc = I->second.desc;
        readyRules.insert(std::make_pair(wClassName, wClassDesc));
        debug(std::cout << "WAITING rule " << wClassName
                        << " now ready from " << class_name
                        << std::endl << std::flush;);
    }
    m_waitingRules.erase(class_name);
        
    RootDict::const_iterator K = readyRules.begin();
    RootDict::const_iterator Kend = readyRules.end();
    for (; K != Kend; ++K) {
        const std::string & rClassName = K->first;
        const Root & rClassDesc = K->second;
        installItem(rClassName, rClassDesc);
    }
    return;
}

int Ruleset::modifyEntityClass(const std::string & class_name,
                               const Root & class_desc)
{
    assert(class_name == class_desc->getId());

    EntityKit * factory = m_builder->getClassFactory(class_name);
    if (factory == 0) {
        log(ERROR, compose("Could not find factory for existing entity class "
                           "\"%1\".", class_name));
        return -1;
    }
    assert(factory != 0);
    
    ScriptKit * script_factory = factory->m_scriptFactory;
    if (script_factory != 0) {
        script_factory->refreshClass();
    }

    MapType backup_attributes = factory->m_attributes,
            backup_class_attributes = factory->m_classAttributes;

    // Copy the defaults from the parent. In populateEntityFactory this may be
    // overriden with the defaults for this class.
    if (factory->m_parent != 0) {
        factory->m_attributes = factory->m_parent->m_attributes;
    } else {
        // This is non fatal, but nice to know it has happened.
        // This should only happen if the client attempted to modify the
        // type data for a core hard coded type.
        log(ERROR, compose("Ruleset::modifyEntityClass: \"%1\" modified "
                           "by client, but has no parent factory.",
                           class_name));
        factory->m_attributes = MapType();
    }
    factory->m_classAttributes = MapType();

    if (populateEntityFactory(class_name, factory,
                              class_desc->asMessage()) != 0) {
        factory->m_attributes = backup_attributes;
        factory->m_classAttributes = backup_class_attributes;
        return -1;
    }

    factory->updateChildren();
    factory->updateChildrenProperties();

    return 0;
}

int Ruleset::modifyTaskClass(const std::string & class_name,
                             const Root & class_desc)
{
    assert(class_name == class_desc->getId());

    TaskKit * factory = m_builder->getTaskFactory(class_name);
    if (factory == 0) {
        log(ERROR, compose("Could not find factory for existing task class "
                           "\"%1\"", class_name));
        return -1;
    }

    assert(factory != 0);

    TaskScriptKit * script_factory = factory->m_scriptFactory;
    if (script_factory != 0) {
        script_factory->refreshClass();
    }

    std::string dependent, reason;
    int ret = populateTaskFactory(class_name, factory, class_desc,
                                  dependent, reason);
    if (ret != 0) {
        if (ret > 0) {
            log(ERROR, reason);
        }
        return -1;
    }

    return 0;
}

int Ruleset::modifyOpDefinition(const std::string & class_name,
                                const Root & class_desc)
{
    // Nothing to actually do
    return 0;
}

int Ruleset::modifyRule(const std::string & class_name,
                        const Root & class_desc)
{
    assert(class_name == class_desc->getId());

    Root o = Inheritance::instance().getClass(class_name);
    if (!o.isValid()) {
        log(ERROR, compose("Could not find existing type \"%1\" in "
                           "inheritance", class_name));
        return -1;
    }
    assert(!o->isDefaultParents());
    assert(!o->getParents().empty());
    if (class_desc->isDefaultParents()) {
        log(ERROR, compose("Updated type \"%1\" has no parents in its "
                           "description", class_name));
        return -1;
    }
    const std::list<std::string> & class_parents = class_desc->getParents();
    if (class_parents.empty()) {
        log(ERROR, compose("Updated type \"%1\" has empty parents in its "
                           "description", class_name));
        return -1;
    }
    if (class_parents.front() != o->getParents().front()) {
        log(ERROR, compose("Updated type \"%1\" attempting to change parent "
                           "from %2 to %3", class_name,
                           o->getParents().front(), class_parents.front()));
        return -1;
    }
    int ret = -1;
    if (m_opHandler->check(class_desc) == 0) {
        ret = m_opHandler->update(class_name, class_desc);
    } else if (m_taskHandler->check(o) == 0) {
        ret = m_taskHandler->update(class_name, class_desc);
    } else if (m_entityHandler->check(class_desc) == 0) {
        ret = m_entityHandler->update(class_name, class_desc);
    }
    if (ret == 0) {
        Inheritance::instance().updateClass(class_name, class_desc);
        if (database_flag) {
            Persistence * p = Persistence::instance();
            p->updateRule(class_desc, class_name);
        }
    }
    return ret;
}

/// \brief Mark a rule down as waiting for another.
///
/// Note that a rule cannot yet be installed because it depends on something
/// that has not yet occured, or a more fatal condition has occured.
void Ruleset::waitForRule(const std::string & rulename,
                          const Root & ruledesc,
                          const std::string & dependent,
                          const std::string & reason)
{
    RuleWaiting rule;
    rule.name = rulename;
    rule.desc = ruledesc;
    rule.reason = reason;

    m_waitingRules.insert(std::make_pair(dependent, rule));
}

void Ruleset::getRulesFromFiles(RootDict & rules)
{
    std::string filename;

    std::string dirname = etc_directory + "/cyphesis/" + ruleset + ".d";
    DIR * rules_dir = ::opendir(dirname.c_str());
    if (rules_dir == 0) {
        filename = etc_directory + "/cyphesis/" + ruleset + ".xml";
        AtlasFileLoader f(filename, rules);
        if (f.isOpen()) {
            log(WARNING, compose("Reading legacy rule data from \"%1\".",
                                 filename));
            f.read();
        }
        return;
    }
    while (struct dirent * rules_entry = ::readdir(rules_dir)) {
        if (rules_entry->d_name[0] == '.') {
            continue;
        }
        filename = dirname + "/" + rules_entry->d_name;
        
        AtlasFileLoader f(filename, rules);
        if (!f.isOpen()) {
            log(ERROR, compose("Unable to open rule file \"%1\".", filename));
        } else {
            f.read();
        }
    }
    ::closedir(rules_dir);
}

void Ruleset::loadRules()
{
    RootDict ruleTable;

    if (database_flag) {
        Persistence * p = Persistence::instance();
        p->getRules(ruleTable);
    } else {
        getRulesFromFiles(ruleTable);
    }

    if (ruleTable.empty()) {
        log(ERROR, "Rule database table contains no rules.");
        if (database_flag) {
            log(NOTICE, "Attempting to load temporary ruleset from files.");
            getRulesFromFiles(ruleTable);
        }
    }

    RootDict::const_iterator Iend = ruleTable.end();
    for (RootDict::const_iterator I = ruleTable.begin(); I != Iend; ++I) {
        const std::string & class_name = I->first;
        const Root & class_desc = I->second;
        installItem(class_name, class_desc);
    }
    // Report on the non-cleared rules.
    // Perhaps we can keep them too?
    // m_waitingRules.clear();
    RuleWaitList::const_iterator J = m_waitingRules.begin();
    RuleWaitList::const_iterator Jend = m_waitingRules.end();
    for (; J != Jend; ++J) {
        log(ERROR, J->second.reason);
    }
}
