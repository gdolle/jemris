/*
 * Mpi2Evolution.cpp
 *
 *  Created on: Jan 13, 2010
 *      Author: dpflug
 */


/*
 *  JEMRIS Copyright (C) 
 *                        2006-2013  Tony Stöcker
 *                        2007-2013  Kaveh Vahedipour
 *                        2009-2013  Daniel Pflugfelder
 *                                  
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Mpi2Evolution.h"
#include "World.h"
#include <cmath>
#include "config.h"

// init variables:
#ifdef HAVE_MPI_THREADS
vector<MPI_File> 		Mpi2Evolution::m_files;
#endif
vector<bool>			Mpi2Evolution::m_first_write;


/*****************************************************************************/
Mpi2Evolution::Mpi2Evolution() {
}

/*****************************************************************************/
void SetSaveFunction() {
	World* pW = World::instance();
	pW->saveEvolFunPtr = &Mpi2Evolution::saveEvolution;
}

/*****************************************************************************/
void Mpi2Evolution::OpenFiles(int is_restart){
	World* pW= World::instance();
	if (pW->saveEvolStepSize == 0) return;


#ifndef HAVE_MPI_THREADS
	if (pW->m_myRank == 0)
		cout << "\n!!! Warning: !!!\nSaving spin evolution in parallel JEMRIS mode requires MPI2.0 (parallel file I/O). no evol files will be written.\nUse MPI2.0 or sequential jemris.\n" << endl;
#else
	long M     = pW->TotalADCNumber / pW->saveEvolStepSize ;
	string fname;
	int SpinNo = pW->TotalSpinNumber;

	MPI_Bcast(&is_restart,1,MPI_INT,0,MPI_COMM_WORLD);

	MPI_Bcast(&SpinNo,1,MPI_INT,0,MPI_COMM_WORLD);
	MPI_Offset filesize;
	filesize = (SpinNo * 7 +2)* sizeof(double);

	for (int i=0; i<M; i++) {
        stringstream sF;
        sF << pW->saveEvolFileName << "_" << setw(3) << setfill('0') << i+1 << ".bin";
        if (is_restart != 1){
         // delete existing old file; (c) by Rolf Rabenseifer...
  /*       	MPI::File fh=MPI::File::Open(MPI::COMM_WORLD,(sF.str()).c_str(),MPI::MODE_DELETE_ON_CLOSE | MPI::MODE_CREATE | MPI::MODE_WRONLY, MPI::INFO_NULL  );
         	fh.Close();*/
 	// above lines lead to trouble on our cluster. try different approach:
 		if (pW->m_myRank == 0) {
 			ofstream myfile;
 			myfile.open ((sF.str()).c_str(),ios::out | ios::binary| ios::trunc);
 			myfile.close();
 		}
 		MPI_Barrier(MPI_COMM_WORLD);
        }
	MPI_File mpifh;
	MPI_File_open (MPI_COMM_WORLD, sF.str()).c_str(),MPI_MODE_WRONLY | MPI_MODE_CREATE, MPI_INFO_NULL, &fh)
        m_files.push_back(mpifh);

		m_files[i].Preallocate(filesize);
		if (pW->m_myRank == 1) {
			double dummy = (double) SpinNo;
			m_files[i].Write_at(0,&dummy,1,MPI_DOUBLE);
			m_first_write.push_back(true);
		}
	}

#endif

}


/*****************************************************************************/
void Mpi2Evolution::CloseFiles(){
#ifdef HAVE_MPI_THREADS
	for (unsigned int i=0; i<m_files.size(); i++) {
		m_files[i].Close();
	}
#endif
}
/*****************************************************************************/
void Mpi2Evolution::saveEvolution(long index, bool close_files) {
#ifdef HAVE_MPI_THREADS

	   World* pW = World::instance();
	    if ( (pW->saveEvolFileName).empty() || pW->saveEvolStepSize==0 )
	        return;

	    int n = index / pW->saveEvolStepSize - 1;

	    //write timepoint at first call
	    if (pW->m_myRank==1 ) {
	    	if (m_first_write[n]) {
	    		m_first_write[n]=false;
				m_files[n].Write_at(1*sizeof(double),&(pW->time),1,MPI_DOUBLE);
	    	}
	    }

	    //write current magnetisation state of this spin
	    double Mx = pW->solution[AMPL]* cos (pW->solution[PHASE]) ;
	    double My = pW->solution[AMPL]* sin (pW->solution[PHASE]) ;
	    double tmp[7];
	    tmp[0]=pW->Values[ID];
	    tmp[1]=pW->Values[XC];
	    tmp[2]=pW->Values[YC];
	    tmp[3]=pW->Values[ZC];
	    tmp[4]=Mx;
	    tmp[5]=My;
	    tmp[6]=pW->solution[ZC];

	    MPI_Offset offset = (2+7*pW->Values[ID])*sizeof(double);
	    m_files[n].Write_at(offset,tmp,7,MPI_DOUBLE);


	    return;
#endif

}
