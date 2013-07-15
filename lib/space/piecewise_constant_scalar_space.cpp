// Copyright (C) 2011-2012 by the BEM++ Authors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "piecewise_constant_scalar_space.hpp"

#include "../common/acc.hpp"
#include "../common/bounding_box_helpers.hpp"
#include "../common/not_implemented_error.hpp"
#include "../fiber/explicit_instantiation.hpp"
#include "../grid/entity.hpp"
#include "../grid/entity_iterator.hpp"
#include "../grid/geometry.hpp"
#include "../grid/grid.hpp"
#include "../grid/grid_segment.hpp"
#include "../grid/grid_view.hpp"
#include "../grid/mapper.hpp"
#include "../grid/vtk_writer.hpp"

namespace Bempp
{

template <typename BasisFunctionType>
PiecewiseConstantScalarSpace<BasisFunctionType>::
PiecewiseConstantScalarSpace(const shared_ptr<const Grid>& grid) :
     ScalarSpace<BasisFunctionType>(grid), m_view(grid->leafView())
{
    assignDofsImpl(GridSegment::wholeGrid(*grid));
}

template <typename BasisFunctionType>
PiecewiseConstantScalarSpace<BasisFunctionType>::
PiecewiseConstantScalarSpace(const shared_ptr<const Grid>& grid,
                             const GridSegment& segment) :
     ScalarSpace<BasisFunctionType>(grid), m_view(grid->leafView())
{
    assignDofsImpl(segment);
}

template <typename BasisFunctionType>
shared_ptr<const Space<BasisFunctionType> >
PiecewiseConstantScalarSpace<BasisFunctionType>::discontinuousSpace(
    const shared_ptr<const Space<BasisFunctionType> >& self) const
{
    if (self.get() != this)
        throw std::invalid_argument(
            "PiecewiseConstantScalarSpace::discontinuousSpace(): "
            "argument should be a shared pointer to *this");
    return self;
}

template <typename BasisFunctionType>
bool
PiecewiseConstantScalarSpace<BasisFunctionType>::isDiscontinuous() const
{
    return true;
}

template <typename BasisFunctionType>
int PiecewiseConstantScalarSpace<BasisFunctionType>::domainDimension() const
{
    return this->grid()->dim();
}

template <typename BasisFunctionType>
int PiecewiseConstantScalarSpace<BasisFunctionType>::codomainDimension() const
{
    return 1;
}

template <typename BasisFunctionType>
const Fiber::Basis<BasisFunctionType>&
PiecewiseConstantScalarSpace<BasisFunctionType>::basis(
        const Entity<0>& element) const
{
    return m_basis;
}

template <typename BasisFunctionType>
ElementVariant PiecewiseConstantScalarSpace<BasisFunctionType>::elementVariant(
        const Entity<0>& element) const
{
    GeometryType type = element.type();
    if (type.dim() == 1)
        return 2;
    if (type.isTriangle())
        return 3;
    else
        return 4;
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::setElementVariant(
        const Entity<0>& element, ElementVariant variant)
{
    if (variant != elementVariant(element))
        // for this space, the element variants are unmodifiable,
        throw std::runtime_error("PiecewiseConstantScalarSpace::"
                                 "setElementVariant(): invalid variant");
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::assignDofsImpl(
        const GridSegment& segment)
{
    const Mapper& mapper = m_view->elementMapper();
    std::auto_ptr<EntityIterator<0> > it = m_view->entityIterator<0>();

    // List of local DOFs corresponding to a single global DOF.
    std::vector<LocalDof> localDofs(1);
    // List of global DOF indices corresponding to the local DOFs of a single
    // element
    std::vector<GlobalDofIndex> globalDofs(1);
    // For this space, there is a one-to-one mapping between the local and
    // global DOFs, so the above vectors consist of one element only.

    // (Re)initialise member variables
    const int elementCount = m_view->entityCount(0);
    m_local2globalDofs.clear();
    m_local2globalDofs.resize(elementCount);
    m_global2localDofs.clear();
    m_global2localDofs.reserve(elementCount);

    int globalDofCount_ = 0;
    while (!it->finished())
    {
        EntityIndex index = mapper.entityIndex(it->entity());
        if (segment.contains(0, index)) {
            std::cout << "contains " << index << "\n";
            localDofs[0] = LocalDof(index, 0 /* local DOF #0 */);
            m_global2localDofs.push_back(localDofs);
            globalDofs[0] = globalDofCount_++;
        } else {
            std::cout << "does not contain " << index << "\n";
            globalDofs[0] = -1;
        }
        m_local2globalDofs[index] = globalDofs;
        it->next();
    }
    // TODO: bboxes
}

template <typename BasisFunctionType>
size_t PiecewiseConstantScalarSpace<BasisFunctionType>::globalDofCount() const
{
    return m_global2localDofs.size();
}

template <typename BasisFunctionType>
size_t PiecewiseConstantScalarSpace<BasisFunctionType>::flatLocalDofCount() const
{
    return m_view->entityCount(0);
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::getGlobalDofs(
        const Entity<0>& element, std::vector<GlobalDofIndex>& dofs) const
{
    const Mapper& mapper = m_view->elementMapper();
    EntityIndex index = mapper.entityIndex(element);
    dofs = m_local2globalDofs[index];
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::global2localDofs(
        const std::vector<GlobalDofIndex>& globalDofs,
        std::vector<std::vector<LocalDof> >& localDofs) const
{
    localDofs.resize(globalDofs.size());
    for (size_t i = 0; i < globalDofs.size(); ++i)
        localDofs[i] = m_global2localDofs[globalDofs[i]];
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::flatLocal2localDofs(
        const std::vector<FlatLocalDofIndex>& flatLocalDofs,
        std::vector<LocalDof>& localDofs) const
{
    // Use the fact that each element contains exactly one DOF
    localDofs.resize(flatLocalDofs.size());
    for (size_t i = 0; i < flatLocalDofs.size(); ++i)
        localDofs[i] = LocalDof(flatLocalDofs[i], 0 /* local DOF #0 */);
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::getGlobalDofPositions(
        std::vector<Point3D<CoordinateType> >& positions) const
{
    std::vector<BoundingBox<CoordinateType> > bboxes;
    getGlobalDofBoundingBoxes(bboxes);

    positions.resize(bboxes.size());
    for (int i = 0; i < positions.size(); ++i)
        positions[i] = bboxes[i].reference;
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::getFlatLocalDofPositions(
        std::vector<Point3D<CoordinateType> >& positions) const
{
    return getGlobalDofPositions(positions);
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::getGlobalDofBoundingBoxes(
       std::vector<BoundingBox<CoordinateType> >& bboxes) const
{
    // NOTE: this has been copied from PiecewiseLinearContinuousScalarSpace.
    // Consider putting this as a default implementation somewhere.
    const IndexSet& indexSet = m_view->indexSet();
    const int elementCount = m_view->entityCount(0);

    std::vector<arma::Mat<CoordinateType> > elementCorners(elementCount);
    std::auto_ptr<EntityIterator<0> > it = m_view->entityIterator<0>();
    while (!it->finished()) {
        const Entity<0>& e = it->entity();
        int index = indexSet.entityIndex(e);
        const Geometry& geo = e.geometry();
        geo.getCorners(acc(elementCorners, index));
        it->next();
    }

    BoundingBox<CoordinateType> model;
    const CoordinateType maxCoord = std::numeric_limits<CoordinateType>::max();
    model.lbound.x = model.lbound.y = model.lbound.z = maxCoord;
    model.ubound.x = model.ubound.y = model.ubound.z = -maxCoord;

    const int globalDofCount_ = m_global2localDofs.size();
    bboxes.resize(globalDofCount_, model);
    for (int i = 0; i < globalDofCount_; ++i) {
        const std::vector<LocalDof>& localDofs = acc(m_global2localDofs, i);
        BoundingBox<CoordinateType>& bbox = acc(bboxes, i);
        for (int j = 0; j < localDofs.size(); ++j)
            extendBoundingBox(bbox, acc(elementCorners,
                                        acc(localDofs, j).entityIndex));
        assert(!localDofs.empty());
        setBoundingBoxReference<CoordinateType>(
                    bbox,
                    acc(elementCorners, localDofs[0].entityIndex).col(
                        localDofs[0].dofIndex));
    }

#ifndef NDEBUG
   for (size_t i = 0; i < globalDofCount_; ++i) {
       assert(bboxes[i].reference.x >= bboxes[i].lbound.x);
       assert(bboxes[i].reference.y >= bboxes[i].lbound.y);
       assert(bboxes[i].reference.z >= bboxes[i].lbound.z);
       assert(bboxes[i].reference.x <= bboxes[i].ubound.x);
       assert(bboxes[i].reference.y <= bboxes[i].ubound.y);
       assert(bboxes[i].reference.z <= bboxes[i].ubound.z);
   }
#endif // NDEBUG
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::getFlatLocalDofBoundingBoxes(
       std::vector<BoundingBox<CoordinateType> >& bboxes) const
{
    getGlobalDofBoundingBoxes(bboxes);
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::getGlobalDofNormals(
        std::vector<Point3D<CoordinateType> >& normals) const
{
    const int gridDim = domainDimension();
    const int globalDofCount_ = globalDofCount();
    normals.resize(globalDofCount_);

    const Mapper& mapper = m_view->elementMapper();

    arma::Col<CoordinateType> center(gridDim);
    center.fill(0.5);
    arma::Col<CoordinateType> normal;
    if (gridDim == 1)
        throw NotImplementedError(
                "PiecewiseConstantScalarSpace::globalDofPositions(): "
                "not implemented for 2D yet.");
    else {
        std::auto_ptr<EntityIterator<0> > it = m_view->entityIterator<0>();
        while (!it->finished())
        {
            const Entity<0>& e = it->entity();
            int index = mapper.entityIndex(e);
            e.geometry().getNormals(center, normal);

            normals[index].x = normal(0);
            normals[index].y = normal(1);
            normals[index].z = normal(2);
            it->next();
        }
    }
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::getFlatLocalDofNormals(
        std::vector<Point3D<CoordinateType> >& normals) const
{
    return getGlobalDofNormals(normals);
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::dumpClusterIds(
        const char* fileName,
        const std::vector<unsigned int>& clusterIdsOfDofs) const
{
    dumpClusterIdsEx(fileName, clusterIdsOfDofs, GLOBAL_DOFS);
}

template <typename BasisFunctionType>
void PiecewiseConstantScalarSpace<BasisFunctionType>::dumpClusterIdsEx(
        const char* fileName,
        const std::vector<unsigned int>& clusterIdsOfGlobalDofs,
        DofType dofType) const
{
    if (dofType != GLOBAL_DOFS && dofType != FLAT_LOCAL_DOFS)
        throw std::invalid_argument("PiecewiseConstantScalarSpace::"
                                    "dumpClusterIds(): invalid DOF type");
    const size_t idCount = clusterIdsOfGlobalDofs.size();
    if ((dofType == GLOBAL_DOFS && idCount != globalDofCount()) ||
            (dofType == FLAT_LOCAL_DOFS && idCount != flatLocalDofCount()))
        throw std::invalid_argument(
                "PiecewiseConstantScalarSpace::dumpClusterIds(): "
                "clusterIds has incorrect length");

    std::auto_ptr<GridView> view = this->grid()->leafView();
    std::auto_ptr<VtkWriter> vtkWriter = view->vtkWriter();
    arma::Row<double> data(idCount);
    for (size_t i = 0; i < idCount; ++i)
        data(i) = clusterIdsOfGlobalDofs[i];
    vtkWriter->addCellData(data, "ids");
    vtkWriter->write(fileName);
}

FIBER_INSTANTIATE_CLASS_TEMPLATED_ON_BASIS(PiecewiseConstantScalarSpace);

} // namespace Bempp
