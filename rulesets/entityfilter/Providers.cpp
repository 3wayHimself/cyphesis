/*
 Copyright (C) 2014 Erik Ogenvik

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Providers.h"

#include "../OutfitProperty.h"
#include "../BBoxProperty.h"
#include "../EntityProperty.h"
#include "../../common/TypeNode.h"
#include "../../common/compose.hpp"
#include "../../common/Inheritance.h"

#include <algorithm>

namespace EntityFilter
{


FixedElementProvider::FixedElementProvider(const Atlas::Message::Element& element)
: m_element(element)
{
}

void FixedElementProvider::value(Atlas::Message::Element& value, const QueryContext& context) const
{
    value = m_element;
}

FixedTypeNodeProvider::FixedTypeNodeProvider(Consumer<TypeNode>* consumer, const TypeNode& type)
: ConsumingProviderBase<TypeNode, QueryContext>(consumer), m_type(type)
{
}

void FixedTypeNodeProvider::value(Atlas::Message::Element& value, const QueryContext& context) const
{
    if (m_consumer) {
        m_consumer->value(value, m_type);
    } else {
        value = (void*)(&m_type);
    }
}

const std::type_info* FixedTypeNodeProvider::getType() const
{
    if (m_consumer) {
        return m_consumer->getType();
    } else {
        return &typeid(const TypeNode*);
    }
}

MemoryProvider::MemoryProvider(Consumer<Atlas::Message::Element>* consumer)
:ConsumingProviderBase<Atlas::Message::Element, MindQueryContext>(consumer){

}

void MemoryProvider::value(Atlas::Message::Element& value, const MindQueryContext& context) const
{
    if(m_consumer){
        auto& ent = context.entity;
        auto& mem = context.memory;
        auto& ent_id = ent.getId();
        const auto& iter = mem.find(ent.getId());
        if (iter != mem.end()){
            m_consumer->value(value, iter->second);
            return;
        }
    }
    value = Atlas::Message::Element();
}

EntityProvider::EntityProvider(Consumer<LocatedEntity>* consumer)
: ConsumingProviderBase<LocatedEntity, QueryContext>(consumer)
{
}

void EntityProvider::value(Atlas::Message::Element& value, const QueryContext& context) const
{
    if (m_consumer) {
        m_consumer->value(value, context.entity);
    } else {
        value = (void*)(&context.entity);
    }
}

const std::type_info* EntityProvider::getType() const
{
    if (m_consumer) {
        return m_consumer->getType();
    } else {
        return &typeid(const LocatedEntity*);
    }
}

EntityTypeProvider::EntityTypeProvider(Consumer<TypeNode>* consumer)
: ConsumingProviderBase<TypeNode, LocatedEntity>(consumer)
{

}

void EntityTypeProvider::value(Atlas::Message::Element& value, const LocatedEntity& entity) const
{
    if (!entity.getType()) {
        return;
    }

    if (m_consumer) {
        m_consumer->value(value, *entity.getType());
    } else {
        value = (void*)(entity.getType());
    }
}

const std::type_info* EntityTypeProvider::getType() const
{
    if (m_consumer) {
        return m_consumer->getType();
    } else {
        return &typeid(const TypeNode*);
    }
}

TypeNodeProvider::TypeNodeProvider(const std::string& attribute_name)
: m_attribute_name(attribute_name)
{

}

void TypeNodeProvider::value(Atlas::Message::Element& value, const TypeNode& type) const
{
    if (m_attribute_name == "name") {
        value = type.name();
    }
}

OutfitEntityProvider::OutfitEntityProvider(Consumer<LocatedEntity>* consumer, const std::string& attribute_name)
: ConsumingNamedAttributeProviderBase<LocatedEntity, OutfitProperty>(consumer, attribute_name)
{

}

void OutfitEntityProvider::value(Atlas::Message::Element& value, const OutfitProperty& prop) const
{
    auto outfit_entity = prop.getEntity(m_attribute_name);
    if (!outfit_entity) {
        return;
    }

    if (m_consumer) {
        m_consumer->value(value, *outfit_entity);
    } else {
        value = (void*)&outfit_entity;
    }
}

BBoxProvider::BBoxProvider(Consumer<Atlas::Message::Element>* consumer, Measurement measurement)
: ConsumingProviderBase<Atlas::Message::Element, BBoxProperty>(consumer), m_measurement(measurement)
{

}

void BBoxProvider::value(Atlas::Message::Element& value, const BBoxProperty& prop) const
{
    const BBox& bbox = prop.data();
    switch (m_measurement) {
    case Measurement::WIDTH:
        value = bbox.highCorner().x() - bbox.lowCorner().x();
        break;
    case Measurement::DEPTH:
        value = bbox.highCorner().y() - bbox.lowCorner().y();
        break;
    case Measurement::HEIGHT:
        value = bbox.highCorner().z() - bbox.lowCorner().z();
        break;
    case Measurement::VOLUME:
        value = (bbox.highCorner().x() - bbox.lowCorner().x()) * (bbox.highCorner().y() - bbox.lowCorner().y()) * (bbox.highCorner().z() - bbox.lowCorner().z());
        break;
    case Measurement::AREA:
        value = (bbox.highCorner().x() - bbox.lowCorner().x()) * (bbox.highCorner().y() - bbox.lowCorner().y());
        break;
    }
}



SoftPropertyProvider::SoftPropertyProvider(Consumer<Atlas::Message::Element>* consumer, const std::string& attribute_name) :
    ConsumingNamedAttributeProviderBase<Atlas::Message::Element, LocatedEntity>(consumer, attribute_name)
{
}

void SoftPropertyProvider::value(Atlas::Message::Element& value, const LocatedEntity& entity) const
{
    auto prop = entity.getProperty(m_attribute_name);
    if (!prop) {
        return;
    }
    if (m_consumer) {
        Atlas::Message::Element propElem;
        prop->get(propElem);
        m_consumer->value(value, propElem);
    } else {
        prop->get(value);
    }
}

MapProvider::MapProvider(Consumer<Atlas::Message::Element>* consumer, const std::string& attribute_name) :
        ConsumingNamedAttributeProviderBase<Atlas::Message::Element, Atlas::Message::Element>(consumer, attribute_name)
{
}

void MapProvider::value(Atlas::Message::Element& value, const Atlas::Message::Element& parent_element) const
{
    if (!parent_element.isMap()) {
        return;
    }
    auto I = parent_element.Map().find(m_attribute_name);
    if (I == parent_element.Map().end()) {
        return;
    }
    if (m_consumer) {
        m_consumer->value(value, I->second);
    } else {
        value = I->second;
    }
}

EntityRefProvider::EntityRefProvider(Consumer<LocatedEntity>* consumer, const std::string& attribute_name):
        ConsumingNamedAttributeProviderBase<LocatedEntity, LocatedEntity>(consumer, attribute_name)
{

}

void EntityRefProvider::value(Atlas::Message::Element& value, const LocatedEntity& entity) const
{
    const EntityProperty* prop = entity.getPropertyClass<EntityProperty>(m_attribute_name);
    if (!prop) {
        return;
    }

    const auto referenced_entity = prop->data().get();
    if (!referenced_entity) {
        return;
    }
    if (m_consumer) {
        return m_consumer->value(value, *referenced_entity);
    } else {
        value = referenced_entity;
    }
}

const std::type_info* EntityRefProvider::getType() const
{
    if (m_consumer) {
        return m_consumer->getType();
    } else {
        return &typeid(const LocatedEntity*);
    }
}

Consumer<QueryContext>* ProviderFactory::createProviders(SegmentsList segments) const
{
    if (!segments.empty()) {
        auto& first_attribute = segments.front().attribute;
        if (first_attribute == "entity") {
            return createEntityProvider(segments);
        } else if (first_attribute == "types") {
            return createFixedTypeNodeProvider(segments);
        }
    }
    return nullptr;
}

FixedTypeNodeProvider* ProviderFactory::createFixedTypeNodeProvider(SegmentsList segments) const
{
    if (segments.empty()) {
        return nullptr;
    }
    segments.pop_front();
    //A little hack here to avoid calling yet another method.
    if (segments.empty()) {
        return nullptr;
    }
    const TypeNode* typeNode = Inheritance::instance().getType(segments.front().attribute);
    if (!typeNode) {
        return nullptr;
    }
    segments.pop_front();
    return new FixedTypeNodeProvider(createTypeNodeProvider(segments), *typeNode);
}

EntityProvider* ProviderFactory::createEntityProvider(SegmentsList segments) const
{
    if (segments.empty()) {
        return nullptr;
    }
    segments.pop_front();
    return new EntityProvider(createPropertyProvider(segments));
}

Consumer<LocatedEntity>* ProviderFactory::createPropertyProvider(SegmentsList segments) const
{
    if (segments.empty()) {
        return nullptr;
    }

    auto& segment = segments.front();
    auto attr = segment.attribute;

    segments.pop_front();

    if (segment.delimiter == ":") {
        return new SoftPropertyProvider(createMapProvider(segments), attr);
    } else {

        if (attr == "type") {
            return new EntityTypeProvider(createTypeNodeProvider(segments));
        } else if (attr == "outfit") {
            return new PropertyProvider<OutfitProperty>(createOutfitEntityProvider(segments), attr);
        } else if (attr == "bbox") {
            return new PropertyProvider<BBoxProperty>(createBBoxProvider(segments), attr);
        } else if (attr == "right_hand_wield") {
            return new EntityRefProvider(createPropertyProvider(segments), attr);
        } else {
            return new SoftPropertyProvider(createMapProvider(segments), attr);
        }
    }
}


OutfitEntityProvider* ProviderFactory::createOutfitEntityProvider(SegmentsList segments) const
{
    if (segments.empty()) {
        return nullptr;
    }

    auto& segment = segments.front();
    auto attr = segment.attribute;

    segments.pop_front();

    return new OutfitEntityProvider(createPropertyProvider(segments), attr);

}

BBoxProvider* ProviderFactory::createBBoxProvider(SegmentsList segments) const
{
    if (segments.empty()) {
        return nullptr;
    }

    auto& segment = segments.front();
    auto attr = segment.attribute;

    auto measurement_extractor = [&]() -> BBoxProvider::Measurement {
        if (attr == "width") {
            return BBoxProvider::Measurement::WIDTH;
        } else if (attr == "depth") {
            return BBoxProvider::Measurement::DEPTH;
        } else if (attr == "height") {
            return BBoxProvider::Measurement::HEIGHT;
        } else if (attr == "volume") {
            return BBoxProvider::Measurement::VOLUME;
        } else if (attr == "area") {
            return BBoxProvider::Measurement::AREA;
        }
        throw std::invalid_argument(String::compose("Could not compile query as '%1' isn't a valid measurement for a Bounding Box.", attr));
    };

    segments.pop_front();

    return new BBoxProvider(createMapProvider(segments), measurement_extractor());

}

MapProvider* ProviderFactory::createMapProvider(SegmentsList segments) const
{
    if (segments.empty()) {
        return nullptr;
    }

    auto& segment = segments.front();
    auto attr = segment.attribute;

    segments.pop_front();

    return new MapProvider(createMapProvider(segments), attr);
}

TypeNodeProvider* ProviderFactory::createTypeNodeProvider(SegmentsList segments) const
{
    if (segments.empty()) {
        return nullptr;
    }

    auto& segment = segments.front();
    auto attr = segment.attribute;

    return new TypeNodeProvider( attr);
}

ComparePredicate::ComparePredicate(const Consumer<QueryContext>* lhs, const Consumer<QueryContext>* rhs, Comparator comparator)
: m_lhs(lhs), m_rhs(rhs), m_comparator(comparator)
{
    if (m_comparator == Comparator::INSTANCE_OF) {
        //make sure rhs and lhs exist
        if(!m_lhs || !m_rhs){
            throw std::invalid_argument("One of the types for 'instanceof' operator doesn't exist");
        }
        //make sure that both providers return TypeNode intances
        if ((m_lhs->getType() !=  &typeid(const TypeNode*)) || (m_rhs->getType() != &typeid(const TypeNode*))) {
            throw std::invalid_argument("When using the 'instanceof' comparator, both statements must return a TypeNode. For example, 'entity.type == types.world'.");
        }
    }

}



bool ComparePredicate::isMatch(const QueryContext& context) const
{
    switch (m_comparator) {
    case Comparator::EQUALS:
    {
        Atlas::Message::Element left, right;
        m_lhs->value(left, context);
        if (!left.isNone()) {
            m_rhs->value(right, context);
            if (!right.isNone()) {
                return left == right;
            }
        }

        return false;
    }
        break;
    case Comparator::NOT_EQUALS:
    {
        Atlas::Message::Element left, right;
        m_lhs->value(left, context);
        if (!left.isNone()) {
            m_rhs->value(right, context);
            if (!right.isNone()) {
                return left != right;
            }
        }

        return true;
    }
        break;
    case Comparator::LESS:
    {
        Atlas::Message::Element left, right;
        m_lhs->value(left, context);
        if (left.isNum()) {
            m_rhs->value(right, context);
            if (right.isNum()) {
                return left.asNum() < right.asNum();
            }
        }
        return false;
    }
    break;
    case Comparator::LESS_EQUAL:
    {
        Atlas::Message::Element left, right;
        m_lhs->value(left, context);
        if (left.isNum()) {
            m_rhs->value(right, context);
            if (right.isNum()) {
                return left.asNum() <= right.asNum();
            }
        }
        return false;
    }
        break;
    case Comparator::GREATER:
    {
        Atlas::Message::Element left, right;
        m_lhs->value(left, context);
        if (left.isNum()) {
            m_rhs->value(right, context);
            if (right.isNum()) {
                return left.asNum() > right.asNum();
            }
        }
        return false;
    }
        break;
    case Comparator::GREATER_EQUAL:
    {
        Atlas::Message::Element left, right;
        m_lhs->value(left, context);
        if (left.isNum()) {
            m_rhs->value(right, context);
            if (right.isNum()) {
                return left.asNum() >= right.asNum();
            }
        }
        return false;
    }
        break;
    case Comparator::INSTANCE_OF:
    {
        //We know that both providers return type node instances, since we checked in the constructor.
        Atlas::Message::Element left, right;
        m_lhs->value(left, context);
        if (left.isPtr()) {
            const TypeNode* leftType = static_cast<const TypeNode*>(left.Ptr());
            if (leftType) {
                m_rhs->value(right, context);
                if (right.isPtr()) {
                    const TypeNode* rightType = static_cast<const TypeNode*>(right.Ptr());
                    if (rightType) {
                        return rightType->isTypeOf(leftType);
                    }
                }
            }
        }
        return false;
    }
    case Comparator::IN:
    {
        Atlas::Message::Element left, right;
        m_lhs->value(left, context);
        if (!left.isNone()) {
            m_rhs->value(right, context);
            if (right.isList()) {
                const auto& right_end = right.List().end();
                const auto& right_begin = right.List().begin();
                return std::find(right_begin, right_end, left) != right_end;
            }
        }
        return false;
    }
    case Comparator::CONTAINS:
    {
        Atlas::Message::Element left, right;
        m_lhs->value(left, context);
        if(left.isList()){
            m_rhs->value(right, context);
            if(!right.isNone()){
                const auto& left_end = left.List().end();
                const auto& left_begin = left.List().begin();
                return std::find(left_begin, left_end, right) != left_end;
            }
        }
        return false;
    }
        break;
    }
    return false;
}

AndPredicate::AndPredicate(const Predicate* lhs, const Predicate* rhs)
: m_lhs(lhs), m_rhs(rhs)
{
}
bool AndPredicate::isMatch(const QueryContext& context) const
{
    return m_lhs->isMatch(context) && m_rhs->isMatch(context);

}

OrPredicate::OrPredicate(const Predicate* lhs, const Predicate* rhs)
: m_lhs(lhs), m_rhs(rhs)
{
}
bool OrPredicate::isMatch(const QueryContext& context) const
{
    return m_lhs->isMatch(context) || m_rhs->isMatch(context);

}

}
