// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2001 Alistair Riddoch

#include <Atlas/Objects/Decoder.h>
#include <Atlas/Codecs/XML.h>

#include <common/Database.h>
#include <common/globals.h>

#include <string>
#include <fstream>

using Atlas::Message::Object;

class RuleBase {
  protected:
    RuleBase() : m_connection(*Database::instance()) { }

    Database & m_connection;
    static RuleBase * m_instance;
  public:
    ~RuleBase() {
        m_connection.shutdownConnection();
    }

    static RuleBase * instance() {
        if (m_instance == NULL) {
            m_instance = new RuleBase();
            m_instance->m_connection.initConnection(true);
            m_instance->m_connection.initRule(true);
        }
        return m_instance;
    }

    void storeInRules(const Object::MapType & o, const std::string & key) {
        m_connection.putObject(m_connection.rule(), key, o);
    }
    bool clearRules() {
        return m_connection.clearTable(m_connection.rule());
    }
};

RuleBase * RuleBase::m_instance = NULL;

class FileDecoder : public Atlas::Message::DecoderBase {
    std::fstream m_file;
    RuleBase & m_db;
    Atlas::Codecs::XML m_codec;
    int m_count;

    virtual void ObjectArrived(const Object & obj) {
        const Object::MapType & omap = obj.AsMap();
        Object::MapType::const_iterator I;
        for (I = omap.begin(); I != omap.end(); ++I) {
            m_count++;
            m_db.storeInRules(I->second.AsMap(), I->first);
        }
    }
  public:
    FileDecoder(const std::string & filename, RuleBase & db) :
                m_file(filename.c_str(), std::ios::in), m_db(db),
                m_codec(m_file, this), m_count(0)
    {
    }

    void read() {
        while (!m_file.eof()) {
            m_codec.Poll();
        }
    }

    void report() {
        std::cout << m_count << " classes stored in rule database."
                  << std::endl << std::flush;
    }

    bool isOpen() {
        return m_file.is_open();
    }
};

static void usage(char * prgname)
{
    std::cout << "usage: " << prgname << " <atlas map file>" << std::endl << std::flush;
    return;
}

int main(int argc, char ** argv)
{
    if (argc > 2) {
        usage(argv[0]);
        return 1;
    }

    int cargc = 0;
    char * cargv[0];

    if (loadConfig(cargc, cargv)) {
        // Fatal error loading config file
        return 1;
    }

    RuleBase & db = *RuleBase::instance();

    if (argc == 2) {
        FileDecoder f(argv[1], db);
        if (!f.isOpen()) {
            std::cerr << "ERROR: Unable to open file " << argv[1]
                      << std::endl << std::flush;
            return 1;
        }
        f.read();
        f.report();
    } else {
        db.clearRules();
        std::vector<std::string>::iterator I = rulesets.begin();
        for (; I != rulesets.end(); ++I) {
            std::cout << "Reading rules from " << *I << std::endl << std::flush;
            FileDecoder f(etc_directory + "/cyphesis/" + *I + ".xml", db);
            if (!f.isOpen()) {
                std::cerr << "ERROR: Unable to open file " << argv[1]
                          << std::endl << std::flush;
                return 1;
            }
            f.read();
            f.report();
        }
    }

    delete &db;
}
