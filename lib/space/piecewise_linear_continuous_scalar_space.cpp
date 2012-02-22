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

#include "piecewise_linear_continuous_scalar_space.hpp"

#include "../grid/entity.hpp"
#include "../grid/entity_iterator.hpp"
#include "../grid/grid.hpp"
#include "../grid/grid_view.hpp"

#include <dune/localfunctions/lagrange/p1/p1localbasis.hh>
#include <dune/localfunctions/lagrange/q1/q1localbasis.hh>

#include <stdexcept>

namespace Bempp
{

template <typename ValueType>
PiecewiseLinearContinuousScalarSpace<ValueType>::
PiecewiseLinearContinuousScalarSpace(Grid& grid) :
     Space<ValueType>(grid)
{
    const int gridDim = grid.dim();
    if (gridDim != 1 && gridDim != 2)
        throw std::invalid_argument("PiecewiseLinearContinuousScalarSpace::"
                                    "PiecewiseLinearContinuousScalarSpace(): "
                                    "only 1- and 2-dimensional grids are supported");
    m_view = this->m_grid.leafView();
}

template <typename ValueType>
int PiecewiseLinearContinuousScalarSpace<ValueType>::domainDimension() const
{
    return this->m_grid.dim();
}

template <typename ValueType>
int PiecewiseLinearContinuousScalarSpace<ValueType>::codomainDimension() const
{
    return 1;
}

template <typename ValueType>
void PiecewiseLinearContinuousScalarSpace<ValueType>::getBases(
        const std::vector<const EntityPointer<0>*>& elements,
        std::vector<const Fiber::Basis<ValueType>*> bases) const
{
    const int elementCount = elements.size();
    bases.resize(elementCount);
    for (int i = 0; i < elementCount; ++i)
        switch (elementVariant(elements[i]->entity()))
        {
        case 3:
            bases[i] = &m_triangleBasis;
        case 4:
            bases[i] = &m_quadrilateralBasis;
        case 2:
            bases[i] = &m_lineBasis;
        }
}

template <typename ValueType>
const Fiber::Basis<ValueType>&
PiecewiseLinearContinuousScalarSpace<ValueType>::basis(
        const EntityPointer<0>& element) const
{
    switch (elementVariant(element.entity()))
    {
    case 3:
        return m_triangleBasis;
    case 4:
        return m_quadrilateralBasis;
    case 2:
        return m_lineBasis;
    }
}

template <typename ValueType>
ElementVariant PiecewiseLinearContinuousScalarSpace<ValueType>::elementVariant(
        const Entity<0>& element) const
{
    GeometryType type = element.type();
    if (type.isLine())
        return 2;
    else if (type.isTriangle())
        return 3;
    else if (type.isQuadrilateral())
        return 4;
    else
        throw std::runtime_error("PiecewiseLinearContinuousScalarSpace::"
                                 "elementVariant(): invalid geometry type, "
                                 "this shouldn't happen!");
}

template <typename ValueType>
void PiecewiseLinearContinuousScalarSpace<ValueType>::setElementVariant(
        const Entity<0>& element, ElementVariant variant)
{
    if (variant != elementVariant(element))
        // for this space, the element variants are unmodifiable,
        throw std::runtime_error("PiecewiseLinearContinuousScalarSpace::"
                                 "setElementVariant(): invalid variant");
}

template <typename ValueType>
void PiecewiseLinearContinuousScalarSpace<ValueType>::assignDofs()
{
    const int gridDim = domainDimension();

    // m_view = this->m_grid.leafView();
    const IndexSet& indexSet = m_view->indexSet();

    // Global DOF numbers will be identical with vertex indices.
    // Thus, the will be as many global DOFs as there are vertices.
    int globalDofCount_ = m_view->entityCount(this->m_grid.dim());

    // (Re)initialise DOF maps
    m_local2globalDofs.clear();
    m_global2localDofs.clear();
    m_global2localDofs.resize(globalDofCount_);
    // TODO: consider calling reserve(x) for each element of m_global2localDofs
    // with x being the typical number of elements adjacent to a vertex in a
    // grid of dimension gridDim

    // Iterate over elements
    std::auto_ptr<EntityIterator<0> > it = m_view->entityIterator<0>();
    int vertexCount;
    while (!it->finished())
    {
        const Entity<0>& element = it->entity();
        EntityIndex elementIndex(element.type(), indexSet.entityIndex(element));

        if (gridDim == 1)
            vertexCount = element.template subEntityCount<1>();
        else // gridDim == 2
            vertexCount = element.template subEntityCount<2>();

        // List of global DOF indices corresponding to the local DOFs of the
        // current element
        std::vector<GlobalDofIndex>& globalDofs = m_local2globalDofs[elementIndex];
        globalDofs.resize(vertexCount);
        for (int i = 0; i < vertexCount; ++i)
        {
            int globalDofIndex = indexSet.subEntityIndex(element, i, gridDim);
            globalDofs[i] = globalDofIndex;
            m_global2localDofs[globalDofIndex].push_back(LocalDof(elementIndex, i));
        }
        it->next();
    }
}

template <typename ValueType>
bool PiecewiseLinearContinuousScalarSpace<ValueType>::dofsAssigned() const
{
    const int gridDim = domainDimension();
    return globalDofCount() == m_view->entityCount(gridDim);
}

template <typename ValueType>
int PiecewiseLinearContinuousScalarSpace<ValueType>::globalDofCount() const
{
    return m_global2localDofs.size();
}

template <typename ValueType>
void PiecewiseLinearContinuousScalarSpace<ValueType>::globalDofs(
        const Entity<0>& element, std::vector<GlobalDofIndex>& dofs) const
{
    const IndexSet& indexSet = m_view->indexSet();
    EntityIndex index(element.type(),
                      indexSet.entityIndex(element));
    GlobalDofMap::const_iterator it = m_local2globalDofs.find(index);
    if (it != m_local2globalDofs.end())
        dofs = it->second;
    else
        dofs.clear();
}

template <typename ValueType>
void PiecewiseLinearContinuousScalarSpace<ValueType>::global2localDofs(
        const std::vector<GlobalDofIndex>& globalDofs,
        std::vector<std::vector<LocalDof> >& localDofs) const
{
    localDofs.resize(globalDofs.size());
    for (int i = 0; i < globalDofs.size(); ++i)
        localDofs[i] = m_global2localDofs[globalDofs[i]];
}

template <typename ValueType>
void PiecewiseLinearContinuousScalarSpace<ValueType>::globalDofPositions(
        std::vector<Point3D>& positions) const
{
    throw NotImplementedError("PiecewiseLinearContinuousScalarSpace::"
                              "globalDofPositions(): "
                              "not implemented");
}


#ifdef COMPILE_FOR_FLOAT
template class PiecewiseLinearContinuousScalarSpace<float>;
#endif
#ifdef COMPILE_FOR_DOUBLE
template class PiecewiseLinearContinuousScalarSpace<double>;
#endif
#ifdef COMPILE_FOR_COMPLEX_FLOAT
#include <complex>
template class PiecewiseLinearContinuousScalarSpace<std::complex<float> >;
#endif
#ifdef COMPILE_FOR_COMPLEX_DOUBLE
#include <complex>
template class PiecewiseLinearContinuousScalarSpace<std::complex<double> >;
#endif

} // namespace Bempp
