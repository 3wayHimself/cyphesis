// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2011 Alistair Riddoch
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

#ifndef PHYSICS_SHAPE_H
#define PHYSICS_SHAPE_H

#include <wfmath/const.h>

#include <map>
#include <string>

namespace Atlas {
    namespace Message {
        class Element;
        typedef std::map<std::string, Element> MapType;
    }
}

/// \brief Shape interface for inheritance based use of wfmath shapes
class Shape {
  private:
    explicit Shape(const Shape &);
    Shape & operator=(const Shape &);
  protected:
    Shape();
  public:
    virtual size_t size() const = 0;

    virtual double area() const = 0;
    virtual WFMath::AxisBox<2> footprint() const = 0;
    virtual void scale(double factor) = 0;

    virtual void toAtlas(Atlas::Message::MapType &) const = 0;
    virtual void fromAtlas(const Atlas::Message::MapType &) = 0;

    virtual void stream(std::ostream &) const = 0;

    /// \brief Name constructor
    static Shape * newFromAtlas(const Atlas::Message::MapType &);
};

template<template <int> class ShapeT, const int dim = 2>
class MathShape : public Shape {
  protected:
    ShapeT<dim> m_shape;

    const char * getType() const;
  public:
    MathShape(const ShapeT<dim> &);

    virtual size_t size() const;

    virtual double area() const;
    virtual WFMath::AxisBox<2> footprint() const;
    virtual void scale(double factor);

    virtual void toAtlas(Atlas::Message::MapType &) const;
    virtual void fromAtlas(const Atlas::Message::MapType &);

    virtual void stream(std::ostream &) const;
};

inline std::ostream & operator<<(std::ostream& os, const Shape & s)
{
    s.stream(os);
    return os;
}

#endif // PHYSICS_SHAPE_H
