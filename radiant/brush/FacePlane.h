#pragma once

#include "math/Plane3.h"
#include "math/Vector3.h"
#include "PlanePoints.h"

class Matrix4;

/// Wrapper for a Plane3 providing certain face-related functions
class FacePlane
{
    Plane3 m_plane;

public:
    class SavedState
    {
    public:
        Plane3 m_plane;

        SavedState(const FacePlane& facePlane) :
            m_plane(facePlane.m_plane)
        {}

        void exportState(FacePlane& facePlane) const
        {
            facePlane.m_plane = m_plane;
        }
    }; // class SavedState

    // Constructor and copy constructor
    FacePlane();
    FacePlane(const FacePlane& other);

    void reverse();

    void translate(const Vector3& translation);
    void transform(const Matrix4& matrix, bool mirror);

    void offset(float offset);

    void setPlane(const Plane3& plane);
    const Plane3& getPlane() const;

    void copy(const FacePlane& other);
    void copy(const Vector3& p0, const Vector3& p1, const Vector3& p2);

}; // class FacePlane
