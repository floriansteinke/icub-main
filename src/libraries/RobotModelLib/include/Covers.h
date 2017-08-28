/*
* Copyright (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
* Author: Alessandro Scalzo
* email:  alessandro.scalzo@iit.it
* Permission is granted to copy, distribute, and/or modify this program
* under the terms of the GNU General Public License, version 2 or any
* later version published by the Free Software Foundation.
*
* A copy of the license can be found at
* http://www.robotcub.org/icub/license/gpl.txt
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
* Public License for more details
*/

#ifndef __ROBOT_COVERS_H__
#define __ROBOT_COVERS_H__

#include <string>
#include <vector>
#include "Geometry.h"
#include "Joints.h"

namespace icub
{
	namespace robot_model
	{
		class Sphere
		{
		protected:
			Vec3 Clocal;
			Transform &T;

		public:
			Vec3 Cworld;
			double radius;
			std::string name;

			//Sphere(){}

			Sphere(double x, double y, double z, double r, const char *s, Transform& Tlink) : Clocal(x, y, z), radius(r), name(s), T(Tlink)
			{
			}

			~Sphere(){}

			void pose()
			{
				Cworld = T*Clocal;
			}
		};

		inline double distance(Sphere &Sa, Sphere &Sb, Vec3 &A, Vec3 &B, Vec3 &U)
		{
			A = Sa.Cworld;
			B = Sb.Cworld;

			U = A - B;

			double r = U.normalize() - (Sa.radius + Sb.radius);

			A -= Sa.radius*U;
			B += Sb.radius*U;

			return r;
		}

		class Cover
		{
		public:
			enum { MAXSPHERES = 32 };
			enum { FLOATING = -1 };

			Sphere *sphere[MAXSPHERES];
			int nspheres;

			Cover(Component *p) : part(p)
			{
				for (int i = 0; i < MAXSPHERES; ++i) sphere[i] = NULL;
				nspheres = 0; 
			}

			~Cover()
			{
				for (int i = 0; i < MAXSPHERES; ++i) if (sphere[i]) delete sphere[i];
			}

			Sphere* addSphere(double x, double y, double z, double r, const char *name, Transform &T)
			{
				sphere[nspheres] = new Sphere(x, y, z, r, name, T);

				return sphere[nspheres++];
			}

			void pose()
			{
				for (int s = 0; s < nspheres; ++s) sphere[s]->pose();
			}

			void getSphere(int s, double &x, double& y, double& z, double &r, std::string &name)
			{
				x = sphere[s]->Cworld.x;
				y = sphere[s]->Cworld.y;
				z = sphere[s]->Cworld.z;
				r = sphere[s]->radius;
				name = sphere[s]->name;
			}

			int partID;
			Component *part;
		};

		inline double repulsion(Cover *Ca, Cover *Cb, Vec3 &Xa, Vec3 &Xb)
		{
			Xa.clear();
			Xb.clear();

			double Z = 0.0;
			double D = 1E10;

			for (int a = 0; a < Ca->nspheres; ++a)
			{
				for (int b = 0; b < Cb->nspheres; ++b)
				{
					Sphere* Sa = Ca->sphere[a];
					Vec3 A = Sa->Cworld;

					Sphere* Sb = Cb->sphere[b];
					Vec3 B = Sb->Cworld;

					Vec3 U = A - B;

					double d = U.normalize() - (Sa->radius + Sb->radius);

					if (d < D) D = d;

					if (d >= 0.0)
					{
						A -= Sa->radius*U;
						B += Sb->radius*U;
					}
					else
					{
						A += Sa->radius*U;
						B -= Sb->radius*U;
					}

					double z = exp(-d*fabs(d) / (0.04*0.04));

					Xa += z*A;
					Xb += z*B;

					Z += z;
				}
			}

			if (Z > 0.0)
			{
				Xa /= Z;
				Xb /= Z;
			}

			return D;
		}

		class Interference
		{
		public:
			enum { MAXDEPS = 32 };

			Interference(Cover *pA, Cover *pB, int j0, int j1)
			{
				init(pA, pB);

				for (int j = j0; j <= j1; ++j) addJointDep(j);
			}

			Interference(Cover *pA, Cover *pB)
			{
				init(pA, pB);
			}

			~Interference(){}

			void addJointDep(int j)
			{
				jdep.push_back(j);
			}

			int getDep(int d)
			{
				return jdep[d];
			}

			Cover *coverA;
			Cover *coverB;

			std::vector<int> jdep;

		protected:
			void init(Cover *pA, Cover *pB)
			{
				coverA = pA;
				coverB = pB;
			}
		};

	}
}

#endif
