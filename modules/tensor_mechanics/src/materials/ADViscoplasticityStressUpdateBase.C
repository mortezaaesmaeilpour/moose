//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ADViscoplasticityStressUpdateBase.h"

defineADLegacyParams(ADViscoplasticityStressUpdateBase);

template <ComputeStage compute_stage>
InputParameters
ADViscoplasticityStressUpdateBase<compute_stage>::validParams()
{
  InputParameters params = ADStressUpdateBase<compute_stage>::validParams();
  params += ADSingleVariableReturnMappingSolution<RESIDUAL>::validParams();
  params.addClassDescription("Base class used to calculate viscoplastic responses to be used in "
                             "ADComputeMultiplePorousInelasticStress");
  params.addParam<Real>("max_inelastic_increment",
                        1.0e-4,
                        "The maximum inelastic strain increment allowed in a time step");
  params.addParam<std::string>(
      "inelastic_strain_name",
      "viscoplasticity",
      "Name of the material property that stores the effective inelastic strain");
  params.addParam<bool>("verbose", false, "Flag to output verbose information");
  params.addParam<MaterialPropertyName>(
      "porosity_name", "porosity", "Name of porosity material property");
  params.addParam<std::string>("total_strain_base_name", "Base name for the total strain");

  params.addParamNamesToGroup("inelastic_strain_name", "Advanced");
  return params;
}

template <ComputeStage compute_stage>
ADViscoplasticityStressUpdateBase<compute_stage>::ADViscoplasticityStressUpdateBase(
    const InputParameters & parameters)
  : ADStressUpdateBase<compute_stage>(parameters),
    ADSingleVariableReturnMappingSolution<compute_stage>(parameters),
    _total_strain_base_name(isParamValid("total_strain_base_name")
                                ? getParam<std::string>("total_strain_base_name") + "_"
                                : ""),
    _strain_increment(
        getADMaterialProperty<RankTwoTensor>(_total_strain_base_name + "strain_increment")),
    _effective_inelastic_strain(declareADProperty<Real>(
        _base_name + "effective_" + getParam<std::string>("inelastic_strain_name"))),
    _effective_inelastic_strain_old(getMaterialPropertyOld<Real>(
        _base_name + "effective_" + getParam<std::string>("inelastic_strain_name"))),
    _inelastic_strain(declareADProperty<RankTwoTensor>(
        _base_name + getParam<std::string>("inelastic_strain_name"))),
    _inelastic_strain_old(getMaterialPropertyOld<RankTwoTensor>(
        _base_name + getParam<std::string>("inelastic_strain_name"))),
    _max_inelastic_increment(getParam<Real>("max_inelastic_increment")),
    _intermediate_porosity(0.0),
    _porosity_old(getMaterialPropertyOld<Real>(getParam<MaterialPropertyName>("porosity_name"))),
    _verbose(getParam<bool>("verbose")),
    _derivative(0.0)
{
}

template <ComputeStage compute_stage>
void
ADViscoplasticityStressUpdateBase<compute_stage>::initQpStatefulProperties()
{
  _effective_inelastic_strain[_qp] = 0.0;
  _inelastic_strain[_qp].zero();
}

template <ComputeStage compute_stage>
void
ADViscoplasticityStressUpdateBase<compute_stage>::propagateQpStatefulProperties()
{
  _effective_inelastic_strain[_qp] = _effective_inelastic_strain_old[_qp];
  _inelastic_strain[_qp] = _inelastic_strain_old[_qp];
}

template <ComputeStage compute_stage>
Real
ADViscoplasticityStressUpdateBase<compute_stage>::computeReferenceResidual(
    const ADReal & /*effective_trial_stress*/, const ADReal & gauge_stress)
{
  // Use gauge stress for relative tolerance criteria, defined as:
  // std::abs(residual / gauge_stress) <= _relative_tolerance
  return MetaPhysicL::raw_value(gauge_stress);
}

template <ComputeStage compute_stage>
Real
ADViscoplasticityStressUpdateBase<compute_stage>::computeTimeStepLimit()
{
  const Real scalar_inelastic_strain_incr =
      MetaPhysicL::raw_value(_effective_inelastic_strain[_qp]) -
      _effective_inelastic_strain_old[_qp];

  if (!scalar_inelastic_strain_incr)
    return std::numeric_limits<Real>::max();

  return _dt * _max_inelastic_increment / scalar_inelastic_strain_incr;
}

template <ComputeStage compute_stage>
void
ADViscoplasticityStressUpdateBase<compute_stage>::outputIterationSummary(
    std::stringstream * iter_output, const unsigned int total_it)
{
  if (iter_output)
  {
    *iter_output << "At element " << _current_elem->id() << " _qp=" << _qp << " Coordinates "
                 << _q_point[_qp] << " block=" << _current_elem->subdomain_id() << '\n';
  }
  ADSingleVariableReturnMappingSolution<compute_stage>::outputIterationSummary(iter_output,
                                                                               total_it);
}

template <ComputeStage compute_stage>
void
ADViscoplasticityStressUpdateBase<compute_stage>::updateIntermediatePorosity(
    const ADRankTwoTensor & elastic_strain_increment)
{
  // Subtract elastic strain from strain increment to find all inelastic strain increments
  // calculated so far except the one that we're about to calculate
  const ADRankTwoTensor inelastic_volumetric_increment =
      _strain_increment[_qp] - elastic_strain_increment;

  // Calculate intermdiate porosity from all inelastic strain increments calculated so far except
  // the one that we're about to calculate
  _intermediate_porosity =
      (1.0 - _porosity_old[_qp]) * inelastic_volumetric_increment.trace() + _porosity_old[_qp];
}

// explicit instantiation is required for AD base classes
adBaseClass(ADViscoplasticityStressUpdateBase);
