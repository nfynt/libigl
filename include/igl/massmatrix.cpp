#include "massmatrix.h"
#include "normalize_row_sums.h"
#include "sparse.h"
#include "repmat.h"
#include <Eigen/Geometry>
#include <iostream>

template <typename DerivedV, typename DerivedF, typename Scalar>
IGL_INLINE void igl::massmatrix(
  const Eigen::MatrixBase<DerivedV> & V, 
  const Eigen::MatrixBase<DerivedF> & F, 
  const MassMatrixType type,
  Eigen::SparseMatrix<Scalar>& M)
{
  using namespace Eigen;
  using namespace std;
  assert(type!=MASSMATRIX_FULL);

  const int n = V.rows();
  const int m = F.rows();
  const int simplex_size = F.cols();

  Matrix<int,Dynamic,1> MI;
  Matrix<int,Dynamic,1> MJ;
  Matrix<Scalar,Dynamic,1> MV;
  if(simplex_size == 3)
  {
    // Triangles
    // edge lengths numbered same as opposite vertices
    Matrix<Scalar,Dynamic,3> l(m,3);
    // loop over faces
    for(int i = 0;i<m;i++)
    {
      l(i,0) = sqrt((V.row(F(i,1))-V.row(F(i,2))).array().pow(2).sum());
      l(i,1) = sqrt((V.row(F(i,2))-V.row(F(i,0))).array().pow(2).sum());
      l(i,2) = sqrt((V.row(F(i,0))-V.row(F(i,1))).array().pow(2).sum());
    }
    // semiperimeters
    Matrix<Scalar,Dynamic,1> s = l.rowwise().sum()*0.5;
    assert(s.rows() == m);
    // Heron's forumal for area
    Matrix<Scalar,Dynamic,1> dblA(m);
    for(int i = 0;i<m;i++)
    {
      dblA(i) = 2.0*sqrt(s(i)*(s(i)-l(i,0))*(s(i)-l(i,1))*(s(i)-l(i,2)));
    }

    switch(type)
    {
      case MASSMATRIX_BARYCENTRIC:
        // diagonal entries for each face corner
        MI.resize(m*3,1); MJ.resize(m*3,1); MV.resize(m*3,1);
        MI.block(0*m,0,m,1) = F.col(0);
        MI.block(1*m,0,m,1) = F.col(1);
        MI.block(2*m,0,m,1) = F.col(2);
        MJ = MI;
        repmat(dblA,3,1,MV);
        MV.array() /= 6.0;
        break;
      case MASSMATRIX_VORONOI:
        {
          // diagonal entries for each face corner
          // http://www.alecjacobson.com/weblog/?p=874
          MI.resize(m*3,1); MJ.resize(m*3,1); MV.resize(m*3,1);
          MI.block(0*m,0,m,1) = F.col(0);
          MI.block(1*m,0,m,1) = F.col(1);
          MI.block(2*m,0,m,1) = F.col(2);
          MJ = MI;

          // Holy shit this needs to be cleaned up and optimized
          Matrix<Scalar,Dynamic,3> cosines(m,3);
          cosines.col(0) = 
            (l.col(2).array().pow(2)+l.col(1).array().pow(2)-l.col(0).array().pow(2))/(l.col(1).array()*l.col(2).array()*2.0);
          cosines.col(1) = 
            (l.col(0).array().pow(2)+l.col(2).array().pow(2)-l.col(1).array().pow(2))/(l.col(2).array()*l.col(0).array()*2.0);
          cosines.col(2) = 
            (l.col(1).array().pow(2)+l.col(0).array().pow(2)-l.col(2).array().pow(2))/(l.col(0).array()*l.col(1).array()*2.0);
          Matrix<Scalar,Dynamic,3> barycentric = cosines.array() * l.array();
          normalize_row_sums(barycentric,barycentric);
          cout<<"barycentric=["<<barycentric<<"];"<<endl;
          Matrix<Scalar,Dynamic,3> partial = barycentric;
          partial.col(0).array() *= dblA.array() * 0.5;
          partial.col(1).array() *= dblA.array() * 0.5;
          partial.col(2).array() *= dblA.array() * 0.5;
          cout<<"partial=["<<partial<<"];"<<endl;
          Matrix<Scalar,Dynamic,3> quads(partial.rows(),partial.cols());
          quads.col(0) = (partial.col(1)+partial.col(2))*0.5;
          quads.col(1) = (partial.col(2)+partial.col(0))*0.5;
          quads.col(2) = (partial.col(0)+partial.col(1))*0.5;
          cout<<"quads=["<<quads<<"];"<<endl;

          Matrix<Scalar,Dynamic,3> quads_clamped(quads.rows(),quads.cols());
          quads_clamped.col(0) = (cosines.col(0).array()<0).select( 0.25*dblA,quads.col(0));
          quads_clamped.col(1) = (cosines.col(0).array()<0).select(0.125*dblA,quads.col(1));
          quads_clamped.col(2) = (cosines.col(0).array()<0).select(0.125*dblA,quads.col(2));
          quads_clamped.col(0) = (cosines.col(1).array()<0).select(0.125*dblA,quads.col(0));
          quads_clamped.col(1) = (cosines.col(1).array()<0).select(0.125*dblA,quads.col(1));
          quads_clamped.col(2) = (cosines.col(1).array()<0).select( 0.25*dblA,quads.col(2));
          quads_clamped.col(0) = (cosines.col(2).array()<0).select(0.125*dblA,quads.col(0));
          quads_clamped.col(1) = (cosines.col(2).array()<0).select(0.125*dblA,quads.col(1));
          quads_clamped.col(2) = (cosines.col(2).array()<0).select( 0.25*dblA,quads.col(2));

          MV.block(0*m,0,m,1) = quads_clamped.col(0);
          MV.block(1*m,0,m,1) = quads_clamped.col(1);
          MV.block(2*m,0,m,1) = quads_clamped.col(2);
          
          break;
        }
      case MASSMATRIX_FULL:
        assert(false);
        break;
      default:
        assert(false);
    }

  }else if(simplex_size == 4)
  {
    assert(V.cols() == 3);
    assert(type == MASSMATRIX_BARYCENTRIC);
    MI.resize(m*4,1); MJ.resize(m*4,1); MV.resize(m*4,1);
    MI.block(0*m,0,m,1) = F.col(0);
    MI.block(1*m,0,m,1) = F.col(1);
    MI.block(2*m,0,m,1) = F.col(2);
    MI.block(3*m,0,m,1) = F.col(3);
    MJ = MI;
    // loop over tets
    for(int i = 0;i<m;i++)
    {
      // http://en.wikipedia.org/wiki/Tetrahedron#Volume
      Matrix<Scalar,3,1> v0m3 = V.row(F(i,0)) - V.row(F(i,3));
      Matrix<Scalar,3,1> v1m3 = V.row(F(i,1)) - V.row(F(i,3));
      Matrix<Scalar,3,1> v2m3 = V.row(F(i,2)) - V.row(F(i,3));
      Scalar v = fabs(v0m3.dot(v1m3.cross(v2m3)))/6.0;
      MV(i+0*m) = v/4.0;
      MV(i+1*m) = v/4.0;
      MV(i+2*m) = v/4.0;
      MV(i+3*m) = v/4.0;
    }
  }else
  {
    // Unsupported simplex size
    assert(false);
  }
  sparse(MI,MJ,MV,n,n,M);
}

#ifndef IGL_HEADER_ONLY
// Explicit template specialization
template void igl::massmatrix<Eigen::Matrix<double, -1, -1, 0, -1, -1>, Eigen::Matrix<int, -1, -1, 0, -1, -1>, double>(Eigen::MatrixBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> > const&, Eigen::MatrixBase<Eigen::Matrix<int, -1, -1, 0, -1, -1> > const&, igl::MassMatrixType, Eigen::SparseMatrix<double, 0, int>&);
#endif
