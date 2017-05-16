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

#ifndef __invdyn_contact_6d_hpp__
#define __invdyn_contact_6d_hpp__

#include <pininvdyn/contacts/contact-base.hpp>
#include <pininvdyn/tasks/task-se3-equality.hpp>

namespace pininvdyn
{
  namespace contacts
  {
    class Contact6d:
        public ContactBase
    {
    public:
      typedef pininvdyn::RobotWrapper RobotWrapper;
      typedef pininvdyn::math::ConstRefMatrix ConstRefMatrix;
      typedef pininvdyn::math::ConstRefVector ConstRefVector;
      typedef pininvdyn::math::Matrix3x Matrix3x;
      typedef pininvdyn::math::Vector3 Vector3;
      typedef pininvdyn::math::Vector Vector;
      typedef pininvdyn::tasks::TaskMotion TaskMotion;
      typedef pininvdyn::tasks::TaskSE3Equality TaskSE3Equality;
      typedef pininvdyn::math::ConstraintInequality ConstraintInequality;
      typedef pininvdyn::math::ConstraintEquality ConstraintEquality;
      typedef se3::SE3 SE3;

      Contact6d(const std::string & name,
                RobotWrapper & robot,
                const std::string & frameName,
                ConstRefMatrix contactPoints,
                ConstRefVector contactNormal,
                const double frictionCoefficient,
                const double minNormalForce,
                const double maxNormalForce,
                const double regularizationTaskWeight);

      /// Return the number of motion constraints
      virtual unsigned int n_motion() const;

      /// Return the number of force variables
      virtual unsigned int n_force() const;

      virtual const ConstraintBase & computeMotionTask(const double t,
                                                       ConstRefVector q,
                                                       ConstRefVector v,
                                                       Data & data);

      virtual const ConstraintInequality & computeForceTask(const double t,
                                                            ConstRefVector q,
                                                            ConstRefVector v,
                                                            Data & data);

      virtual const Matrix & getForceGeneratorMatrix();

      virtual const ConstraintEquality & computeForceRegularizationTask(const double t,
                                                                        ConstRefVector q,
                                                                        ConstRefVector v,
                                                                        Data & data);

      const TaskMotion & getMotionTask() const;
      const ConstraintBase & getMotionConstraint() const;
      const ConstraintInequality & getForceConstraint() const;
      const ConstraintEquality & getForceRegularizationTask() const;
      double getForceRegularizationWeight() const;

      double getNormalForce(ConstRefVector f) const;
      double getMinNormalForce() const;
      double getMaxNormalForce() const;

      const Vector & Kp() const;
      const Vector & Kd() const;
      void Kp(ConstRefVector Kp);
      void Kd(ConstRefVector Kp);

      bool setContactPoints(ConstRefMatrix contactPoints);
      bool setContactNormal(ConstRefVector contactNormal);

      bool setFrictionCoefficient(const double frictionCoefficient);
      bool setMinNormalForce(const double minNormalForce);
      bool setMaxNormalForce(const double maxNormalForce);
      bool setRegularizationTaskWeight(const double w);
      void setReference(const SE3 & ref);

    protected:

      void updateForceInequalityConstraints();
      void updateForceRegularizationTask();
      void updateForceGeneratorMatrix();

      TaskSE3Equality m_motionTask;
      ConstraintInequality m_forceInequality;
      ConstraintEquality m_forceRegTask;
      Matrix3x m_contactPoints;
      Vector3 m_contactNormal;
      double m_mu;
      double m_fMin;
      double m_fMax;
      double m_regularizationTaskWeight;
      Matrix m_forceGenMat;
    };
  }
}

#endif // ifndef __invdyn_contact_6d_hpp__
