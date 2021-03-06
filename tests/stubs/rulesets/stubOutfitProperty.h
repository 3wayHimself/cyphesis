// AUTOGENERATED file, created by the tool generate_stub.py, don't edit!
// If you want to add your own functionality, instead edit the stubOutfitProperty_custom.h file.

#include "rulesets/OutfitProperty.h"
#include "stubOutfitProperty_custom.h"

#ifndef STUB_RULESETS_OUTFITPROPERTY_H
#define STUB_RULESETS_OUTFITPROPERTY_H

#ifndef STUB_OutfitProperty_itemRemoved
//#define STUB_OutfitProperty_itemRemoved
  void OutfitProperty::itemRemoved(LocatedEntity * garment, LocatedEntity * wearer)
  {
    
  }
#endif //STUB_OutfitProperty_itemRemoved

#ifndef STUB_OutfitProperty_OutfitProperty
//#define STUB_OutfitProperty_OutfitProperty
   OutfitProperty::OutfitProperty()
    : PropertyBase()
  {
    
  }
#endif //STUB_OutfitProperty_OutfitProperty

#ifndef STUB_OutfitProperty_OutfitProperty_DTOR
//#define STUB_OutfitProperty_OutfitProperty_DTOR
   OutfitProperty::~OutfitProperty()
  {
    
  }
#endif //STUB_OutfitProperty_OutfitProperty_DTOR

#ifndef STUB_OutfitProperty_get
//#define STUB_OutfitProperty_get
  int OutfitProperty::get(Atlas::Message::Element & val) const
  {
    return 0;
  }
#endif //STUB_OutfitProperty_get

#ifndef STUB_OutfitProperty_set
//#define STUB_OutfitProperty_set
  void OutfitProperty::set(const Atlas::Message::Element & val)
  {
    
  }
#endif //STUB_OutfitProperty_set

#ifndef STUB_OutfitProperty_add
//#define STUB_OutfitProperty_add
  void OutfitProperty::add(const std::string & key, Atlas::Message::MapType & map) const
  {
    
  }
#endif //STUB_OutfitProperty_add

#ifndef STUB_OutfitProperty_add
//#define STUB_OutfitProperty_add
  void OutfitProperty::add(const std::string & key, const Atlas::Objects::Entity::RootEntity & ent) const
  {
    
  }
#endif //STUB_OutfitProperty_add

#ifndef STUB_OutfitProperty_copy
//#define STUB_OutfitProperty_copy
  OutfitProperty* OutfitProperty::copy() const
  {
    return nullptr;
  }
#endif //STUB_OutfitProperty_copy

#ifndef STUB_OutfitProperty_getEntity
//#define STUB_OutfitProperty_getEntity
  LocatedEntity* OutfitProperty::getEntity(const std::string& key) const
  {
    return nullptr;
  }
#endif //STUB_OutfitProperty_getEntity

#ifndef STUB_OutfitProperty_cleanUp
//#define STUB_OutfitProperty_cleanUp
  void OutfitProperty::cleanUp()
  {
    
  }
#endif //STUB_OutfitProperty_cleanUp

#ifndef STUB_OutfitProperty_wear
//#define STUB_OutfitProperty_wear
  void OutfitProperty::wear(LocatedEntity * wearer, const std::string & location, LocatedEntity * garment)
  {
    
  }
#endif //STUB_OutfitProperty_wear

#ifndef STUB_OutfitProperty_data
//#define STUB_OutfitProperty_data
  const EntityRefMap& OutfitProperty::data() const
  {
    return *static_cast<const EntityRefMap*>(nullptr);
  }
#endif //STUB_OutfitProperty_data


#endif