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

#ifndef PHYSICS_SHAPE_IMPL_H
#define PHYSICS_SHAPE_IMPL_H

#include "Shape.h"

#include <Atlas/Message/Element.h>

#include <wfmath/atlasconv.h>

template<template <int> class ShapeT, const int dim>
const char * MathShape<ShapeT, dim>::getType() const
{
    return "unknown";
}

template<template <int> class ShapeT, const int dim>
MathShape<ShapeT, dim>::MathShape(const ShapeT<dim> & s) : m_shape(s)
{
}

template<template <int> class ShapeT, const int dim>
size_t MathShape<ShapeT, dim>::size() const
{
    return m_shape.numCorners();
}

template<template <int> class ShapeT, const int dim>
bool MathShape<ShapeT, dim>::isValid() const
{
    return m_shape.isValid();
}

template<template <int> class ShapeT, const int dim>
WFMath::CoordType MathShape<ShapeT, dim>::area() const
{
    return 1.;
}

template<template <int> class ShapeT, const int dim>
WFMath::AxisBox<2> MathShape<ShapeT, dim>::footprint() const
{
    return WFMath::AxisBox<2>();
}

template<template <int> class ShapeT, const int dim>
WFMath::Point<3> MathShape<ShapeT, dim>::lowCorner() const
{
    return m_shape.boundingBox().lowCorner();
}

template<template <int> class ShapeT, const int dim>
WFMath::Point<3> MathShape<ShapeT, dim>::highCorner() const
{
    return m_shape.boundingBox().highCorner();
}

template<template <int> class ShapeT, const int dim>
void MathShape<ShapeT, dim>::scale(WFMath::CoordType factor)
{
}

template<template <int> class ShapeT, const int dim>
void MathShape<ShapeT, dim>::toAtlas(Atlas::Message::MapType & data) const
{
    data["type"] = getType();
    int size = m_shape.numCorners();
    if (size > 0) {
        Atlas::Message::ListType points;
        for (int i = 0; i < size; ++i) {
            WFMath::Point<dim> corner = m_shape.getCorner(i);
            points.push_back(corner.toAtlas());
        }
        data["points"] = points;
    }
}

template<template <int> class ShapeT, const int dim>
void MathShape<ShapeT, dim>::fromAtlas(const Atlas::Message::MapType & data)
{
    m_shape.fromAtlas(data);
}

template<template <int> class ShapeT, const int dim>
void MathShape<ShapeT, dim>::stream(std::ostream & o) const
{
    o << getType() << ": ";
    o << m_shape;
}

#endif // PHYSICS_SHAPE_IMPL_H
