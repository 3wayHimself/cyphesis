// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2001 Alistair Riddoch
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

#include "Interactive.h"

#include "Flusher.h"
#include "OperationMonitor.h"
#include "WorldDumper.h"
#include "WorldLoader.h"

#include "common/AtlasStreamClient.h"
#include "common/log.h"
#include "common/OperationRouter.h"
#include "common/globals.h"
#include "common/sockets.h"

#include "common/Tick.h"

#include <Atlas/Objects/Encoder.h>
#include <Atlas/Net/Stream.h>
#include <Atlas/Objects/Decoder.h>
#include <Atlas/Codec.h>
#include <Atlas/Objects/Entity.h>
#include <Atlas/Objects/Anonymous.h>
#include <Atlas/Objects/Operation.h>

#include "common/Monitor.h"
#include "common/Connect.h"
#include "common/compose.hpp"
#include "common/utils.h"

#include <skstream/skstream_unix.h>

#include <sigc++/connection.h>

#ifndef READLINE_CXX_SANE   // defined in config.h
extern "C" {
#endif
#define USE_VARARGS
#define PREFER_STDARG
#include <readline/readline.h>
#include <readline/history.h>
#ifndef READLINE_CXX_SANE
}
#endif

#include <iostream>
#include <algorithm>

#include <cstdio>

using Atlas::Message::Element;
using Atlas::Message::MapType;
using Atlas::Message::ListType;

using Atlas::Objects::Root;

using Atlas::Objects::Operation::Appearance;
using Atlas::Objects::Operation::Create;
using Atlas::Objects::Operation::Delete;
using Atlas::Objects::Operation::Disappearance;
using Atlas::Objects::Operation::Get;
using Atlas::Objects::Operation::Set;
using Atlas::Objects::Operation::Look;
using Atlas::Objects::Operation::Login;
using Atlas::Objects::Operation::Logout;
using Atlas::Objects::Operation::Talk;
using Atlas::Objects::Operation::Tick;
using Atlas::Objects::Entity::RootEntity;
using Atlas::Objects::Entity::Anonymous;

using Atlas::Objects::smart_dynamic_cast;

using Atlas::Objects::Operation::Monitor;
using Atlas::Objects::Operation::Connect;

/// \brief Entry in the global command table for cycmd
struct command {
    const char * cmd_string;
    const char * cmd_description;
    int (Interactive::*cmd_method)(struct command *, const std::string &);
    const char * cmd_longhelp;
};

struct command commands[] = {
    { "add_agent",      "Create an in-game agent",
      &Interactive::commandUnknown, 0, },
    { "cancel",         "Cancel the current admin task",
      &Interactive::commandUnknown, 0, },
    { "connect",        "Connect server to a peer",
      &Interactive::commandUnknown, 0, },
    { "create",         "Use account to create server objects",
      &Interactive::commandUnknown, 0, },
    { "creator_create", "Use agent to create an entity",
      &Interactive::commandUnknown, 0, },
    { "creator_look",   "Use agent to look at an entity",
      &Interactive::commandUnknown, 0, },
    { "delete",         "Delete an entity from the server",
      &Interactive::commandUnknown, 0, },
    { "dump",           "Write a copy of the world to an Atlas file",
      &Interactive::commandUnknown, 0, },
    { "get",            "Examine any object on the server",
      &Interactive::commandUnknown, 0, },
    { "find_by_name",   "Find an entity with the given name",
      &Interactive::commandUnknown, 0, },
    { "find_by_type",   "Find an entity with the given type",
      &Interactive::commandUnknown, 0, },
    { "flush",          "Flush entities from the server",
      &Interactive::commandUnknown, 0, },
    { "help",           "Display this help",
      &Interactive::commandUnknown, 0, },
    { "install",        "Install a new type",
      &Interactive::commandUnknown, 0, },
    { "login",          "Log into a peer server",
      &Interactive::commandUnknown, 0, },
    { "restore",        "Read world data from file and add it to the world",
      &Interactive::commandUnknown, 0, },
    { "look",           "Return the current server lobby",
      &Interactive::commandUnknown, 0, },
    { "logout",         "Log user out of server",
      &Interactive::commandUnknown, 0, },
    { "monitor",        "Enable in-game op monitoring",
      &Interactive::commandUnknown, 0, },
    { "query",          "Synonym for \"get\" (deprecated)",
      &Interactive::commandUnknown, 0, },
    { "reload",         "Reload the script for a type",
      &Interactive::commandUnknown, 0, },
    { "stat",           "Return current server status",
      &Interactive::commandUnknown, 0, },
    { "unmonitor",      "Disable in-game op monitoring",
      &Interactive::commandUnknown, 0, },
    { NULL,             "Guard", 0, 0, }
};


static void help()
{
    size_t max_length = 0;

    for (struct command * I = &commands[0]; I->cmd_string != NULL; ++I) {
       max_length = std::max(max_length, strlen(I->cmd_string));
    }
    max_length += 2;

    std::cout << "Cyphesis commands:" << std::endl << std::endl;

    for (struct command * I = &commands[0]; I->cmd_string != NULL; ++I) {
        std::cout << " " << I->cmd_string
                  << std::string(max_length - strlen(I->cmd_string), ' ')
                  << I->cmd_description << std::endl;
    }
    std::cout << std::endl << std::flush;
}

AccountContext::AccountContext(const std::string & u) : m_username(u),
                                                        m_refNo(0L)
{
}

bool AccountContext::accept(const Operation& op) const
{
    std::cout << "Checking account context to see if it matches"
              << std::endl << std::flush;
    if (m_refNo != 0L && !op->isDefaultRefno() && op->getRefno() == m_refNo) {
        return true;
    }
    return false;
}

int AccountContext::dispatch(const Operation & op)
{
    std::cout << "Dispatching with account context to see if it matches"
              << std::endl << std::flush;
    assert(m_refNo != 0L);
    m_refNo = 0L;
    return 0;
}

std::string AccountContext::repr() const
{
    return m_username;
}

Interactive::Interactive() : m_avatar_flag(false), m_server_flag(false),
                             m_juncture_flag(false),
                             m_serverName("cyphesis"), m_prompt("cyphesis> "),
                             m_exit_flag(false)
{
}

Interactive::~Interactive()
{
}

void Interactive::operation(const Operation & op)
{
    ContextMap::const_iterator J = m_contexts.begin();
    ContextMap::const_iterator Jend = m_contexts.end();
    for (; J != Jend; ++J) {
        ObjectContext & c = *J->second;
        if (c.accept(op)) {
            c.dispatch(op);
        }
    }
    AdminClient::operation(op);
}

void Interactive::appearanceArrived(const Operation & op)
{
    if (accountId.empty()) {
        return;
    }
    if (accountId != op->getTo()) {
        // This is an IG op we are monitoring
        return;
    }
    if (op->getArgs().empty()) {
        return;
    }
    RootEntity ent = smart_dynamic_cast<RootEntity>(op->getArgs().front());
    if (!ent.isValid()) {
        std::cerr << "Got Appearance of non-entity" << std::endl << std::flush;
        return;
    }
    if (!ent->hasAttrFlag(Atlas::Objects::ID_FLAG)) {
        std::cerr << "Got Appearance of non-string ID" << std::endl << std::flush;
        return;
    }
    const std::string & id = ent->getId();
    std::cout << "Appearance(id: " << id << ")";
    if (!ent->hasAttrFlag(Atlas::Objects::Entity::LOC_FLAG)) {
        std::cout << std::endl << std::flush;
        return;
    }
    const std::string & loc = ent->getLoc();
    std::cout << " in " << loc << std::endl;
    if (loc == "lobby") {
        std::cout << id << " has logged in." << std::endl;
    }
    std::cout << std::flush;
}

void Interactive::disappearanceArrived(const Operation & op)
{
    if (accountId.empty()) {
        return;
    }
    if (accountId != op->getTo()) {
        // This is an IG op we are monitoring
        return;
    }
    if (op->getArgs().empty()) {
        return;
    }
    RootEntity ent = smart_dynamic_cast<RootEntity>(op->getArgs().front());
    if (!ent.isValid()) {
        std::cerr << "Got Disappearance of non-entity" << std::endl << std::flush;
        return;
    }
    if (!ent->hasAttrFlag(Atlas::Objects::ID_FLAG)) {
        std::cerr << "Got Disappearance of non-string ID" << std::endl << std::flush;
        return;
    }
    const std::string & id = ent->getId();
    std::cout << "Disappearance(id: " << id << ")";
    if (!ent->hasAttrFlag(Atlas::Objects::Entity::LOC_FLAG)) {
        std::cout << std::endl << std::flush;
        return;
    }
    const std::string & loc = ent->getLoc();
    std::cout << " in " << loc << std::endl;
    if (loc == "lobby") {
        std::cout << id << " has logged out." << std::endl;
    }
    std::cout << std::flush;
}

void Interactive::infoArrived(const Operation & op)
{
    reply_flag = true;
    if (op->getArgs().empty()) {
        return;
    }
    const Root & ent = op->getArgs().front();
    if (m_avatar_flag) {
        std::cout << "Create agent success" << std::endl << std::flush;
        if (!ent->hasAttrFlag(Atlas::Objects::ID_FLAG)) {
            std::cerr << "ERROR: Response to agent create does not contain agent id"
                      << std::endl << std::flush;
            
        } else {
            m_agentId = ent->getId();
            m_avatar_flag = false;
        }
    } else if (m_juncture_flag) {
        std::cout << "Juncture create success" << std::endl << std::flush;
        if (!ent->hasAttrFlag(Atlas::Objects::ID_FLAG)) {
            std::cerr << "ERROR: Response to juncture create does not contain agent id"
                      << std::endl << std::flush;
            
        } else {
            m_juncture_id = ent->getId();
            m_juncture_flag = false;
        }
    } else if (m_server_flag) {
        std::cout << "Server query success" << std::endl << std::flush;
        if (!ent->isDefaultName()) {
            m_serverName = ent->getName();
            std::string::size_type p = m_serverName.find(".");
            if (p != std::string::npos) {
                m_serverName = m_serverName.substr(0, p);
            }
            updatePrompt();
        }
        Element raw_attr;
        if (ent->copyAttr("server", raw_attr) == 0) {
            if (raw_attr.isString()) {
                m_systemType = raw_attr.String();
                updatePrompt();
            }
        }
        m_server_flag = false;
    } else if (m_currentTask == 0) {
        AtlasStreamClient::infoArrived(op);
        std::cout << "Info(" << std::endl;
        MapType entmap = ent->asMessage();
        MapType::const_iterator Iend = entmap.end();
        for (MapType::const_iterator I = entmap.begin(); I != Iend; ++I) {
            const Element & item = I->second;
            std::cout << std::string(spacing(), ' ') << I->first << ": ";
            output(item, 1);
            std::cout << std::endl;
        }
        std::cout << ")" << std::endl << std::flush;
        // Display results of command
    }
}

void Interactive::errorArrived(const Operation & op)
{
    reply_flag = true;
    error_flag = true;
    
    if (m_currentTask != 0) {
        return;
    }
    std::cout << "Error(";
    const std::vector<Root> & args = op->getArgs();
    const Root & arg = args.front();
    Element message_attr;
    if (arg->copyAttr("message", message_attr) == 0 && message_attr.isString()) {
        std::cout << message_attr.asString();
    }
    std::cout << ")" << std::endl << std::flush;
}

void Interactive::sightArrived(const Operation & op)
{
    if (accountId.empty()) {
        return;
    }
    if (accountId != op->getTo() && m_agentId != op->getTo()) {
        // This is an IG op we are monitoring
        return;
    }
    reply_flag = true;
    if (m_currentTask != 0) {
        return;
    }
    std::cout << "Sight(" << std::endl;
    const MapType & ent = op->getArgs().front()->asMessage();
    MapType::const_iterator Iend = ent.end();
    for (MapType::const_iterator I = ent.begin(); I != Iend; ++I) {
        const Element & item = I->second;
        std::cout << "      " << I->first << ":";
        output(item, 1);
        std::cout << std::endl;
    }
    std::cout << ")" << std::endl << std::flush;
}

void Interactive::soundArrived(const Operation & op)
{
    if (accountId.empty()) {
        return;
    }
    if (accountId != op->getTo()) {
        // This is an IG op we are monitoring
        return;
    }
    reply_flag = true;
    if (op->getArgs().empty()) {
        std::cout << "Sound op has no args" << std::endl << std::flush;
        return;
    }
    Operation sub_op = smart_dynamic_cast<Operation>(op->getArgs().front());
    if (!sub_op.isValid()) {
        return;
    }
    if (sub_op->isDefaultFrom()) {
        std::cout << "Sound arg has no from" << std::endl << std::flush;
        return;
    }
    const std::string & from = sub_op->getFrom();
    if (sub_op->getArgs().empty()) {
        std::cout << "Sound arg has no args" << std::endl << std::flush;
        return;
    }
    const Root & arg = sub_op->getArgs().front();
    Element say;
    if (arg->copyAttr("say", say) != 0 || !say.isString()) {
        std::cout << "Sound arg arg has no say" << std::endl << std::flush;
        return;
    }
    std::cout << "[" << from << "] " << say.String()
              << std::endl << std::flush;
}

void Interactive::loginSuccess(const Atlas::Objects::Root & arg)
{
    const std::string & id = arg->getId();
    m_contexts.insert(std::make_pair(id, new AccountContext(m_username)));
}

sigc::signal<void, char *> CmdLine;
sigc::signal<void, int, int> ContextSwitch;

void Interactive::gotCommand(char * cmd)
{
    CmdLine.emit(cmd);
}

void Interactive::runCommand(char * cmd)
{
    if (cmd == NULL) {
        m_exit_flag = true;
        std::cout << std::endl << std::flush;
        return;
    }

    if (*cmd == 0) {
        free(cmd);
        return;
    }

    add_history(cmd);

    char * arg = strchr(cmd, ' ');
    if (arg != NULL) {
        *arg++ = 0;
        int len = strlen(arg);
        while (len > 0 && arg[--len] == ' ') { arg[len] = 0; }
    } else {
        arg = (char *)"";
    }

    for (struct command * I = &commands[0]; I->cmd_string != NULL; ++I) {
        if (strcmp(cmd, I->cmd_string) == 0) {
            (this->*(I->cmd_method))(I, arg);
        }
    }

    exec(cmd, arg);
}

void Interactive::switchContext(int, int)
{
    updatePrompt();
}

int completion_iterator = 0;

char * completion_generator(const char * text, int state)
{
    if (state == 0) {
        completion_iterator = 0;
    }
    for (int i = completion_iterator; commands[i].cmd_string != 0; ++i) {
        if (strncmp(text, commands[i].cmd_string, strlen(text)) == 0) {
            completion_iterator = i + 1;
            return strdup(commands[i].cmd_string);
        }
    }
    return 0;
}

static int context_switch(int a, int b)
{
    ContextSwitch.emit(a, b);
    return 0;
}

void Interactive::loop()
{
    if (rl_bind_keyseq("`", &context_switch) != 0) {
        std::cout << "BINDING FALED" << std::endl;
    }
    rl_callback_handler_install(m_prompt.c_str(),
                                &Interactive::gotCommand);
    rl_completion_entry_function = &completion_generator;
    CmdLine.connect(sigc::mem_fun(this, &Interactive::runCommand));
    ContextSwitch.connect(sigc::mem_fun(this, &Interactive::switchContext));
    while (!m_exit_flag) {
        select();
    };
    std::cout << std::endl << std::flush;
    rl_callback_handler_remove();
}

void Interactive::select(bool rewrite_prompt)
// poll the codec if select says there is something there.
{
    fd_set infds;
    struct timeval tv;
    int retval;

    FD_ZERO(&infds);

    FD_SET(m_fd, &infds);
    FD_SET(STDIN_FILENO, &infds);

    tv.tv_sec = 0;
    tv.tv_usec = 500000;

    if (rewrite_prompt) {
        retval = ::select(m_fd+1, &infds, NULL, NULL, NULL);
    } else {
        retval = ::select(m_fd+1, &infds, NULL, NULL, &tv);
    }

    if (retval > 0) {
        if (FD_ISSET(m_fd, &infds)) {
            if (m_ios->peek() == -1) {
                std::cout << "Server disconnected" << std::endl << std::flush;
                m_exit_flag = true;
            } else {
                if (rewrite_prompt) {
                    std::cout << std::endl;
                }
                m_codec->poll();
                if (rewrite_prompt) {
                    updatePrompt();
                    rl_forced_update_display();
                }
            }
        }
        if (FD_ISSET(STDIN_FILENO, &infds)) {
            rl_callback_read_char();
        }
    }
}

void Interactive::updatePrompt()
{
    std::string designation(">");
    if (accountType == "admin" || accountType == "sys") {
        designation = "#";
    } else {
        designation = "$";
    }
    std::string status = "idle";
    if (m_currentTask != 0) {
        status = m_currentTask->description();
    }
    m_prompt = String::compose("[%1@%2 %3{%4}]%5 ", m_username, m_serverName,
                             m_systemType, status, designation);
    rl_set_prompt(m_prompt.c_str());
}

int Interactive::setup()
{
    Get get;

    send(get);

    m_server_flag = true;

    reply_flag = true;
    while (m_server_flag && !error_flag) {
       m_codec->poll();
    }

    m_server_flag = false;
    if (!error_flag) {
       return 0;
    }
    return -1;
}

void Interactive::exec(const std::string & cmd, const std::string & arg)
{
    bool reply_expected = true;
    reply_flag = false;
    error_flag = false;

    if (cmd == "stat") {
        Get g;
        send(g);
    } else if (cmd == "install") {
        size_t space = arg.find(' ');
        if (space == std::string::npos || space >= (arg.size() - 1)) {
            std::cout << "usage: install <type id> <parent id>"
                      << std::endl << std::flush;
        } else {
            Create c;
            c->setFrom(accountId);
            Anonymous ent;
            ent->setId(std::string(arg, 0, space));
            ent->setObjtype("class");
            ent->setParents(std::list<std::string>(1, std::string(arg, space + 1)));
            c->setArgs1(ent);
            send(c);
        }
        reply_expected = false;
    } else if (cmd == "look") {
        Look l;
        if (!arg.empty()) {
            Anonymous cmap;
            cmap->setId(arg);
            l->setArgs1(cmap);
        }
        l->setFrom(accountId);
        send(l);
    } else if (cmd == "logout") {
        Logout l;
        l->setFrom(accountId);
        if (!arg.empty()) {
            Anonymous lmap;
            lmap->setId(arg);
            l->setArgs1(lmap);
            reply_expected = false;
        }
        send(l);
    } else if (cmd == "say") {
        Talk t;
        Anonymous ent;
        ent->setAttr("say", arg);
        t->setArgs1(ent);
        t->setFrom(accountId);
        send(t);
    } else if (cmd == "help" || cmd == "?") {
        reply_expected = false;
        help();
    } else if (cmd == "query") {
        Get g;

        if (!arg.empty()) {
            Anonymous cmap;
            if (::isdigit(arg[0])) {
                cmap->setObjtype("obj");
            } else {
                cmap->setObjtype("meta");
            }
            cmap->setId(arg);
            g->setArgs1(cmap);
        }
        g->setFrom(accountId);

        send(g);
    } else if (cmd == "reload") {
        if (arg.empty()) {
            reply_expected = false;
            std::cout << "reload: Argument required" << std::endl << std::flush;
        } else {
            Set s;

            Anonymous tmap;
            tmap->setObjtype("class");
            tmap->setId(arg);
            s->setArgs1(tmap);
            s->setFrom(accountId);

            send(s);
        }
    } else if (cmd == "get") {
        Get g;

        if (!arg.empty()) {
            Anonymous cmap;
            if (::isdigit(arg[0])) {
                cmap->setObjtype("obj");
            } else {
                cmap->setObjtype("meta");
            }
            cmap->setId(arg);
            g->setArgs1(cmap);
        }
        g->setFrom(accountId);

        send(g);
    } else if (cmd == "monitor") {
        ClientTask * task = new OperationMonitor;
        if (runTask(task, arg) == 0) {
            Monitor m;

            m->setArgs1(Anonymous());
            m->setFrom(accountId);

            send(m);
        }

        reply_expected = false;
    } else if (cmd == "unmonitor") {
        OperationMonitor * om = dynamic_cast<OperationMonitor *>(m_currentTask);

        if (om != 0) {
            Monitor m;

            m->setFrom(accountId);

            send(m);

            reply_expected = false;

            SystemTime now;
            now.update();

            time_t monitor_time = now.seconds() - om->startTime();

            std::cout << om->count() << " operations monitored in "
                      << monitor_time << " seconds = "
                      << om->count() / monitor_time
                      << " operations per second"
                      << std::endl << std::flush;

            endTask();
        }
    } else if (cmd == "connect") {
        reply_expected = false;
        if (m_juncture_id.empty()) {
            std::cout << "Use create juncture to create a juncture object "
                         "on the server"
                      << std::endl << std::flush;
        } else {

            std::vector<std::string> args;
            tokenize(arg, args);

            if (args.size() != 2) {
                std::cout << "usage: connect <hostname> <port>"
                          << std::endl << std::flush;
            } else {
                Anonymous cmap;
                cmap->setAttr("hostname", args[0]);
                cmap->setAttr("port", strtol(args[1].c_str(), 0, 10));
                // cmap->setAttr("username", args[2]);
                // cmap->setAttr("password", args[3]);

                Connect m;
                m->setArgs1(cmap);
                m->setFrom(m_juncture_id);

                send(m);
            }
        }
    } else if (cmd == "add_agent") {
        std::string agent_type("creator");

        if (!arg.empty()) {
            agent_type = arg;
        }
        
        Create c;

        Anonymous cmap;
        cmap->setParents(std::list<std::string>(1, agent_type));
        cmap->setName("cycmd agent");
        cmap->setObjtype("obj");
        c->setArgs1(cmap);
        c->setFrom(accountId);

        m_avatar_flag = true;

        send(c);
    } else if (cmd == "delete") {
        if (m_agentId.empty()) {
            std::cout << "Use add_agent to add an in-game agent first" << std::endl << std::flush;
            reply_expected = false;
        } else if (arg.empty()) {
            std::cout << "Please specify the entity to delete" << std::endl << std::flush;
            reply_expected = false;
        } else {
            Delete del;

            Anonymous del_arg;
            del_arg->setId(arg);
            del->setArgs1(del_arg);
            del->setFrom(m_agentId);
            del->setTo(arg);

            send(del);

            reply_expected = false;
        }
    } else if (cmd == "find_by_name") {
        if (m_agentId.empty()) {
            std::cout << "Use add_agent to add an in-game agent first" << std::endl << std::flush;
            reply_expected = false;
        } else if (arg.empty()) {
            std::cout << "Please specify the name to search for" << std::endl << std::flush;
            reply_expected = false;
        } else {
            Look l;

            Anonymous lmap;
            lmap->setName(arg);
            l->setArgs1(lmap);
            l->setFrom(m_agentId);

            send(l);

            reply_expected = false;
        }
    } else if (cmd == "find_by_type") {
        if (m_agentId.empty()) {
            std::cout << "Use add_agent to add an in-game agent first" << std::endl << std::flush;
            reply_expected = false;
        } else if (arg.empty()) {
            std::cout << "Please specify the type to search for" << std::endl << std::flush;
            reply_expected = false;
        } else {
            Look l;

            Anonymous lmap;
            lmap->setParents(std::list<std::string>(1, arg));
            l->setArgs1(lmap);
            l->setFrom(m_agentId);

            send(l);

            reply_expected = false;
        }
    } else if (cmd == "flush") {
        if (m_agentId.empty()) {
            std::cout << "Use add_agent to add an in-game agent first" << std::endl << std::flush;
            reply_expected = false;
        } else if (arg.empty()) {
            std::cout << "Please specify the type to flush" << std::endl << std::flush;
            reply_expected = false;
        } else {
            ClientTask * task = new Flusher(m_agentId);
            runTask(task, arg);
            reply_expected = false;
        }
    } else if (cmd == "creator_create") {
        if (m_agentId.empty()) {
            std::cout << "Use add_agent to add an in-game agent first" << std::endl << std::flush;
            reply_expected = false;
        } else if (arg.empty()) {
            std::cout << "Please specify the type to create" << std::endl << std::flush;
            reply_expected = false;
        } else {
            Create c;

            Anonymous thing;
            thing->setParents(std::list<std::string>(1, arg));
            c->setArgs1(thing);
            c->setFrom(m_agentId);

            send(c);

            reply_expected = false;
        }
    } else if (cmd == "creator_look") {
        if (m_agentId.empty()) {
            std::cout << "Use add_agent to add an in-game agent first" << std::endl << std::flush;
            reply_expected = false;
        } else {
            Look l;

            if (!arg.empty()) {
                Anonymous cmap;
                cmap->setId(arg);
                l->setArgs1(cmap);
            }
            l->setFrom(m_agentId);

            send(l);
            reply_expected = true;
        }
    } else if (cmd == "cancel") {
        if (endTask() != 0) {
            std::cout << "No task currently running" << std::endl << std::flush;
        }
    } else if (cmd == "dump") {
        ClientTask * task = new WorldDumper(accountId);
        runTask(task, arg);
        reply_expected = false;
    } else if (cmd == "restore") {
        if (m_agentId.empty()) {
            std::cout << "Use add_agent to add an in-game agent first" << std::endl << std::flush;
            reply_expected = false;
        } else {
            ClientTask * task = new WorldLoader(accountId, m_agentId);
            runTask(task, arg);
            reply_expected = false;
        }
    } else if (cmd == "create") {
        std::vector<std::string> args;
        tokenize(arg, args);

        if (args.size() < 1) {
            std::cout << "usage: create <type> <params> ... "
                      << std::endl << std::flush;
            reply_expected = false;
        } else {
            Anonymous cmap;
            cmap->setParents(std::list<std::string>(1, args[0]));
            cmap->setObjtype("obj");

            Create c;
            c->setArgs1(cmap);
            c->setFrom(accountId);

            m_juncture_flag = true;

            send(c);
        }
    } else if (cmd == "login") {
        reply_expected = false;
        if (m_juncture_id.empty()) {
            std::cout << "Use create juncture to create a juncture object "
                         "on the server"
                      << std::endl << std::flush;
        } else {

            std::vector<std::string> args;
            tokenize(arg, args);

            if (args.size() != 2) {
                std::cout << "usage: login <username> <password>"
                          << std::endl << std::flush;
            } else {
                Anonymous cmap;
                cmap->setAttr("username", args[0]);
                cmap->setAttr("password", args[1]);

                Login m;
                m->setArgs1(cmap);
                m->setFrom(m_juncture_id);
                m->setSerialno(newSerialNo());

                send(m);
            }
        }
    } else {
        reply_expected = false;
        std::cout << cmd << ": Command not known" << std::endl << std::flush;
    }

    if (!reply_expected) {
        updatePrompt();
        return;
    }
    // Wait for reply
    time_t wait_start_time = time(NULL);
    while (!reply_flag) {
       if (time(NULL) - wait_start_time > 5) {
           std::cout << cmd << ": No reply from server" << std::endl << std::flush;
           return;
       }
       select(false);
    }
}

int Interactive::commandUnknown(struct command * cmd,
                                const std::string & arg)
{
    return 0;
}
