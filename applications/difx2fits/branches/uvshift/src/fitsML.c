/***************************************************************************
 *   Copyright (C) 2008, 2009 by Walter Brisken                            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
/*===========================================================================
 * SVN properties (DO NOT CHANGE)
 *
 * $Id: fitsML.c 1442 2009-09-02 19:41:48Z WalterBrisken $
 * $HeadURL: https://svn.atnf.csiro.au/difx/master_tags/DiFX-1.5.2/applications/difx2fits/src/fitsML.c $
 * $LastChangedRevision: 1442 $
 * $Author: WalterBrisken $
 * $LastChangedDate: 2009-09-02 21:41:48 +0200 (Wed, 02 Sep 2009) $
 *
 *==========================================================================*/

#include <stdlib.h>
#include <sys/types.h>
#include <strings.h>
#include <sys/time.h>
#include "config.h"
#include "difx2fits.h"

#define array_N_POLY 6

double current_mjd()
{
	struct timeval t;
	const double MJD_UNIX0=40587.0; /* MJD at beginning of unix time */

	gettimeofday(&t, 0);
	
	return MJD_UNIX0 + (t.tv_sec + t.tv_usec*1.0e-6)/86400.0;
}
		
/* given four samples, calculate polynomial referenced to time of second one */
void calcPolynomial(double gpoly[array_N_POLY], 
	double a, double b, double c, double d,
	double deltaT)
{
	int i;

	for(i = 0; i < array_N_POLY; i++)
	{
		gpoly[i] = 0.0;
	}

	/* FIXME -- for now just assume linear! */
	a = d = 0.0; /* line to prevent compiler warning */

	/* don't convert to convert to seconds from nanosec */
	gpoly[0] = b * 1.0e-6;
	gpoly[1] = (c-b) * 1.0e-6 / deltaT;
}

const DifxInput *DifxInput2FitsML(const DifxInput *D,
	struct fits_keywords *p_fits_keys, struct fitsPrivate *out,
	struct CommandLineOptions *opts)
{
	char bandFormDouble[4];
	char bandFormFloat[4];

	struct fitsBinTableColumn columns[] =
	{
		{"TIME", "1D", "time of model start", "DAYS"},
		{"TIME_INTERVAL", "1E", "model interval", "DAYS"},
		{"SOURCE_ID", "1J", "source id from sources tbl", 0},
		{"ANTENNA_NO", "1J", "antenna number from antennas tbl", 0},
		{"ARRAY", "1J", "array id number", 0},
		{"FREQID", "1J", "frequency id number from frequency tbl", 0},
		{"I.FAR.ROT", "1E", "ionospheric faraday rotation", 
			"RAD/METER**2"},
		{"FREQ.VAR", bandFormFloat, "time variable freq. offset", "HZ"},
		{"PDELAY_1", bandFormDouble, "total phase delay at ref time", 
			"TURNS"},
		{"GDELAY_1", "6D", "total group delay at ref time", "SECONDS"},
		{"PRATE_1", bandFormDouble, "phase delay rate", "HZ"},
		{"GRATE_1", "6D", "group delay rate", "SEC/SEC"},
		{"DISP_1", "1E", "dispersive delay for polar.1", "SECONDS"},
		{"DDISP_1", "1E", "dispersive delay rate for polar. 1", 
			"SEC/SEC"},
		{"PDELAY_2", bandFormDouble, "total phase delay at ref time", 
			"TURNS"},
		{"GDELAY_2", "6D", "total group delay at ref time", "SECONDS"},
		{"PRATE_2", bandFormDouble, "phase delay rate", "HZ"},
		{"GRATE_2", "6D", "group delay rate", "SEC/SEC"},
		{"DISP_2", "1E", "dispersive delay for polar.2", "SECONDS"},
		{"DDISP_2", "1E", "dispersive delay rate for polar. 2", 
			"SEC/SEC"}
	};

	char *fitsbuf;		/* compose FITS file rows here */
	int nBand;
	int nColumn;
	int nRowBytes;
	char str[80];
	int a, i, j, k, p, s, antId;
	double ppoly[array_MAX_BANDS][array_N_POLY];
	double gpoly[array_N_POLY];
	double prate[array_MAX_BANDS][array_N_POLY];
	double grate[array_N_POLY];
	float freqVar[array_MAX_BANDS];
	float faraday;
	int configId, jobId, dsId;
	double time;
	float timeInt;
	int nPol, np;
	char *p_fitsbuf;
	const DifxScan *scan;
	const DifxJob *job;
	const DifxConfig *config;
	const DifxModel *M;
	const DifxPolyModel *P;
	float dispDelay;
	float dispDelayRate;
	double modelInc;
	double start;
	double deltat;
	double freq;
	int *skip;
	int skipped=0;
	int printed=0;
	/* 1-based indices for FITS */
	int32_t sourceId1, freqId1, arrayId1, antId1;
	double clockRate;

	if(D == 0)
	{
		return 0;
	}

	if(!opts->writemodel)
	{
		return 0;
	}

	nBand = p_fits_keys->no_band;
	nPol = D->nPol;
  
	/* set FITS header to reflect number of bands in observation */
	sprintf(bandFormDouble, "%dD", array_N_POLY * nBand);  
	sprintf(bandFormFloat, "%dE", nBand);  
  
	/* determine size of records for FITS file */
	if(nPol == 2)
	{
		nColumn = NELEMENTS(columns);
	}
	else  /* don't populate last 6 columns if not full polar */
	{
		nColumn = NELEMENTS(columns) - 6;
	}
	nRowBytes = FitsBinTableSize(columns, nColumn);

	/* write "binary file extension description" to output file */
	fitsWriteBinTable(out, nColumn, columns, nRowBytes,
		     "INTERFEROMETER_MODEL");
  
	/* calloc space for storing table in FITS order */
	fitsbuf = (char *)calloc(nRowBytes, 1);
	if(fitsbuf == 0)
	{
		return 0;
	}
  
	/* write standard FITS header keywords and values to output file */
	arrayWriteKeys(p_fits_keys, out);
  
	/* and write some more keywords and values to output file */
	fitsWriteInteger(out, "TABREV", 2, "");
	fitsWriteInteger(out, "NO_POL", p_fits_keys->no_pol, "");
	fitsWriteFloat(out, "GSTIA0", 0.0, "");
	fitsWriteFloat(out, "DEGPDY", 0.0, "");
	mjd2fits(p_fits_keys->ref_date, str);
	fitsWriteString(out, "RDATE", str, "");
	mjd2fits((int)current_mjd(), str);
	fitsWriteString(out, "CDATE", str, ""); 
	fitsWriteInteger(out, "NPOLY", array_N_POLY, "");
	fitsWriteFloat(out, "REVISION", 1.00, "");

	fitsWriteEnd(out);

	arrayId1 = 1;

	/* some values that are always zero */
	dispDelay = 0.0;
	dispDelayRate = 0.0;
	faraday = 0.0;
	for(i = 0; i < nBand; i++)
	{
		freqVar[i] = 0.0;
	}

	skip = (int *)calloc(D->nAntenna, sizeof(int));

	for(s = 0; s < D->nScan; s++)
	{
	   scan = D->scan + s;
	   jobId = scan->jobId;
	   job = D->job + jobId;
	   configId = scan->configId;
	   if(configId < 0)
	   {
	   	continue;
	   }

	   config = D->config + configId;
	   freqId1 = config->freqId + 1;
	   sourceId1 = D->source[scan->sourceId].fitsSourceId + 1;

	   modelInc = job->modelInc;
	   start = D->scan[s].mjdStart - (int)(D->mjdStart);
	   
	   if(scan->im)
	   {
	   	np = scan->nPoly;
		timeInt = job->polyInterval / 86400.0;
	   }
	   else
	   {
	   	np = scan->nPoint;
	   	timeInt = modelInc / 86400.0;
	   }

	   for(p = 0; p < np; p++)
	   {
	      for(a = 0; a < config->nAntenna; a++)
	      {
		dsId = config->ant2dsId[a];
		if(dsId < 0 || dsId >= D->nDatastream)
		{
			continue;
		}
		/* convert to D->antenna[] index ... */
		antId = D->datastream[dsId].antennaId;

		if(antId < 0 || antId >= scan->nAntenna)
		{
		  continue;
		}

		/* ... and to FITS antennaId */
		antId1 = antId + 1;

	        if(scan->im)  /* use polynomial model (preferred) */
		{
		  if(scan->im[antId] == 0)
		  {
		    if(skip[antId] == 0)
		    {
		      printf("\n    Polynomial model error : skipping antId %d = %s",
		        antId, D->antenna[antId].name);
		      skip[antId]++;
		      printed++;
		      skipped++;
		    }
		    continue;
		  }
	       
		  P = scan->im[antId] + p;

	          time = P->mjd - (int)(D->mjdStart) + P->sec/86400.0;
		  deltat = (P->mjd - D->mjdStart)*86400.0 + P->sec;

	          for(k = 0; k < array_N_POLY; k++)
		  {
		    gpoly[k] = -P->delay[k]*1.0e-6;
		  }
		}
		else if(scan->model)	   /* use tabulated model */
		{
		  if(scan->model[antId] == 0)
		  {
		    if(skip[antId] == 0)
		    {
		      printf("\n    Tabulated model error : skipping antId %d = %s",
		        antId, D->antenna[antId].name);
		      skip[antId]++;
		      printed++;
		      skipped++;
		    }
		    continue;
		  }
	       	
		  M = scan->model[antId] + p;

		  time = start + timeInt*p;
		  deltat = modelInc*p;

		  calcPolynomial(gpoly,
			-M[-1].t, -M[0].t, -M[1].t, -M[2].t, modelInc);
		}
		else
		{
		  if(skip[antId] == 0)
		  {
		    printf("\n    Model error : no model information for antId %d = %s",
		      antId, D->antenna[antId].name);
		    skip[antId]++;
		    printed++;
		    skipped++;
		  }
		  deltat = modelInc*p;
		}

		clockRate = D->antenna[antId].rate*1.0e-6;

		gpoly[0] -= (D->antenna[antId].delay*1.0e-6 + clockRate*deltat);
		gpoly[1] -= clockRate;		

		/* All others are derived from gpoly */
		for(k = 1; k < array_N_POLY; k++)
		{
			grate[k-1] = k*gpoly[k];
		}
		grate[array_N_POLY-1] = 0.0;
		
		for(j = 0; j < nBand; j++)
		{
			freq = config->IF[j].freq*1.0e6;
			for(k = 0; k < array_N_POLY; k++)
			{
				ppoly[j][k] = gpoly[k]*freq;
				prate[j][k] = grate[k]*freq;
			}
		}

		p_fitsbuf = fitsbuf;

		FITS_WRITE_ITEM (time, p_fitsbuf);
		FITS_WRITE_ITEM (timeInt, p_fitsbuf);
		FITS_WRITE_ITEM (sourceId1, p_fitsbuf);
		FITS_WRITE_ITEM (antId1, p_fitsbuf);
		FITS_WRITE_ITEM (arrayId1, p_fitsbuf);
		FITS_WRITE_ITEM (freqId1, p_fitsbuf);
		FITS_WRITE_ITEM (faraday, p_fitsbuf);
		FITS_WRITE_ARRAY(freqVar, p_fitsbuf, nBand);

		for(i = 0; i < nPol; i++)
		{
			FITS_WRITE_ARRAY(ppoly[0], p_fitsbuf,
				nBand*array_N_POLY);
			FITS_WRITE_ARRAY(gpoly, p_fitsbuf, array_N_POLY);
			FITS_WRITE_ARRAY(prate[0], p_fitsbuf,
				nBand*array_N_POLY);
			FITS_WRITE_ARRAY(grate, p_fitsbuf, array_N_POLY);
			FITS_WRITE_ITEM (dispDelay, p_fitsbuf);
			FITS_WRITE_ITEM (dispDelayRate, p_fitsbuf);
		} /* Polar loop */

		testFitsBufBytes(p_fitsbuf - fitsbuf, nRowBytes, "ML");
      
#ifndef WORDS_BIGENDIAN
		FitsBinRowByteSwap(columns, nColumn, fitsbuf);
#endif
		fitsWriteBinRow(out, fitsbuf);
		
	      } /* end antenna loop */
	   } /* end model loop */
	} /* Scan loop */

  	if(printed)
	{
		printf("\n                            ");
	}
  
	/* release buffer space */
	free(fitsbuf);
	free(skip);

	return D;
}
