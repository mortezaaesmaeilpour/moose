//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ADCompute1DFiniteStrain.h"

#include "libmesh/quadrature.h"

defineADLegacyParams(ADCompute1DFiniteStrain);

template <ComputeStage compute_stage>
InputParameters
ADCompute1DFiniteStrain<compute_stage>::validParams()
{
  InputParameters params = ADComputeFiniteStrain<compute_stage>::validParams();
  params.addClassDescription("Compute strain increment for finite strain in 1D problem");
  return params;
}

template <ComputeStage compute_stage>
ADCompute1DFiniteStrain<compute_stage>::ADCompute1DFiniteStrain(const InputParameters & parameters)
  : ADComputeFiniteStrain<compute_stage>(parameters)
{
}

template <ComputeStage compute_stage>
void
ADCompute1DFiniteStrain<compute_stage>::computeProperties()
{
  for (_qp = 0; _qp < _qrule->n_points(); ++_qp)
  {
    ADRankTwoTensor A((*_grad_disp[0])[_qp],
                      (*_grad_disp[1])[_qp],
                      (*_grad_disp[2])[_qp]); // Deformation gradient
    RankTwoTensor Fbar((*_grad_disp_old[0])[_qp],
                       (*_grad_disp_old[1])[_qp],
                       (*_grad_disp_old[2])[_qp]); // Old Deformation gradient

    // Compute the displacement gradient dUy/dy and dUz/dz value for 1D problems
    A(1, 1) = computeGradDispYY();
    A(2, 2) = computeGradDispZZ();

    Fbar(1, 1) = computeGradDispYYOld();
    Fbar(2, 2) = computeGradDispZZOld();

    A -= Fbar; // very nearly A = gradU - gradUold, adapted to cylindrical coords

    Fbar.addIa(1.0); // Fbar = ( I + gradUold)

    // Incremental deformation gradient _Fhat = I + A Fbar^-1
    _Fhat[_qp] = A * Fbar.inverse();
    _Fhat[_qp].addIa(1.0);
  }

  for (_qp = 0; _qp < _qrule->n_points(); ++_qp)
    computeQpStrain();
}

// explicit instantiation is required for AD base classes
adBaseClass(ADCompute1DFiniteStrain);
