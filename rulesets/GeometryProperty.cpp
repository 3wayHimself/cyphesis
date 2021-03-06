// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2016 Erik Ogenvik
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

#include "GeometryProperty.h"
#include "physics/Convert.h"
#include "common/log.h"
#include "common/globals.h"
#include "common/TypeNode.h"
#include "OgreMeshDeserializer.h"
#include "BBoxProperty.h"

#include <wfmath/atlasconv.h>

#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCylinderShape.h>
#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <boost/algorithm/string.hpp>
#include <common/AtlasQuery.h>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <common/debug.h>
#include <BulletCollision/CollisionShapes/btScaledBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <common/compose.hpp>

const std::string GeometryProperty::property_name = "geometry";
const std::string GeometryProperty::property_atlastype = "map";

auto createBoxFn = [&](const WFMath::AxisBox<3>& bbox, const WFMath::Vector<3>& size, btVector3& centerOfMassOffset, float)
    -> std::pair<btCollisionShape*, std::shared_ptr<btCollisionShape>> {
    auto btSize = Convert::toBullet(size * 0.5).absolute();
    centerOfMassOffset = -Convert::toBullet(bbox.getCenter());
    return std::make_pair(new btBoxShape(btSize), std::shared_ptr<btCollisionShape>());
};

void GeometryProperty::set(const Atlas::Message::Element& data)
{
    Property<Atlas::Message::MapType>::set(data);


    std::shared_ptr<OgreMeshDeserializer> deserializer;
    AtlasQuery::find<std::string>(data, "path", [&](const std::string& path) {
        try {
            if (boost::algorithm::ends_with(path, ".mesh")) {
                boost::filesystem::path fullpath = boost::filesystem::path(assets_directory) / path;
                boost::filesystem::fstream fileStream(fullpath);
                if (fileStream) {
                    deserializer.reset(new OgreMeshDeserializer(fileStream));
                    deserializer->deserialize();
                    m_meshBounds = deserializer->m_bounds;

                } else {
                    log(ERROR, "Could not find geometry file at " + fullpath.string());
                }
            } else {
                log(ERROR, "Could not recognize geometry file type: " + path);
            }
        } catch (const std::exception& ex) {
            log(ERROR, "Exception when trying to parse geometry at " + path);
        }
    });

    auto I = m_data.find("type");
    if (I != m_data.end() && I->second.isString()) {
        const std::string& shapeType = I->second.String();
        if (shapeType == "sphere") {
            mShapeCreator = [&](const WFMath::AxisBox<3>& bbox, const WFMath::Vector<3>& size, btVector3& centerOfMassOffset, float)
                -> std::pair<btCollisionShape*, std::shared_ptr<btCollisionShape>> {
                float minRadius = std::min(size.x(), std::min(size.y(), size.z())) * 0.5f;
                float xOffset = bbox.lowCorner().x() + minRadius;
                float yOffset = bbox.lowCorner().y() + minRadius;
                float zOffset = bbox.lowCorner().z() + minRadius;

                centerOfMassOffset = -btVector3(xOffset, yOffset, zOffset);
                return std::make_pair(new btSphereShape(minRadius), std::shared_ptr<btCollisionShape>());
            };
        } else if (shapeType == "capsule-y") {
            mShapeCreator = [&](const WFMath::AxisBox<3>& bbox, const WFMath::Vector<3>& size, btVector3& centerOfMassOffset, float)
                -> std::pair<btCollisionShape*, std::shared_ptr<btCollisionShape>> {
                centerOfMassOffset = -Convert::toBullet(bbox.getCenter());
                float minRadius = std::min(size.x(), size.z()) * 0.5f;
                //subtract the radius times 2 from the height
                float height = size.y() - (minRadius * 2.0f);
                //If the resulting height is negative we need to use a sphere instead.
                if (height > 0) {
                    return std::make_pair(new btCapsuleShape(minRadius, height), std::shared_ptr<btCollisionShape>());
                } else {
                    return std::make_pair(new btSphereShape(minRadius), std::shared_ptr<btCollisionShape>());
                }
            };

        } else if (shapeType == "capsule-x") {
            mShapeCreator = [&](const WFMath::AxisBox<3>& bbox, const WFMath::Vector<3>& size, btVector3& centerOfMassOffset, float)
                -> std::pair<btCollisionShape*, std::shared_ptr<btCollisionShape>> {
                centerOfMassOffset = -Convert::toBullet(bbox.getCenter());
                float minRadius = std::min(size.z(), size.y()) * 0.5f;
                //subtract the radius times 2 from the height
                float height = size.x() - (minRadius * 2.0f);
                //If the resulting height is negative we need to use a sphere instead.
                if (height > 0) {
                    return std::make_pair(new btCapsuleShapeX(minRadius, height), std::shared_ptr<btCollisionShape>());
                } else {
                    return std::make_pair(new btSphereShape(minRadius), std::shared_ptr<btCollisionShape>());
                }
            };
        } else if (shapeType == "capsule-z") {
            mShapeCreator = [&](const WFMath::AxisBox<3>& bbox, const WFMath::Vector<3>& size, btVector3& centerOfMassOffset, float)
                -> std::pair<btCollisionShape*, std::shared_ptr<btCollisionShape>> {
                centerOfMassOffset = -Convert::toBullet(bbox.getCenter());
                float minRadius = std::min(size.x(), size.y()) * 0.5f;
                //subtract the radius times 2 from the height
                float height = size.z() - (minRadius * 2.0f);
                //If the resulting height is negative we need to use a sphere instead.
                if (height > 0) {
                    return std::make_pair(new btCapsuleShapeZ(minRadius, height), std::shared_ptr<btCollisionShape>());
                } else {
                    return std::make_pair(new btSphereShape(minRadius), std::shared_ptr<btCollisionShape>());
                }
            };

        } else if (shapeType == "box") {
            mShapeCreator = createBoxFn;
        } else if (shapeType == "cylinder-y") {
            mShapeCreator = [&](const WFMath::AxisBox<3>& bbox, const WFMath::Vector<3>& size, btVector3& centerOfMassOffset, float)
                -> std::pair<btCollisionShape*, std::shared_ptr<btCollisionShape>> {
                centerOfMassOffset = -Convert::toBullet(bbox.getCenter());
                btCylinderShape* shape = new btCylinderShape(btVector3(1, 1, 1));
                shape->setLocalScaling(Convert::toBullet(size * 0.5f));
                return std::make_pair(shape, std::shared_ptr<btCollisionShape>());
            };


        } else if (shapeType == "cylinder-x") {
            mShapeCreator = [&](const WFMath::AxisBox<3>& bbox, const WFMath::Vector<3>& size, btVector3& centerOfMassOffset, float)
                -> std::pair<btCollisionShape*, std::shared_ptr<btCollisionShape>> {
                centerOfMassOffset = -Convert::toBullet(bbox.getCenter());
                btCylinderShape* shape = new btCylinderShapeX(btVector3(1, 1, 1));
                shape->setLocalScaling(Convert::toBullet(size * 0.5f));
                return std::make_pair(shape, std::shared_ptr<btCollisionShape>());
            };
        } else if (shapeType == "cylinder-z") {
            mShapeCreator = [&](const WFMath::AxisBox<3>& bbox, const WFMath::Vector<3>& size, btVector3& centerOfMassOffset, float)
                -> std::pair<btCollisionShape*, std::shared_ptr<btCollisionShape>> {
                centerOfMassOffset = -Convert::toBullet(bbox.getCenter());
                btCylinderShape* shape = new btCylinderShapeZ(btVector3(1, 1, 1));
                shape->setLocalScaling(Convert::toBullet(size * 0.5f));
                return std::make_pair(shape, std::shared_ptr<btCollisionShape>());
            };
        } else if (shapeType == "mesh") {
            buildMeshCreator(std::move(deserializer));
        } else if (shapeType == "compound") {
            buildCompoundCreator();
        }
    } else {
        log(WARNING, "Geometry property without 'type' attribute set. Property value: " + debug_tostring(data));
    }
}

std::pair<btCollisionShape*, std::shared_ptr<btCollisionShape>> GeometryProperty::createShape(const WFMath::AxisBox<3>& bbox, btVector3& centerOfMassOffset, float mass) const
{
    auto size = bbox.highCorner() - bbox.lowCorner();
    if (mShapeCreator) {
        return mShapeCreator(bbox, size, centerOfMassOffset, mass);
    } else {
        auto btSize = Convert::toBullet(size * 0.5).absolute();
        centerOfMassOffset = -Convert::toBullet(bbox.getCenter());
        auto shape = new btBoxShape(btSize);
        return std::make_pair(shape, std::shared_ptr<btCollisionShape>());
    }
}


void GeometryProperty::buildMeshCreator(std::shared_ptr<OgreMeshDeserializer> meshDeserializer)
{
    //Shared pointers since we want these values to survive as long as "meshShape" is alive.
    std::shared_ptr<std::vector<float>> verts(new std::vector<float>());
    std::shared_ptr<std::vector<unsigned int>> indices(new std::vector<unsigned int>());

    if (!meshDeserializer) {

        auto vertsI = m_data.find("vertices");
        if (vertsI != m_data.end() && vertsI->second.isList()) {
            auto trisI = m_data.find("indices");
            if (trisI != m_data.end() && trisI->second.isList()) {
                auto& vertsList = vertsI->second.List();
                auto& trisList = trisI->second.List();

                if (vertsList.empty()) {
                    log(ERROR, "Vertices is empty for mesh.");
                    return;
                }

                if (vertsList.size() % 3 != 0) {
                    log(ERROR, "Vertices is not even with 3.");
                    return;
                }

                if (trisList.empty()) {
                    log(ERROR, "Triangles is empty for mesh.");
                    return;
                }

                if (trisList.size() % 3 != 0) {
                    log(ERROR, "Triangles is not even with 3.");
                    return;
                }

                int numberOfVertices = static_cast<int>(vertsList.size() / 3);

                auto& local_verts = *verts.get();
                auto& local_indices = *indices.get();
                local_verts.resize(vertsList.size());

                for (size_t i = 0; i < vertsList.size(); i += 3) {
                    if (!vertsList[i].isFloat() || !vertsList[i + 1].isFloat() || !vertsList[i + 2].isFloat()) {
                        log(ERROR, "Vertex data was not a float for mesh.");
                        return;
                    }
                    local_verts[i] = (float) vertsList[i].Float();
                    local_verts[i + 1] = (float) vertsList[i + 1].Float();
                    local_verts[i + 2] = (float) vertsList[i + 2].Float();
                }

                local_indices.resize(trisList.size());
                for (size_t i = 0; i < trisList.size(); i += 3) {
                    if (!trisList[i].isInt() || !trisList[i + 1].isInt() || !trisList[i + 2].isInt()) {
                        log(ERROR, "Index data was not an int for mesh.");
                        return;
                    }
                    if (trisList[i].Int() >= numberOfVertices || trisList[i + 1].Int() >= numberOfVertices || trisList[i + 2].Int() >= numberOfVertices) {
                        log(ERROR, "Index data was out of bounds for vertices for mesh.");
                        return;
                    }
                    local_indices[i] = (unsigned int) trisList[i].Int();
                    local_indices[i + 1] = (unsigned int) trisList[i + 1].Int();
                    local_indices[i + 2] = (unsigned int) trisList[i + 2].Int();
                }


            } else {
                log(ERROR, "Could not find list of triangles for mesh.");
            }
        } else {
            log(ERROR, "Could not find list of vertices for mesh.");
        }
    } else {
        if (meshDeserializer) {
            *indices = std::move(meshDeserializer->m_indices);
            *verts = std::move(meshDeserializer->m_vertices);
        } else {
            //No mesh deserializer, and no other mesh data, return.
            return;
        }
    }


    if (indices->empty() || verts->empty()) {
        log(ERROR, "Vertices or indices were empty.");
        return;
    }

    for (auto index : *indices) {
        if (index >= verts->size() / 3) {
            log(ERROR, "Index out of bounds.");
            return;
        }
    }

    int vertStride = sizeof(float) * 3;
    int indexStride = sizeof(unsigned int) * 3;

    int indicesCount = static_cast<int>(indices->size() / 3);
    int vertexCount = static_cast<int>(verts->size() / 3);

    //Make sure to capture "verts" and "indices" so that they are kept around.
    std::shared_ptr<btTriangleIndexVertexArray> triangleVertexArray(new btTriangleIndexVertexArray(indicesCount, reinterpret_cast<int*>(indices->data()), indexStride,
                                                                                                   vertexCount, verts->data(), vertStride),
                                                                    [verts, indices](btTriangleIndexVertexArray* p) {
                                                                        delete p;
                                                                    });

    std::shared_ptr<btBvhTriangleMeshShape> meshShape(new btBvhTriangleMeshShape(triangleVertexArray.get(), true, true),
                                                      [triangleVertexArray](btBvhTriangleMeshShape* p) {
                                                          delete p;
                                                      });
    meshShape->setLocalScaling(btVector3(1, 1, 1));
    //Store the bounds, so that the "bbox" property can be updated when this is applied to a TypeNode
    m_meshBounds = WFMath::AxisBox<3>(Convert::toWF<WFMath::Point<3>>(meshShape->getLocalAabbMin()),
                                      Convert::toWF<WFMath::Point<3>>(meshShape->getLocalAabbMax()));

    mShapeCreator = [meshShape, verts, triangleVertexArray](const WFMath::AxisBox<3>& bbox, const WFMath::Vector<3>& size,
                                                            btVector3& centerOfMassOffset, float mass) -> std::pair<btCollisionShape*, std::shared_ptr<btCollisionShape>> {
        //In contrast to other shapes there's no centerOfMassOffset for mesh shapes
        centerOfMassOffset = btVector3(0, 0, 0);
        btVector3 meshSize = meshShape->getLocalAabbMax() - meshShape->getLocalAabbMin();
        btVector3 scaling(size.x() / meshSize.x(), size.y() / meshSize.y(), size.z() / meshSize.z());

        //Due to performance reasons we should use different shapes depending on whether it's static (i.e. mass == 0) or not
        if (mass == 0) {
            return std::make_pair(new btScaledBvhTriangleMeshShape(meshShape.get(), scaling), meshShape);
        } else {
            auto shape = new btConvexHullShape(verts.get()->data(), verts.get()->size() / 3, sizeof(float) * 3);

            //btConvexHullShape::optimizeConvexHull was introduced in 2.84. It's useful, but not necessary.
            //version number 285 corresponds to version 2.84...
            #if BT_BULLET_VERSION > 284
            shape->optimizeConvexHull();
            #endif
            shape->recalcLocalAabb();
            shape->setLocalScaling(scaling);
            return std::make_pair(shape, meshShape);
        }

    };


}

GeometryProperty* GeometryProperty::copy() const
{
    return new GeometryProperty(*this);
}


void GeometryProperty::install(TypeNode* typeNode, const std::string&)
{
    //If there are valid mesh bounds read, and there's no bbox property already, add one.
    if (m_meshBounds.isValid()) {
        BBoxProperty* bBoxProperty = nullptr;
        auto I = typeNode->defaults().find("bbox");
        if (I == typeNode->defaults().end()) {
            //Update the bbox property of the type if there are valid bounds from the mesh.
            bBoxProperty = new BBoxProperty();
            bBoxProperty->set(m_meshBounds.toAtlas());
            //Mark the property as ephemeral since it's calulcated.
            bBoxProperty->setFlags(flag_class | per_ephem);
            bBoxProperty->install(typeNode, "bbox");
        } else if ((I->second->flags() & per_ephem) != 0) {
            bBoxProperty = dynamic_cast<BBoxProperty*>(I->second);
            if (bBoxProperty) {
                bBoxProperty->set(m_meshBounds.toAtlas());
            }
        }

        if (bBoxProperty) {
            typeNode->injectProperty("bbox", bBoxProperty);
        }
    }
}

void GeometryProperty::buildCompoundCreator()
{
    mShapeCreator = [&](const WFMath::AxisBox<3>& bbox, const WFMath::Vector<3>& size,
                        btVector3& centerOfMassOffset, float mass) -> std::pair<btCollisionShape*, std::shared_ptr<btCollisionShape>> {
        auto I = m_data.find("shapes");
        if (I != m_data.end() && I->second.isList()) {
            auto shapes = I->second.List();
            #if BT_BULLET_VERSION > 283
            btCompoundShape* compoundShape = new btCompoundShape(true, shapes.size());
            #else
            btCompoundShape* compoundShape = new btCompoundShape(true);
            #endif
            std::vector<btCollisionShape*> childShapes(shapes.size());
            for (auto& shapeElement : shapes) {
                if (shapeElement.isMap()) {
                    auto& shapeMap = shapeElement.Map();
                    AtlasQuery::find<std::string>(shapeMap, "type", [&](const std::string& type) {
                        if (type == "box") {
                            AtlasQuery::find<Atlas::Message::ListType>(shapeMap, "points", [&](const Atlas::Message::ListType& points) {
                                WFMath::AxisBox<3> shapeBox(points);

                                btTransform transform(btQuaternion::getIdentity());
                                transform.setOrigin(Convert::toBullet(shapeBox.getCenter()));

                                AtlasQuery::find<Atlas::Message::ListType>(shapeMap, "orientation", [&](const Atlas::Message::ListType& orientationList) {
                                    transform.setRotation(Convert::toBullet(WFMath::Quaternion(orientationList)));

                                });

                                auto boxSize = shapeBox.highCorner() - shapeBox.lowCorner();

                                btBoxShape* boxShape = new btBoxShape(Convert::toBullet(boxSize / 2.f));
                                childShapes.emplace_back(boxShape);
                                compoundShape->addChildShape(transform, boxShape);
                            });
                        } else {
                            //TODO: implement more shapes when needed. "box" should go a long way though.
                            log(WARNING, String::compose("Unrecognized compound shape type '%1'.", type));
                        }
                    });

                }
            }

            btVector3 aabbMin, aabbMax;
            compoundShape->getAabb(btTransform::getIdentity(), aabbMin, aabbMax);

            centerOfMassOffset = btVector3(0, 0, 0);
            btVector3 meshSize = aabbMax - aabbMin;
            btVector3 scaling(size.x() / meshSize.x(), size.y() / meshSize.y(), size.z() / meshSize.z());
            compoundShape->setLocalScaling(scaling);

            return std::make_pair(compoundShape, std::shared_ptr<btCollisionShape>(nullptr, [childShapes](btCollisionShape* p) {
                //Don't delete the shape, just the child shapes.
                for (btCollisionShape* childShape : childShapes) {
                    delete childShape;
                }
            }));
        }
        return createBoxFn(bbox, size, centerOfMassOffset, mass);
    };

}

