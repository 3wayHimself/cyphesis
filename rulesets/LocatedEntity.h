// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2000-2007 Alistair Riddoch
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

#ifndef RULESETS_LOCATED_ENTITY_H
#define RULESETS_LOCATED_ENTITY_H

#include "modules/Location.h"

#include "common/Router.h"

#include <set>

#include <cassert>

class LocatedEntity;
class PropertyBase;
class Script;
class TypeNode;

template <typename T>
class Property;

typedef std::set<LocatedEntity *> LocatedEntitySet;
typedef std::map<std::string, PropertyBase *> PropertyDict;

/// \brief This is the base class from which in-game and in-memory objects
/// inherit.
///
/// This class should not normally be instantiated directly.
/// This class provides hard-coded attributes that are common to all
/// in-game objects, and objects in the memory on an NPC.
/// It provides a physical location for the entity, and a contains list
/// which lists other entities which specify their location with reference to
/// this one. It also provides the script interface for handling operations
/// in scripts rather than in the C++ code.
class LocatedEntity : public Router {
  private:
    static std::set<std::string> m_immutable;
    static const std::set<std::string> & immutables();

    /// Count of references held by other objects to this entity
    int m_refCount;
  protected:
    /// Map of properties
    PropertyDict m_properties;

    /// Sequence number
    int m_seq;

    /// Script associated with this entity
    Script * m_script;
    /// Class of which this is an instance
    const TypeNode * m_type;

  public:
    /// Full details of location
    Location m_location;
    /// List of entities which use this as ref
    LocatedEntitySet * m_contains;

    explicit LocatedEntity(const std::string & id, long intId);
    virtual ~LocatedEntity();

    /// \brief Increment the reference count on this entity
    void incRef() {
        ++m_refCount;
    }

    /// \brief Decrement the reference count on this entity
    void decRef() {
        if (m_refCount <= 0) {
            assert(m_refCount == 0);
            delete this;
        } else {
            --m_refCount;
        }
    }

    /// \brief Check the reference count on this entity
    int checkRef() const {
        return m_refCount;
    }

    /// \brief Accessor for pointer to script object
    Script * script() const {
        return m_script;
    }

    /// \brief Accessor for sequence number
    const int getSeq() const { return m_seq; }
    /// \brief Accessor for entity type property
    const TypeNode * getType() const { return m_type; }
    /// \brief Accessor for properies
    const PropertyDict & getProperties() const { return m_properties; }

    /// \brief Set the value of the entity type property
    void setType(const TypeNode * t) {
        m_type = t;
    }

    virtual bool hasAttr(const std::string & name) const;
    virtual int getAttr(const std::string & name,
                        Atlas::Message::Element &) const;
    virtual int getAttrType(const std::string & name,
                            Atlas::Message::Element &,
                            int type) const;
    virtual PropertyBase* setAttr(const std::string & name,
                                  const Atlas::Message::Element &);
    virtual const PropertyBase * getProperty(const std::string & name) const;

    virtual void onContainered();
    virtual void onUpdated();

    void setScript(Script * scrpt);
    void makeContainer();
    void changeContainer(LocatedEntity *);
    void merge(const Atlas::Message::MapType &);

    /// \brief Get a property that is required to of a given type.
    template <class PropertyT>
    const PropertyT * getPropertyClass(const std::string & name) const
    {
        const PropertyBase * p = getProperty(name);
        if (p != 0) {
            return dynamic_cast<const PropertyT *>(p);
        }
        return 0;
    }

    /// \brief Get a property that is a generic property of a given type
    template <typename T>
    const Property<T> * getPropertyType(const std::string & name) const
    {
        const PropertyBase * p = getProperty(name);
        if (p != 0) {
            return dynamic_cast<const Property<T> *>(p);
        }
        return 0;
    }
};

#endif // RULESETS_LOCATED_ENTITY_H
