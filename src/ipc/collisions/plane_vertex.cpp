#include "plane_vertex.hpp"

#include <ipc/distance/point_plane.hpp>

namespace ipc {

PlaneVertexConstraint::PlaneVertexConstraint(
    const VectorMax3d& plane_origin,
    const VectorMax3d& plane_normal,
    const long vertex_id)
    : plane_origin(plane_origin)
    , plane_normal(plane_normal)
    , vertex_id(vertex_id)
{
}

double PlaneVertexConstraint::compute_distance(const VectorMax12d& point) const
{
    assert(point.size() == plane_origin.size());
    return point_plane_distance(point, plane_origin, plane_normal);
}

VectorMax12d PlaneVertexConstraint::compute_distance_gradient(
    const VectorMax12d& point) const
{
    assert(point.size() == plane_origin.size());
    return point_plane_distance_gradient(point, plane_origin, plane_normal);
}

MatrixMax12d
PlaneVertexConstraint::compute_distance_hessian(const VectorMax12d& point) const
{
    assert(point.size() == plane_origin.size());
    return point_plane_distance_hessian(point, plane_origin, plane_normal);
}

} // namespace ipc
