#pragma once

#include "ibrush.h"
#include "NodeBase.h"
#include "math/Matrix3.h"

namespace textool
{

class FaceNode :
    public NodeBase
{
private:
    IFace& _face;
    mutable AABB _bounds;

public:
    FaceNode(IFace& face) :
        _face(face)
    {}

    void applyTransformToSelected(const Matrix3& transform) override
    {
        for (auto& vertex : _face.getWinding())
        {
            vertex.texcoord = transform * vertex.texcoord;
        }

        Vector3 vertices[3] = { _face.getWinding().at(0).vertex, _face.getWinding().at(1).vertex, _face.getWinding().at(2).vertex };
        Vector2 texcoords[3] = { _face.getWinding().at(0).texcoord, _face.getWinding().at(1).texcoord, _face.getWinding().at(2).texcoord };

        _face.setTexDefFromPoints(vertices, texcoords);
    }

    const AABB& localAABB() const
    {
        _bounds = AABB();

        for (const auto& vertex : _face.getWinding())
        {
            _bounds.includePoint({ vertex.texcoord.x(), vertex.texcoord.y(), 0 });
        }

        return _bounds;
    }

    void testSelect(Selector& selector, SelectionTest& test) override
    {
        // Arrange the UV coordinates in a Vector3 array for testing
        std::vector<Vector3> uvs;
        uvs.reserve(_face.getWinding().size());

        for (const auto& vertex : _face.getWinding())
        {
            uvs.emplace_back(vertex.texcoord.x(), vertex.texcoord.y(), 0);
        }

        test.BeginMesh(Matrix4::getIdentity(), true);

        SelectionIntersection best;
        test.TestPolygon(VertexPointer(uvs.data(), sizeof(Vector3)), uvs.size(), best);

        if (best.isValid())
        {
            Selector_add(selector, *this);
        }
    }
};

}
