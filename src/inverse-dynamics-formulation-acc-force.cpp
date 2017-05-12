//
// Copyright (c) 2017 CNRS
//
// This file is part of PinInvDyn
// PinInvDyn is free software: you can redistribute it
// and/or modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either version
// 3 of the License, or (at your option) any later version.
// PinInvDyn is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Lesser Public License for more details. You should have
// received a copy of the GNU Lesser General Public License along with
// PinInvDyn If not, see
// <http://www.gnu.org/licenses/>.
//

#include <pininvdyn/inverse-dynamics-formulation-acc-force.hpp>
#include <pininvdyn/math/constraint-bound.hpp>

using namespace std;
using namespace pininvdyn;
using namespace pininvdyn::math;
using namespace pininvdyn::tasks;
using namespace pininvdyn::contacts;
using namespace pininvdyn::solvers;

typedef se3::Data Data;

TaskLevel::TaskLevel(pininvdyn::tasks::TaskBase & task,
                     double weight,
                     unsigned int priority):
  task(task),
  weight(weight),
  priority(priority)
{}


ContactLevel::ContactLevel(pininvdyn::contacts::ContactBase & contact):
  contact(contact)
{}


InverseDynamicsFormulationAccForce::InverseDynamicsFormulationAccForce(const std::string & name,
                                                                       RobotWrapper & robot,
                                                                       bool verbose):
  InverseDynamicsFormulationBase(name, robot, verbose),
  m_data(robot.model()),
  m_baseDynamics("base-dynamics", 6, robot.nv())
{
  m_t = 0.0;
  m_v = robot.nv();
  m_k = 0;
  m_eq = 6;
  m_in = 0;
  m_hqpData.resize(2);
  m_Jc.setZero(m_k, m_v);
  m_hqpData[0].push_back(make_pair<double, ConstraintBase*>(1.0, &m_baseDynamics));
}


const Data & InverseDynamicsFormulationAccForce::data() const
{
  return m_data;
}

unsigned int InverseDynamicsFormulationAccForce::nVar() const
{
  return m_v+m_k;
}

unsigned int InverseDynamicsFormulationAccForce::nEq() const
{
  return m_eq;
}

unsigned int InverseDynamicsFormulationAccForce::nIn() const
{
  return m_in;
}


void InverseDynamicsFormulationAccForce::resizeHqpData()
{
  m_Jc.setZero(m_k, m_v);
  m_baseDynamics.resize(6, m_v+m_k);
  for(HqpData::iterator it=m_hqpData.begin(); it!=m_hqpData.end(); it++)
  {
    for(ConstraintLevel::iterator itt=it->begin(); itt!=it->end(); itt++)
    {
      itt->second->resize(itt->second->rows(), m_v+m_k);
    }
  }
}


void InverseDynamicsFormulationAccForce::addTask(TaskLevel* tl,
                                                 double weight,
                                                 unsigned int priorityLevel)
{
  if(priorityLevel > m_hqpData.size())
    m_hqpData.resize(priorityLevel);
  const ConstraintBase & c = tl->task.getConstraint();
  if(c.isEquality())
  {
    tl->constraint = new ConstraintEquality(c.name(), c.rows(), m_v+m_k);
    if(priorityLevel==0)
      m_eq += c.rows();
  }
  else if(c.isInequality())
  {
    tl->constraint = new ConstraintInequality(c.name(), c.rows(), m_v+m_k);
    if(priorityLevel==0)
      m_in += c.rows();
  }
  else
    tl->constraint = new ConstraintBound(c.name(), m_v+m_k);
  m_hqpData[priorityLevel].push_back(make_pair<double, ConstraintBase*>(weight, tl->constraint));
}


bool InverseDynamicsFormulationAccForce::addMotionTask(TaskMotion & task,
                                                       double weight,
                                                       unsigned int priorityLevel,
                                                       double transition_duration)
{
  assert(weight>=0.0);
  assert(transition_duration>=0.0);
  TaskLevel *tl = new TaskLevel(task, weight, priorityLevel);
  m_taskMotions.push_back(tl);
  addTask(tl, weight, priorityLevel);

  return true;
}


bool InverseDynamicsFormulationAccForce::addForceTask(TaskContactForce & task,
                                                      double weight,
                                                      unsigned int priorityLevel,
                                                      double transition_duration)
{
  assert(weight>=0.0);
  assert(transition_duration>=0.0);
  TaskLevel *tl = new TaskLevel(task, weight, priorityLevel);
  m_taskContactForces.push_back(tl);
  addTask(tl, weight, priorityLevel);
  return true;
}


bool InverseDynamicsFormulationAccForce::addTorqueTask(TaskActuation & task,
                                                       double weight,
                                                       unsigned int priorityLevel,
                                                       double transition_duration)
{
  assert(weight>=0.0);
  assert(transition_duration>=0.0);
  TaskLevel *tl = new TaskLevel(task, weight, priorityLevel);
  m_taskActuations.push_back(tl);

  if(priorityLevel > m_hqpData.size())
    m_hqpData.resize(priorityLevel);

  const ConstraintBase & c = tl->task.getConstraint();
  if(c.isEquality())
  {
    tl->constraint = new ConstraintEquality(c.name(), c.rows(), m_v+m_k);
    if(priorityLevel==0)
      m_eq += c.rows();
  }
  else  // an actuator bound becomes an inequality because actuator forces are not in the problem variables
  {
    tl->constraint = new ConstraintInequality(c.name(), c.rows(), m_v+m_k);
    if(priorityLevel==0)
      m_in += c.rows();
  }

  m_hqpData[priorityLevel].push_back(make_pair<double, ConstraintBase*>(weight, tl->constraint));

  return true;
}


bool InverseDynamicsFormulationAccForce::addRigidContact(ContactBase & contact)
{
  ContactLevel *cl = new ContactLevel(contact);
  cl->index = m_k;
  m_k += contact.n_force();
  m_contacts.push_back(cl);
  resizeHqpData();

  const ConstraintBase & motionConstr = contact.getMotionConstraint();
  cl->motionConstraint = new ConstraintEquality(contact.name(), motionConstr.rows(), m_v+m_k);
  m_hqpData[0].push_back(make_pair<double, ConstraintBase*>(1.0, cl->motionConstraint));

  const ConstraintInequality & forceConstr = contact.getForceConstraint();
  cl->forceConstraint = new ConstraintInequality(contact.name(), forceConstr.rows(), m_v+m_k);
  m_hqpData[0].push_back(make_pair<double, ConstraintBase*>(1.0, cl->forceConstraint));

  const ConstraintEquality & forceRegConstr = contact.getForceRegularizationTask();
  cl->forceRegTask = new ConstraintEquality(contact.name(), forceRegConstr.rows(), m_v+m_k);
  m_hqpData[1].push_back(make_pair<double, ConstraintBase*>(
                           contact.getForceRegularizationWeight(), cl->forceRegTask));

  m_eq += motionConstr.rows();
  m_in += forceConstr.rows();

  return true;
}


const HqpData & InverseDynamicsFormulationAccForce::computeProblemData(double time,
                                                                       ConstRefVector q,
                                                                       ConstRefVector v)
{
  m_t = time;
  m_robot.computeAllTerms(m_data, q, v);

  for(vector<ContactLevel*>::iterator it=m_contacts.begin(); it!=m_contacts.end(); it++)
  {
    ContactLevel* cl = *it;
    unsigned int m = cl->contact.n_force();

    const ConstraintBase & mc = cl->contact.computeMotionTask(time, q, v, m_data);
    cl->motionConstraint->matrix().leftCols(m_v) = mc.matrix();
    cl->motionConstraint->vector() = mc.vector();

    const Matrix & T = cl->contact.getForceGeneratorMatrix(); // 6x12
    m_Jc.middleRows(cl->index, m).noalias() = T.transpose()*mc.matrix();

    const ConstraintInequality & fc = cl->contact.computeForceTask(time, q, v, m_data);
    cl->forceConstraint->matrix().middleCols(m_v+cl->index, m) = fc.matrix();
    cl->forceConstraint->lowerBound() = fc.lowerBound();
    cl->forceConstraint->upperBound() = fc.upperBound();

    const ConstraintEquality & fr = cl->contact.computeForceRegularizationTask(time, q, v, m_data);
    cl->forceRegTask->matrix().middleCols(m_v+cl->index, m) = fr.matrix();
    cl->forceRegTask->vector() = fr.vector();
  }

  const Matrix & M_a = m_robot.mass(m_data).bottomRows(m_v-6);
  const Vector & h_a = m_robot.nonLinearEffects(m_data).tail(m_v-6);
  const Matrix & J_a = m_Jc.rightCols(m_v-6);
  const Matrix & M_u = m_robot.mass(m_data).topRows<6>();
  const Vector & h_u = m_robot.nonLinearEffects(m_data).head<6>();
  const Matrix & J_u = m_Jc.leftCols<6>();

  m_baseDynamics.matrix().leftCols(m_v) = M_u;
  m_baseDynamics.matrix().rightCols(m_k) = -J_u.transpose();
  m_baseDynamics.vector() = -h_u;

  vector<TaskLevel*>::iterator it;
  for(it=m_taskMotions.begin(); it!=m_taskMotions.end(); it++)
  {
    const ConstraintBase & c = (*it)->task.compute(time, q, v, m_data);
    if(c.isEquality())
    {
      (*it)->constraint->matrix().leftCols(m_v) = c.matrix();
      (*it)->constraint->vector() = c.vector();
    }
    else if(c.isInequality())
    {
      (*it)->constraint->matrix().leftCols(m_v) = c.matrix();
      (*it)->constraint->lowerBound() = c.lowerBound();
      (*it)->constraint->upperBound() = c.upperBound();
    }
    else
    {
      (*it)->constraint->lowerBound().head(m_v) = c.lowerBound();
      (*it)->constraint->upperBound().head(m_v) = c.upperBound();
    }
  }

  for(it=m_taskContactForces.begin(); it!=m_taskContactForces.end(); it++)
  {
    assert(false);
  }

  for(it=m_taskActuations.begin(); it!=m_taskActuations.end(); it++)
  {
    const ConstraintBase & c = (*it)->task.compute(time, q, v, m_data);
    if(c.isEquality())
    {
      (*it)->constraint->matrix().leftCols(m_v).noalias() = c.matrix() * M_a;
      (*it)->constraint->matrix().rightCols(m_k).noalias() = - c.matrix() * J_a.transpose();
      (*it)->constraint->vector() = c.vector();
      (*it)->constraint->vector().noalias() -= c.matrix() * h_a;
    }
    else if(c.isInequality())
    {
      (*it)->constraint->matrix().leftCols(m_v).noalias() = c.matrix() * M_a;
      (*it)->constraint->matrix().rightCols(m_k).noalias() = - c.matrix() * J_a.transpose();
      (*it)->constraint->lowerBound() = c.lowerBound();
      (*it)->constraint->lowerBound().noalias() -= c.matrix() * h_a;
      (*it)->constraint->upperBound() = c.upperBound();
      (*it)->constraint->upperBound().noalias() -= c.matrix() * h_a;
    }
    else
    {
      // NB: An actuator bound becomes an inequality
      (*it)->constraint->matrix().leftCols(m_v) = M_a;
      (*it)->constraint->matrix().rightCols(m_k) = - J_a.transpose();
      (*it)->constraint->lowerBound() = c.lowerBound() - h_a;
      (*it)->constraint->upperBound() = c.upperBound() - h_a;
    }
  }

  return m_hqpData;
}

const Vector & InverseDynamicsFormulationAccForce::computeActuatorForces(const HqpOutput & sol)
{
  const Matrix & M_a = m_robot.mass(m_data).bottomRows(m_v-6);
  const Vector & h_a = m_robot.nonLinearEffects(m_data).tail(m_v-6);
  const Matrix & J_a = m_Jc.rightCols(m_v-6);
  m_dv = sol.x.head(m_v);
  m_f = sol.x.tail(m_k);
  m_tau = h_a;
  m_tau.noalias() += M_a*m_dv;
  m_tau.noalias() -= J_a.transpose()*m_f;
  return m_tau;
}


bool InverseDynamicsFormulationAccForce::removeTask(const std::string & taskName,
                                                    double transition_duration)
{
  bool taskFound = removeFromHqpData(taskName);
  assert(taskFound);

  vector<TaskLevel*>::iterator it;
  for(it=m_taskMotions.begin(); it!=m_taskMotions.end(); it++)
  {
    if((*it)->task.name()==taskName)
    {
      if((*it)->priority==0)
      {
        if((*it)->constraint->isEquality())
          m_eq -= (*it)->constraint->rows();
        else if((*it)->constraint->isInequality())
          m_in -= (*it)->constraint->rows();
      }
      m_taskMotions.erase(it);
      return true;
    }
  }
  for(it=m_taskContactForces.begin(); it!=m_taskContactForces.end(); it++)
  {
    if((*it)->task.name()==taskName)
    {
      m_taskContactForces.erase(it);
      return true;
    }
  }
  for(it=m_taskActuations.begin(); it!=m_taskActuations.end(); it++)
  {
    if((*it)->task.name()==taskName)
    {
      if((*it)->priority==0)
      {
        if((*it)->constraint->isEquality())
          m_eq -= (*it)->constraint->rows();
        else
          m_in -= (*it)->constraint->rows();
      }
      m_taskActuations.erase(it);
      return true;
    }
  }
  return false;
}


bool InverseDynamicsFormulationAccForce::removeRigidContact(const std::string & contactName,
                                                            double transition_duration)
{
  bool first_constraint_found = removeFromHqpData(contactName);
  assert(first_constraint_found);

  bool second_constraint_found = removeFromHqpData(contactName);
  assert(second_constraint_found);

  bool contact_found = false;
  for(vector<ContactLevel*>::iterator it=m_contacts.begin(); it!=m_contacts.end(); it++)
  {
    if((*it)->contact.name()==contactName)
    {
      m_k -= (*it)->contact.n_force();
      m_eq -= (*it)->motionConstraint->rows();
      m_in -= (*it)->forceConstraint->rows();
      m_contacts.erase(it);
      resizeHqpData();
      contact_found = true;
      break;
    }
  }

  int k=0;
  for(vector<ContactLevel*>::iterator it=m_contacts.begin(); it!=m_contacts.end(); it++)
  {
    ContactLevel * cl = *it;
    cl->index = k;
    k += cl->contact.n_force();
  }
  return contact_found;
}

bool InverseDynamicsFormulationAccForce::removeFromHqpData(const std::string & name)
{
  bool found = false;
  for(HqpData::iterator it=m_hqpData.begin(); !found && it!=m_hqpData.end(); it++)
  {
    for(ConstraintLevel::iterator itt=it->begin(); !found && itt!=it->end(); itt++)
    {
      if(itt->second->name()==name)
      {
        it->erase(itt);
        found = true;
      }
    }
  }
  return found;
}
