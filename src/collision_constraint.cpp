#include <ipc/collision_constraint.hpp>

#include <stdexcept> // std::out_of_range

#include <ipc/barrier/barrier.hpp>
#include <ipc/distance/edge_edge.hpp>
#include <ipc/distance/edge_edge_mollifier.hpp>
#include <ipc/distance/point_edge.hpp>
#include <ipc/distance/point_triangle.hpp>

namespace ipc {

double CollisionConstraint::compute_potential_common(
    const double distance, const double dhat) const
{
    const double dhat_squared = dhat * dhat;
    return barrier(
        distance - minimum_distance * minimum_distance,
        2 * minimum_distance * dhat + dhat_squared);
}

template <typename DerivedDistanceGrad>
VectorMax12d CollisionConstraint::compute_potential_gradient_common(
    const double distance,
    const Eigen::MatrixBase<DerivedDistanceGrad>& distance_grad,
    const double dhat) const
{
    const double dhat_squared = dhat * dhat;

    // ∇b(d(x)) = b'(d(x)) * ∇d(x)
    double grad_b = barrier_gradient(
        distance - minimum_distance * minimum_distance,
        2 * minimum_distance * dhat + dhat_squared);
    return grad_b * distance_grad;
}

template <typename DerivedDistanceGrad, typename DerivedDistanceHess>
MatrixMax12d CollisionConstraint::compute_potential_hessian_common(
    const double distance,
    const Eigen::MatrixBase<DerivedDistanceGrad>& distance_grad,
    const Eigen::MatrixBase<DerivedDistanceHess>& distance_hess,
    const double dhat,
    const bool project_hessian_to_psd) const
{
    const double dhat_squared = dhat * dhat;
    const double min_dist_squrared = minimum_distance * minimum_distance;

    // ∇²[b(d(x))] = ∇(b'(d(x)) * ∇d(x))
    //             = b"(d(x)) * ∇d(x) * ∇d(x)ᵀ + b'(d(x)) * ∇²d(x)

    double grad_b = barrier_gradient(
        distance - min_dist_squrared,
        2 * minimum_distance * dhat + dhat_squared);

    double hess_b = barrier_hessian(
        distance - min_dist_squrared,
        2 * minimum_distance * dhat + dhat_squared);

    // b"(x) ≥ 0 ⟹ b"(x) * ∇d(x) * ∇d(x)ᵀ is PSD
    assert(hess_b >= 0);
    return hess_b * distance_grad * distance_grad.transpose()
        + (project_hessian_to_psd
               ? project_to_psd((grad_b * distance_hess).eval())
               : (grad_b * distance_hess));
}

///////////////////////////////////////////////////////////////////////////////

VertexVertexConstraint::VertexVertexConstraint(
    long vertex0_index, long vertex1_index)
    : VertexVertexCandidate(vertex0_index, vertex1_index)
{
}

VertexVertexConstraint::VertexVertexConstraint(
    const VertexVertexCandidate& candidate)
    : VertexVertexCandidate(candidate)
{
}

double VertexVertexConstraint::compute_potential(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat) const
{
    double distance =
        point_point_distance(V.row(vertex0_index), V.row(vertex1_index));
    return multiplicity * compute_potential_common(distance, dhat);
}

VectorMax12d VertexVertexConstraint::compute_potential_gradient(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat) const
{
    // ∇[m * b(d(x))] = m * b'(d(x)) * ∇d(x)
    const auto& p0 = V.row(vertex0_index);
    const auto& p1 = V.row(vertex1_index);

    VectorMax6d distance_grad;
    point_point_distance_gradient(p0, p1, distance_grad);

    double distance = point_point_distance(p0, p1);

    return multiplicity
        * compute_potential_gradient_common(distance, distance_grad, dhat);
}

MatrixMax12d VertexVertexConstraint::compute_potential_hessian(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat,
    const bool project_hessian_to_psd) const
{
    // ∇²[m * b(d(x))] = m * ∇(b'(d(x)) * ∇d(x))
    //                 = m * [b"(d(x)) * ∇d(x) * ∇d(x)ᵀ + b'(d(x)) * ∇²d(x)]
    const auto& p0 = V.row(vertex0_index);
    const auto& p1 = V.row(vertex1_index);

    double distance = point_point_distance(p0, p1);
    VectorMax6d distance_grad;
    point_point_distance_gradient(p0, p1, distance_grad);
    MatrixMax6d distance_hess;
    point_point_distance_hessian(p0, p1, distance_hess);

    return multiplicity
        * compute_potential_hessian_common(
               distance, distance_grad, distance_hess, dhat,
               project_hessian_to_psd);
}

///////////////////////////////////////////////////////////////////////////////

EdgeVertexConstraint::EdgeVertexConstraint(long edge_index, long vertex_index)
    : EdgeVertexCandidate(edge_index, vertex_index)
{
}

EdgeVertexConstraint::EdgeVertexConstraint(const EdgeVertexCandidate& candidate)
    : EdgeVertexCandidate(candidate)
{
}

double EdgeVertexConstraint::compute_potential(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat) const
{
    // The distance type is known because of construct_constraint_set()
    double distance = point_edge_distance(
        V.row(vertex_index), V.row(E(edge_index, 0)), V.row(E(edge_index, 1)),
        PointEdgeDistanceType::P_E);
    return multiplicity * compute_potential_common(distance, dhat);
}

VectorMax12d EdgeVertexConstraint::compute_potential_gradient(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat) const
{
    // ∇[m * b(d(x))] = m * b'(d(x)) * ∇d(x)
    const auto& p = V.row(vertex_index);
    const auto& e0 = V.row(E(edge_index, 0));
    const auto& e1 = V.row(E(edge_index, 1));

    VectorMax9d distance_grad;
    point_edge_distance_gradient(
        p, e0, e1, PointEdgeDistanceType::P_E, distance_grad);

    double distance =
        point_edge_distance(p, e0, e1, PointEdgeDistanceType::P_E);

    return multiplicity
        * compute_potential_gradient_common(distance, distance_grad, dhat);
}

MatrixMax12d EdgeVertexConstraint::compute_potential_hessian(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat,
    const bool project_hessian_to_psd) const
{
    // ∇²[m * b(d(x))] = m * ∇(b'(d(x)) * ∇d(x))
    //                 = m * [b"(d(x)) * ∇d(x) * ∇d(x)ᵀ + b'(d(x)) * ∇²d(x)]
    const auto& p = V.row(vertex_index);
    const auto& e0 = V.row(E(edge_index, 0));
    const auto& e1 = V.row(E(edge_index, 1));

    double distance =
        point_edge_distance(p, e0, e1, PointEdgeDistanceType::P_E);
    VectorMax9d distance_grad;
    point_edge_distance_gradient(
        p, e0, e1, PointEdgeDistanceType::P_E, distance_grad);
    MatrixMax12d distance_hess;
    point_edge_distance_hessian(
        p, e0, e1, PointEdgeDistanceType::P_E, distance_hess);

    return multiplicity
        * compute_potential_hessian_common(
               distance, distance_grad, distance_hess, dhat,
               project_hessian_to_psd);
}

///////////////////////////////////////////////////////////////////////////////

EdgeEdgeConstraint::EdgeEdgeConstraint(
    long edge0_index, long edge1_index, double eps_x)
    : EdgeEdgeCandidate(edge0_index, edge1_index)
    , eps_x(eps_x)
{
}

EdgeEdgeConstraint::EdgeEdgeConstraint(
    const EdgeEdgeCandidate& candidate, double eps_x)
    : EdgeEdgeCandidate(candidate)
    , eps_x(eps_x)
{
}

double EdgeEdgeConstraint::compute_potential(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat) const
{
    const auto& ea0 = V.row(E(edge0_index, 0));
    const auto& ea1 = V.row(E(edge0_index, 1));
    const auto& eb0 = V.row(E(edge1_index, 0));
    const auto& eb1 = V.row(E(edge1_index, 1));

    // The distance type is unknown because of mollified PP and PE
    // constraints where also added as EE constraints.
    double distance = edge_edge_distance(ea0, ea1, eb0, eb1);
    return edge_edge_mollifier(ea0, ea1, eb0, eb1, eps_x)
        * compute_potential_common(distance, dhat);
}

VectorMax12d EdgeEdgeConstraint::compute_potential_gradient(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat) const
{
    const double dhat_squared = dhat * dhat;

    // ∇[m(x) * b(d(x))] = (∇m(x)) * b(d(x)) + m(x) * b'(d(x)) * ∇d(x)
    const auto& ea0 = V.row(E(edge0_index, 0));
    const auto& ea1 = V.row(E(edge0_index, 1));
    const auto& eb0 = V.row(E(edge1_index, 0));
    const auto& eb1 = V.row(E(edge1_index, 1));

    // The distance type is unknown because of mollified PP and PE
    // constraints where also added as EE constraints.
    EdgeEdgeDistanceType dtype = edge_edge_distance_type(ea0, ea1, eb0, eb1);
    double distance = edge_edge_distance(ea0, ea1, eb0, eb1, dtype);
    VectorMax12d distance_grad;
    edge_edge_distance_gradient(ea0, ea1, eb0, eb1, dtype, distance_grad);

    // m(x)
    double mollifier = edge_edge_mollifier(ea0, ea1, eb0, eb1, eps_x);
    // ∇m(x)
    VectorMax12d mollifier_grad;
    edge_edge_mollifier_gradient(ea0, ea1, eb0, eb1, eps_x, mollifier_grad);

    // b(d(x))
    double b = barrier(
        distance - minimum_distance * minimum_distance,
        2 * minimum_distance * dhat + dhat_squared);

    // b'(d(x)) * ∇d(x)
    auto barrier_distance_grad =
        compute_potential_gradient_common(distance, distance_grad, dhat);

    return mollifier_grad * b + mollifier * barrier_distance_grad;
}

MatrixMax12d EdgeEdgeConstraint::compute_potential_hessian(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat,
    const bool project_hessian_to_psd) const
{
    const double dhat_squared = dhat * dhat;
    const double min_dist_squrared = minimum_distance * minimum_distance;

    // ∇²[m(x) * b(d(x))] = ∇[∇m(x) * b(d(x)) + m(x) * b'(d(x)) * ∇d(x)]
    //                    = ∇²m(x) * b(d(x)) + b'(d(x)) * ∇d(x) * ∇m(x)ᵀ
    //                      + ∇m(x) * b'(d(x)) * ∇d(x))ᵀ
    //                      + m(x) * b"(d(x)) * ∇d(x) * ∇d(x)ᵀ
    //                      + m(x) * b'(d(x)) * ∇²d(x)
    const auto& ea0 = V.row(E(edge0_index, 0));
    const auto& ea1 = V.row(E(edge0_index, 1));
    const auto& eb0 = V.row(E(edge1_index, 0));
    const auto& eb1 = V.row(E(edge1_index, 1));

    // Compute distance derivatives
    // The distance type is unknown because of mollified PP and PE
    // constraints where also added as EE constraints.
    EdgeEdgeDistanceType dtype = edge_edge_distance_type(ea0, ea1, eb0, eb1);
    double distance = edge_edge_distance(ea0, ea1, eb0, eb1, dtype);
    VectorMax12d distance_grad;
    edge_edge_distance_gradient(ea0, ea1, eb0, eb1, dtype, distance_grad);
    MatrixMax12d distance_hess;
    edge_edge_distance_hessian(ea0, ea1, eb0, eb1, dtype, distance_hess);

    // Compute mollifier derivatives
    double mollifier = edge_edge_mollifier(ea0, ea1, eb0, eb1, eps_x);
    VectorMax12d mollifier_grad;
    edge_edge_mollifier_gradient(ea0, ea1, eb0, eb1, eps_x, mollifier_grad);
    MatrixMax12d mollifier_hess;
    edge_edge_mollifier_hessian(ea0, ea1, eb0, eb1, eps_x, mollifier_hess);

    // Compute barrier derivatives
    double b = barrier(
        distance - min_dist_squrared,
        2 * minimum_distance * dhat + dhat_squared);
    double grad_b = barrier_gradient(
        distance - min_dist_squrared,
        2 * minimum_distance * dhat + dhat_squared);
    double hess_b = barrier_hessian(
        distance - min_dist_squrared,
        2 * minimum_distance * dhat + dhat_squared);

    MatrixMax12d hess = mollifier_hess * b
        + grad_b
            * (distance_grad * mollifier_grad.transpose()
               + mollifier_grad * distance_grad.transpose())
        + mollifier
            * (hess_b * distance_grad * distance_grad.transpose()
               + grad_b * distance_hess);

    if (project_hessian_to_psd) {
        hess = project_to_psd(hess);
    }

    return hess;
}

///////////////////////////////////////////////////////////////////////////////

FaceVertexConstraint::FaceVertexConstraint(long face_index, long vertex_index)
    : FaceVertexCandidate(face_index, vertex_index)
{
}

FaceVertexConstraint::FaceVertexConstraint(const FaceVertexCandidate& candidate)
    : FaceVertexCandidate(candidate)
{
}

double FaceVertexConstraint::compute_potential(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat) const
{
    // The distance type is known because of construct_constraint_set()
    double distance = point_triangle_distance(
        V.row(vertex_index), V.row(F(face_index, 0)), V.row(F(face_index, 1)),
        V.row(F(face_index, 2)), PointTriangleDistanceType::P_T);
    return compute_potential_common(distance, dhat);
}

VectorMax12d FaceVertexConstraint::compute_potential_gradient(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat) const
{
    // ∇b(d(x)) = b'(d(x)) * ∇d(x)
    const auto& p = V.row(vertex_index);
    const auto& t0 = V.row(F(face_index, 0));
    const auto& t1 = V.row(F(face_index, 1));
    const auto& t2 = V.row(F(face_index, 2));

    double distance =
        point_triangle_distance(p, t0, t1, t2, PointTriangleDistanceType::P_T);
    VectorMax12d distance_grad;
    point_triangle_distance_gradient(
        p, t0, t1, t2, PointTriangleDistanceType::P_T, distance_grad);

    return compute_potential_gradient_common(distance, distance_grad, dhat);
}

MatrixMax12d FaceVertexConstraint::compute_potential_hessian(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat,
    const bool project_hessian_to_psd) const
{
    // ∇²b(d(x)) = ∇(b'(d(x)) * ∇d(x))
    //           = b"(d(x)) * ∇d(x) * ∇d(x)ᵀ + b'(d(x)) * ∇²d(x)
    const auto& p = V.row(vertex_index);
    const auto& t0 = V.row(F(face_index, 0));
    const auto& t1 = V.row(F(face_index, 1));
    const auto& t2 = V.row(F(face_index, 2));

    double distance =
        point_triangle_distance(p, t0, t1, t2, PointTriangleDistanceType::P_T);
    VectorMax12d distance_grad;
    point_triangle_distance_gradient(
        p, t0, t1, t2, PointTriangleDistanceType::P_T, distance_grad);
    MatrixMax12d distance_hess;
    point_triangle_distance_hessian(
        p, t0, t1, t2, PointTriangleDistanceType::P_T, distance_hess);

    return compute_potential_hessian_common(
        distance, distance_grad, distance_hess, dhat, project_hessian_to_psd);
}

///////////////////////////////////////////////////////////////////////////////

PlaneVertexConstraint::PlaneVertexConstraint(
    const VectorMax3d& plane_origin,
    const VectorMax3d& plane_normal,
    const long vertex_index)
    : plane_origin(plane_origin)
    , plane_normal(plane_normal)
    , vertex_index(vertex_index)
{
}

double PlaneVertexConstraint::compute_potential(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat) const
{
    double distance =
        point_plane_distance(V.row(vertex_index), plane_origin, plane_normal);
    return compute_potential_common(distance, dhat);
}

VectorMax12d PlaneVertexConstraint::compute_potential_gradient(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat) const
{
    // ∇b(d(x)) = b'(d(x)) * ∇d(x)
    const auto& p = V.row(vertex_index);

    double distance = point_plane_distance(p, plane_origin, plane_normal);
    VectorMax3d distance_grad;
    point_plane_distance_gradient(p, plane_origin, plane_normal, distance_grad);

    return compute_potential_gradient_common(distance, distance_grad, dhat);
}

MatrixMax12d PlaneVertexConstraint::compute_potential_hessian(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    const double dhat,
    const bool project_hessian_to_psd) const
{
    // ∇²b(d(x)) = ∇(b'(d(x)) * ∇d(x))
    //           = b"(d(x)) * ∇d(x) * ∇d(x)ᵀ + b'(d(x)) * ∇²d(x)
    const auto& p = V.row(vertex_index);

    double distance = point_plane_distance(p, plane_origin, plane_normal);
    VectorMax3d distance_grad;
    point_plane_distance_gradient(p, plane_origin, plane_normal, distance_grad);
    MatrixMax3d distance_hess;
    point_plane_distance_hessian(p, plane_origin, plane_normal, distance_hess);

    return compute_potential_hessian_common(
        distance, distance_grad, distance_hess, dhat, project_hessian_to_psd);
}

///////////////////////////////////////////////////////////////////////////////

size_t Constraints::size() const
{
    return vv_constraints.size() + ev_constraints.size() + ee_constraints.size()
        + fv_constraints.size() + pv_constraints.size();
}

size_t Constraints::num_constraints() const
{
    size_t num_constraints = 0;
    for (const auto& vv_constraint : vv_constraints) {
        num_constraints += vv_constraint.multiplicity;
    }
    for (const auto& ev_constraint : ev_constraints) {
        num_constraints += ev_constraint.multiplicity;
    }
    num_constraints +=
        ee_constraints.size() + fv_constraints.size() + pv_constraints.size();
    return num_constraints;
}

bool Constraints::empty() const
{
    return vv_constraints.empty() && ev_constraints.empty()
        && ee_constraints.empty() && fv_constraints.empty()
        && pv_constraints.empty();
}

void Constraints::clear()
{
    vv_constraints.clear();
    ev_constraints.clear();
    ee_constraints.clear();
    fv_constraints.clear();
    pv_constraints.clear();
}

CollisionConstraint& Constraints::operator[](size_t idx)
{
    if (idx < 0) {
        throw std::out_of_range("Constraint index is out of range!");
    }
    if (idx < vv_constraints.size()) {
        return vv_constraints[idx];
    }
    idx -= vv_constraints.size();
    if (idx < ev_constraints.size()) {
        return ev_constraints[idx];
    }
    idx -= ev_constraints.size();
    if (idx < ee_constraints.size()) {
        return ee_constraints[idx];
    }
    idx -= ee_constraints.size();
    if (idx < fv_constraints.size()) {
        return fv_constraints[idx];
    }
    idx -= fv_constraints.size();
    if (idx < pv_constraints.size()) {
        return pv_constraints[idx];
    }
    throw std::out_of_range("Constraint index is out of range!");
}

const CollisionConstraint& Constraints::operator[](size_t idx) const
{
    if (idx < 0) {
        throw std::out_of_range("Constraint index is out of range!");
    }
    if (idx < vv_constraints.size()) {
        return vv_constraints[idx];
    }
    idx -= vv_constraints.size();
    if (idx < ev_constraints.size()) {
        return ev_constraints[idx];
    }
    idx -= ev_constraints.size();
    if (idx < ee_constraints.size()) {
        return ee_constraints[idx];
    }
    idx -= ee_constraints.size();
    if (idx < fv_constraints.size()) {
        return fv_constraints[idx];
    }
    idx -= fv_constraints.size();
    if (idx < pv_constraints.size()) {
        return pv_constraints[idx];
    }
    throw std::out_of_range("Constraint index is out of range!");
}

} // namespace ipc
