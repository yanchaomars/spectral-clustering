// Copyright (C) 2015 Yixuan Qiu <yixuan.qiu@cos.name>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef DOUBLE_SHIFT_QR_H
#define DOUBLE_SHIFT_QR_H

#include <Eigen/Core>
#include <vector>     // std::vector
#include <algorithm>  // std::min, std::fill, std::copy
#include <cmath>      // std::abs, std::sqrt, std::pow
#include <limits>     // std::numeric_limits
#include <stdexcept>  // std::invalid_argument, std::logic_error

namespace Spectra {


template <typename Scalar = double>
class DoubleShiftQR
{
private:
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> Matrix;
    typedef Eigen::Matrix<Scalar, 3, Eigen::Dynamic> Matrix3X;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1> Vector;
    typedef Eigen::Array<unsigned char, Eigen::Dynamic, 1> IntArray;

    typedef typename Matrix::Index Index;

    typedef Eigen::Ref<Matrix> GenericMatrix;
    typedef const Eigen::Ref<const Matrix> ConstGenericMatrix;

    Index m_n;            // Dimension of the matrix
    Matrix m_mat_H;       // A copy of the matrix to be factorized
    Scalar m_shift_s;     // Shift constant
    Scalar m_shift_t;     // Shift constant
    Matrix3X m_ref_u;     // Householder reflectors
    IntArray m_ref_nr;    // How many rows does each reflector affects
                          // 3 - A general reflector
                          // 2 - A Givens rotation
                          // 1 - An identity transformation
    const Scalar m_prec;  // Approximately zero
    const Scalar m_eps_rel;
    const Scalar m_eps_abs;
    bool m_computed;      // Whether matrix has been factorized

    void compute_reflector(const Scalar &x1, const Scalar &x2, const Scalar &x3, Index ind)
    {
        Scalar *u = m_ref_u.data() + 3 * ind;
        unsigned char *nr = m_ref_nr.data();
        // In general case the reflector affects 3 rows
        nr[ind] = 3;
        // If x3 is zero, decrease nr by 1
        if(std::abs(x3) < m_prec)
        {
            // If x2 is also zero, nr will be 1, and we can exit this function
            if(std::abs(x2) < m_prec)
            {
                nr[ind] = 1;
                return;
            } else {
                nr[ind] = 2;
            }
        }

        // x1' = x1 - rho * ||x||
        // rho = -sign(x1), if x1 == 0, we choose rho = 1
        Scalar tmp = x2 * x2 + x3 * x3;
        Scalar x1_new = x1 - ((x1 <= 0) - (x1 > 0)) * std::sqrt(x1 * x1 + tmp);
        Scalar x_norm = std::sqrt(x1_new * x1_new + tmp);
        // Double check the norm of new x
        if(x_norm < m_prec)
        {
            nr[ind] = 1;
            return;
        }
        u[0] = x1_new / x_norm;
        u[1] = x2 / x_norm;
        u[2] = x3 / x_norm;
    }

    void compute_reflector(const Scalar *x, Index ind)
    {
        compute_reflector(x[0], x[1], x[2], ind);
    }

    // Update the block X = H(il:iu, il:iu)
    void update_block(Index il, Index iu)
    {
        // Block size
        Index bsize = iu - il + 1;

        // If block size == 1, there is no need to apply reflectors
        if(bsize == 1)
        {
            m_ref_nr[il] = 1;
            return;
        }

        // For block size == 2, do a Givens rotation on M = X * X - s * X + t * I
        if(bsize == 2)
        {
            // m00 = x00 * (x00 - s) + x01 * x10 + t
            Scalar m00 = m_mat_H.coeff(il, il) * (m_mat_H.coeff(il, il) - m_shift_s) +
                         m_mat_H.coeff(il, il + 1) * m_mat_H.coeff(il + 1, il) +
                         m_shift_t;
            // m10 = x10 * (x00 + x11 - s)
            Scalar m10 = m_mat_H.coeff(il + 1, il) * (m_mat_H.coeff(il, il) + m_mat_H.coeff(il + 1, il + 1) - m_shift_s);
            // This causes nr=2
            compute_reflector(m00, m10, 0, il);
            // Apply the reflector to X
            apply_PX(m_mat_H.block(il, il, 2, m_n - il), m_n, il);
            apply_XP(m_mat_H.block(0, il, il + 2, 2), m_n, il);

            m_ref_nr[il + 1] = 1;
            return;
        }

        // For block size >=3, use the regular strategy
        Scalar m00 = m_mat_H.coeff(il, il) * (m_mat_H.coeff(il, il) - m_shift_s) +
                     m_mat_H.coeff(il, il + 1) * m_mat_H.coeff(il + 1, il) +
                     m_shift_t;
        Scalar m10 = m_mat_H.coeff(il + 1, il) * (m_mat_H.coeff(il, il) + m_mat_H.coeff(il + 1, il + 1) - m_shift_s);
        // m20 = x21 * x10
        Scalar m20 = m_mat_H.coeff(il + 2, il + 1) * m_mat_H.coeff(il + 1, il);
        compute_reflector(m00, m10, m20, il);

        // Apply the first reflector
        apply_PX(m_mat_H.block(il, il, 3, m_n - il), m_n, il);
        apply_XP(m_mat_H.block(0, il, il + std::min(bsize, Index(4)), 3), m_n, il);

        // Calculate the following reflectors
        // If entering this loop, block size is at least 4.
        for(Index i = 1; i < bsize - 2; i++)
        {
            compute_reflector(&m_mat_H.coeffRef(il + i, il + i - 1), il + i);
            // Apply the reflector to X
            apply_PX(m_mat_H.block(il + i, il + i - 1, 3, m_n - il - i + 1), m_n, il + i);
            apply_XP(m_mat_H.block(0, il + i, il + std::min(bsize, Index(i + 4)), 3), m_n, il + i);
        }

        // The last reflector
        // This causes nr=2
        compute_reflector(m_mat_H.coeff(iu - 1, iu - 2), m_mat_H.coeff(iu, iu - 2), 0, iu - 1);
        // Apply the reflector to X
        apply_PX(m_mat_H.block(iu - 1, iu - 2, 2, m_n - iu + 2), m_n, iu - 1);
        apply_XP(m_mat_H.block(0, iu - 1, il + bsize, 2), m_n, iu - 1);

        m_ref_nr[iu] = 1;
    }

    // P = I - 2 * u * u' = P'
    // PX = X - 2 * u * (u'X)
    void apply_PX(GenericMatrix X, Index stride, Index u_ind)
    {
        if(m_ref_nr[u_ind] == 1)
            return;

        Scalar *u = m_ref_u.data() + 3 * u_ind;

        const Index nrow = X.rows();
        const Index ncol = X.cols();
        const Scalar u0_2 = 2 * u[0];
        const Scalar u1_2 = 2 * u[1];

        Scalar *xptr = X.data();
        if(m_ref_nr[u_ind] == 2 || nrow == 2)
        {
            for(Index i = 0; i < ncol; i++, xptr += stride)
            {
                Scalar tmp = u0_2 * xptr[0] + u1_2 * xptr[1];
                xptr[0] -= tmp * u[0];
                xptr[1] -= tmp * u[1];
            }
        } else {
            const Scalar u2_2 = 2 * u[2];
            for(Index i = 0; i < ncol; i++, xptr += stride)
            {
                Scalar tmp = u0_2 * xptr[0] + u1_2 * xptr[1] + u2_2 * xptr[2];
                xptr[0] -= tmp * u[0];
                xptr[1] -= tmp * u[1];
                xptr[2] -= tmp * u[2];
            }
        }
    }

    // x is a pointer to a vector
    // Px = x - 2 * dot(x, u) * u
    void apply_PX(Scalar *x, Index u_ind)
    {
        if(m_ref_nr[u_ind] == 1)
            return;

        Scalar u0 = m_ref_u(0, u_ind),
               u1 = m_ref_u(1, u_ind),
               u2 = m_ref_u(2, u_ind);

        // When the reflector only contains two elements, u2 has been set to zero
        bool nr_is_2 = (m_ref_nr[u_ind] == 2);
        Scalar dot2 = x[0] * u0 + x[1] * u1 + (nr_is_2 ? 0 : (x[2] * u2));
        dot2 *= 2;
        x[0] -= dot2 * u0;
        x[1] -= dot2 * u1;
        if(!nr_is_2)
            x[2] -= dot2 * u2;
    }

    // XP = X - 2 * (X * u) * u'
    void apply_XP(GenericMatrix X, Index stride, Index u_ind)
    {
        if(m_ref_nr[u_ind] == 1)
            return;

        Scalar *u = m_ref_u.data() + 3 * u_ind;
        const int nrow = X.rows();
        const int ncol = X.cols();
        const Scalar u0_2 = 2 * u[0];
        const Scalar u1_2 = 2 * u[1];
        Scalar *X0 = X.data(), *X1 = X0 + stride;  // X0 => X.col(0), X1 => X.col(1)

        if(m_ref_nr[u_ind] == 2 || ncol == 2)
        {
            // tmp = 2 * u0 * X0 + 2 * u1 * X1
            // X0 => X0 - u0 * tmp
            // X1 => X1 - u1 * tmp
            for(Index i = 0; i < nrow; i++)
            {
                Scalar tmp = u0_2 * X0[i] + u1_2 * X1[i];
                X0[i] -= tmp * u[0];
                X1[i] -= tmp * u[1];
            }
        } else {
            Scalar *X2 = X1 + stride;  // X2 => X.col(2)
            const Scalar u2_2 = 2 * u[2];
            for(Index i = 0; i < nrow; i++)
            {
                Scalar tmp = u0_2 * X0[i] + u1_2 * X1[i] + u2_2 * X2[i];
                X0[i] -= tmp * u[0];
                X1[i] -= tmp * u[1];
                X2[i] -= tmp * u[2];
            }
        }
    }

public:
    DoubleShiftQR(Index size) :
        m_n(size),
        m_prec(std::numeric_limits<Scalar>::epsilon()),
        m_eps_rel(std::pow(m_prec, Scalar(2.0) / 3)),
        m_eps_abs(std::min(std::pow(m_prec, Scalar(3.0) / 4), m_n * m_prec)),
        m_computed(false)
    {}

    DoubleShiftQR(ConstGenericMatrix &mat, Scalar s, Scalar t) :
        m_n(mat.rows()),
        m_mat_H(m_n, m_n),
        m_shift_s(s),
        m_shift_t(t),
        m_ref_u(3, m_n),
        m_ref_nr(m_n),
        m_prec(std::numeric_limits<Scalar>::epsilon()),
        m_eps_rel(std::pow(m_prec, Scalar(2.0) / 3)),
        m_eps_abs(std::min(std::pow(m_prec, Scalar(3.0) / 4), m_n * m_prec)),
        m_computed(false)
    {
        compute(mat, s, t);
    }

    void compute(ConstGenericMatrix &mat, Scalar s, Scalar t)
    {
        if(mat.rows() != mat.cols())
            throw std::invalid_argument("DoubleShiftQR: matrix must be square");

        m_n = mat.rows();
        m_mat_H.resize(m_n, m_n);
        m_shift_s = s;
        m_shift_t = t;
        m_ref_u.resize(3, m_n);
        m_ref_nr.resize(m_n);

        // Make a copy of mat
        std::copy(mat.data(), mat.data() + mat.size(), m_mat_H.data());

        // Obtain the indices of zero elements in the subdiagonal,
        // so that H can be divided into several blocks
        std::vector<int> zero_ind;
        zero_ind.reserve(m_n - 1);
        zero_ind.push_back(0);
        Scalar *Hii = m_mat_H.data();
        for(Index i = 0; i < m_n - 2; i++, Hii += (m_n + 1))
        {
            // Hii[1] => m_mat_H(i + 1, i)
            const Scalar h = std::abs(Hii[1]);
            if(h <= m_eps_abs || h <= m_eps_rel * (std::abs(Hii[0]) + std::abs(Hii[m_n + 1])))
            {
                Hii[1] = 0;
                zero_ind.push_back(i + 1);
            }
            // Make sure m_mat_H is upper Hessenberg
            // Zero the elements below m_mat_H(i + 1, i)
            std::fill(Hii + 2, Hii + m_n - i, Scalar(0));
        }
        zero_ind.push_back(m_n);

        for(std::vector<int>::size_type i = 0; i < zero_ind.size() - 1; i++)
        {
            Index start = zero_ind[i];
            Index end = zero_ind[i + 1] - 1;
            // Compute refelctors and update each block
            update_block(start, end);
        }

        m_computed = true;
    }

    Matrix matrix_QtHQ()
    {
        if(!m_computed)
            throw std::logic_error("DoubleShiftQR: need to call compute() first");

        return m_mat_H;
    }

    // Q = P0 * P1 * ...
    // Q'y = P_{n-2} * ... * P1 * P0 * y
    void apply_QtY(Vector &y)
    {
        if(!m_computed)
            throw std::logic_error("DoubleShiftQR: need to call compute() first");

        Scalar *y_ptr = y.data();
        for(Index i = 0; i < m_n - 1; i++, y_ptr++)
        {
            apply_PX(y_ptr, i);
        }
    }

    // Q = P0 * P1 * ...
    // YQ = Y * P0 * P1 * ...
    void apply_YQ(GenericMatrix Y)
    {
        if(!m_computed)
            throw std::logic_error("DoubleShiftQR: need to call compute() first");

        Index nrow = Y.rows();
        for(Index i = 0; i < m_n - 2; i++)
        {
            apply_XP(Y.block(0, i, nrow, 3), nrow, i);
        }
        apply_XP(Y.block(0, m_n - 2, nrow, 2), nrow, m_n - 2);
    }
};


} // namespace Spectra

#endif // DOUBLE_SHIFT_QR_H
