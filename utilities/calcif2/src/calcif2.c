/***************************************************************************
 *   Copyright (C) 2008-2015 by Walter Brisken & Adam Deller               *
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
//===========================================================================
// SVN properties (DO NOT CHANGE)
//
// $Id$
// $HeadURL: $
// $LastChangedRevision$
// $Author$
// $LastChangedDate$
//
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <fcntl.h>
#include <rpc/rpc.h>
#include <glob.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "difxcalc.h"
#include "DiFX_Delay_Server.h"

#define MAX_FILES	2048

const char program[] = "calcif2";
const char author[]  = "Walter Brisken <wbrisken@nrao.edu>";
const char version[] = VERSION;
const char verdate[] = "20150401";

typedef struct
{
	int verbose;
	int force;
	int doall;
    enum AberCorr aberCorr; /* level of correction for aberration */
	enum PerformDirectionDerivativeType perform_uvw_deriv;
	enum PerformDirectionDerivativeType perform_lmn_deriv;
	enum PerformDirectionDerivativeType perform_xyz_deriv;
	double delta_lmn; /* (rad) step size for calculating d\tau/dl, d\tau/dm,
						 and d\tau/dn for the LMN polynomial model
						 and (u,v) from the delay derivatives for the UVW
						 polynomial model
					  */
	double delta_xyz; /* step size for calculating d\tau/dx, d\tau/dy, and
						 d\tau/dz for the Cartesian (x,y,z) coordinate
						 system of the source.  If positive, this variable
						 has units of meters (\Delta x = xyz_delta).
						 If negative, then the variable is a fractional value
						 to indicate the step size as a function of the
						 current radius of the source.  (So with
						 r = (x^2 + y^2 + z^2)^{1/2},
						 \Delta x = xyz_delta \times r.)
					  */
	char delayServerHost[DIFXIO_HOSTNAME_LENGTH];
	enum DelayServerType delayServerType;
	unsigned long delayVersion;
	unsigned long delayHandler;
	int nFile;
	int polyOrder;
	int polyInterval;	/* (sec) */
	int polyOversamp;
	int interpol;
	int allowNegDelay;
	int useExtraExternalDelay; /* Flag
								  0: Do not use some extra delay software
								  1: Use the calc_Sekido software
							   */
	int warnSpacecraftPointingSource;
	char *files[MAX_FILES];
	int overrideVersion;
} CommandLineOptions;

static void usage()
{
	enum DelayServerType ds;
	fprintf(stderr, "%s ver. %s  %s  %s\n\n", program, version, author, verdate);
	fprintf(stderr, "A program to calculate a model for DiFX using a calc server.\n\n");
	fprintf(stderr, "Usage : %s [options] { <calc file> | -a }\n\n", program);
	fprintf(stderr, "<calc file> should be a '.calc' file as generated by vex2difx.\n\n");
	fprintf(stderr, "options can include:\n");
	fprintf(stderr, "  --help\n");
	fprintf(stderr, "  -h                      Print this help and quit\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --verbose\n");
	fprintf(stderr, "  -v                      Be more verbose in operation\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --quiet\n");
	fprintf(stderr, "  -q                      Be less verbose in operation\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --delta <delta>\n");
	fprintf(stderr, "  --delta_lmn <delta>\n");
	fprintf(stderr, "  -d <delta>              set delta (in radians) for UWV and LMN calculation\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --delta_xyz <delta>      set delta (in radians) for XYZ calculation\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --force\n");
	fprintf(stderr, "  -f                      Force recalc\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --noaber\n");
	fprintf(stderr, "  -n                      Don't do aberration, etc, corrections\n");
	fprintf(stderr, "                          (also turns off numerical (u,v) calculations)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --noatmos\n");
	fprintf(stderr, "  -A                      Don't include atmosphere in UVW calculations\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --all\n");
	fprintf(stderr, "  -a                      Do all calc files found\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --allow-neg-delay\n");
	fprintf(stderr, "  -z                      Don't zero negative delays\n");
	fprintf(stderr, "  --not-allow-neg-delay   Zero negative delays\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --allow-sc-pointing     Allow spacecraft pointing sources\n");
	fprintf(stderr, "  --not-allow-sc-pointing Do not allow spacecraft pointing sources\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --order <n>\n");
	fprintf(stderr, "  -o      <n>             Use <n>th order polynomial [5]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --oversamp <m>\n");
	fprintf(stderr, "  -O         <m>          Oversample polynomial by factor <m> [1]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --interval <int>\n");
	fprintf(stderr, "  -i         <int>        New delay poly every <int> sec. [120]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --fit\n");
	fprintf(stderr, "  -F                      Fit oversampled polynomials\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --sekido                Use Sekido near-field model\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --override-version      Ignore difx versions\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --perform_delta         Use delta_lmn (in radians) for UWV calculation\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --perform_delta2        Use delta_lmn (in radians) for UWV calculation (alt)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --perform_delta_lmn_1   Use delta_lmn (in radians) for LMN calculation\n"      "                          to 1st order\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --perform_delta_lmn_12  Use delta_lmn (in radians) for LMN calculation\n"      "                          to 1st order (alt)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --perform_delta_lmn_2   Use delta_lmn (in radians) for XYZ calculation\n"      "                          to 2nd order\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --perform_delta_lmn_22  Use delta_lmn (in radians) for XYZ calculation\n"      "                          to 2nd order (alt)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --perform_delta_xyz_1   Use delta_xyz (in radians) for LMN calculation\n"      "                          to 1st order\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --perform_delta_xyz_12  Use delta_xyz (in radians) for LMN calculation\n"      "                          to 1st order (alt)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --perform_delta_xyz_2   Use delta_xyz (in radians) for XYZ calculation\n"      "                          to 2nd order\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --perform_delta_xyz_22  Use delta_xyz (in radians) for XYZ calculation\n"      "                          to 2nd order (alt)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --not-perform_delta     Do not use delta_lmn (in radians) for UWV calculation\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --not-perform_delta_lmn Do not use delta_lmn (in radians) for LMN calculation\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --not-perform_delta_xyz Do not use delta_xyz (in radians) for LMN calculation\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --server <servername>\n");
	fprintf(stderr, "  -s       <servername>   Use <servername> as hostname for the delay server\n\n");
	fprintf(stderr, "      By default 'localhost' will be the delay server host.  An environment\n");
	fprintf(stderr, "      variable DIFX_DELAY_SERVER can be used to override that.  The command line\n");
	fprintf(stderr, "      overrides all.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --server-type <type>    Use <type> as the type of delay server to use\n\n");
	fprintf(stderr, "      Allowed values are\n");
	for(ds = 0; ds < NumDelayServerTypes; ++ds)
	{
		fprintf(stderr, "          %s\n", delayServerTypeNames[ds]);
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "  --server-version <ver>  Use <ver> as the version of the delay server handler\n");
	fprintf(stderr, "\n");
}

static void deleteCommandLineOptions(CommandLineOptions *opts)
{
	int i;

	if(!opts)
	{
		return;
	}

	for(i = 0; i < opts->nFile; ++i)
	{
		free(opts->files[i]);
	}

	free(opts);
}

static CommandLineOptions *newCommandLineOptions(int argc, char **argv)
{
	CommandLineOptions *opts;
	glob_t globbuf;
	int i, v;
	char *cs;
	int die = 0;

	opts = (CommandLineOptions *)calloc(1, sizeof(CommandLineOptions));
	opts->perform_uvw_deriv = PerformDirectionDerivativeDefault;
	opts->perform_lmn_deriv = PerformDirectionDerivativeDefault;
	opts->perform_xyz_deriv = PerformDirectionDerivativeDefault;
	opts->delta_lmn = 0.0;
	opts->delta_xyz = 0.0;
	opts->delayServerType = UnknownServer; /* default to value in the .calc file */
	opts->polyOrder = 0;    /* default to value in the .calc file */
	opts->polyOversamp = 1;
	opts->polyInterval = 0; /* default to value in the .calc file */
	opts->interpol = 0;	/* usual solve */
	opts->warnSpacecraftPointingSource = 1;
	opts->aberCorr = DIFXIO_DEFAULT_ABER_CORR_TYPE;

	for(i = 1; i < argc; ++i)
	{
		if(argv[i][0] == '-')
		{
			if(strcmp(argv[i], "-v") == 0 ||
			   strcmp(argv[i], "--verbose") == 0)
			{
				++opts->verbose;
			}
			else if(strcmp(argv[i], "-q") == 0 ||
					strcmp(argv[i], "--quiet") == 0)
			{
				--opts->verbose;
			}
			else if(strcmp(argv[i], "-f") == 0 ||
					strcmp(argv[i], "--force") == 0)
			{
				++opts->force;
			}
			else if(strcmp(argv[i], "-a") == 0 ||
			        strcmp(argv[i], "--all") == 0)
			{
				opts->doall = 1;
			}
			else if(strcmp(argv[i], "-z") == 0 ||
			        strcmp(argv[i], "--allow-neg-delay") == 0)
			{
				opts->allowNegDelay = 1;
			}
			else if(strcmp(argv[i], "--not-allow-neg-delay") == 0)
			{
				opts->allowNegDelay = 0;
			}
			else if(strcmp(argv[i], "--allow-sc-pointing") == 0)
			{
				opts->warnSpacecraftPointingSource = 0;
			}
			else if(strcmp(argv[i], "--not-allow-sc-pointing") == 0)
			{
				opts->warnSpacecraftPointingSource = 1;
			}
			else if(strcmp(argv[i], "-n") == 0 ||
					strcmp(argv[i], "--noaber") == 0)
			{
				opts->aberCorr = AberCorrUncorrected;
				opts->perform_uvw_deriv = PerformDirectionDerivativeNone;
			}
			else if(strcmp(argv[i], "-A") == 0 ||
					strcmp(argv[i], "--noatmos") == 0)
			{
				opts->aberCorr = AberCorrNoAtmos;
			}
			else if(strcmp(argv[i], "-F") == 0 ||
					strcmp(argv[i], "--fit") == 0)
			{
				opts->interpol = 1;
			}
			else if(strcmp(argv[i], "-h") == 0 ||
					strcmp(argv[i], "--help") == 0)
			{
				usage();
				deleteCommandLineOptions(opts);
				
				return 0;
			}
			else if(strcmp(argv[i], "--override-version") == 0)
			{
				opts->overrideVersion = 1;
			}
			else if(strcmp(argv[i], "--not-perform_delta") == 0)
			{
				opts->perform_uvw_deriv = PerformDirectionDerivativeNone;
			}
			else if(strcmp(argv[i], "--perform_delta") == 0)
			{
				opts->perform_uvw_deriv = PerformDirectionDerivativeFirstDerivative;
			}
			else if(strcmp(argv[i], "--perform_delta2") == 0)
			{
				opts->perform_uvw_deriv = PerformDirectionDerivativeFirstDerivative2;
			}
			else if(strcmp(argv[i], "--not-perform_delta_lmn") == 0)
			{
				opts->perform_lmn_deriv = PerformDirectionDerivativeNone;
			}
			else if(strcmp(argv[i], "--perform_delta_lmn_1") == 0)
			{
				opts->perform_lmn_deriv = PerformDirectionDerivativeFirstDerivative;
			}
			else if(strcmp(argv[i], "--perform_delta_lmn_12") == 0)
			{
				opts->perform_lmn_deriv = PerformDirectionDerivativeFirstDerivative2;
			}
			else if(strcmp(argv[i], "--perform_delta_lmn_2") == 0)
			{
				opts->perform_lmn_deriv = PerformDirectionDerivativeSecondDerivative;
			}
			else if(strcmp(argv[i], "--perform_delta_lmn_22") == 0)
			{
				opts->perform_lmn_deriv = PerformDirectionDerivativeSecondDerivative2;
			}
			else if(strcmp(argv[i], "--not-perform_delta_xyz") == 0)
			{
				opts->perform_xyz_deriv = PerformDirectionDerivativeNone;
			}
			else if(strcmp(argv[i], "--perform_delta_xyz_1") == 0)
			{
				opts->perform_xyz_deriv = PerformDirectionDerivativeFirstDerivative;
			}
			else if(strcmp(argv[i], "--perform_delta_xyz_12") == 0)
			{
				opts->perform_xyz_deriv = PerformDirectionDerivativeFirstDerivative2;
			}
			else if(strcmp(argv[i], "--perform_delta_xyz_2") == 0)
			{
				opts->perform_xyz_deriv = PerformDirectionDerivativeSecondDerivative;
			}
			else if(strcmp(argv[i], "--perform_delta_xyz_22") == 0)
			{
				opts->perform_xyz_deriv = PerformDirectionDerivativeSecondDerivative2;
			}
			else if(strcmp(argv[i], "--sekido") == 0)
			{
				opts->useExtraExternalDelay = 1;
			}
			else if(i+1 < argc)
			{
				if(strcmp(argv[i], "--server") == 0 ||
				   strcmp(argv[i], "-s") == 0)
				{
					++i;
					v = snprintf(opts->delayServerHost, DIFXIO_HOSTNAME_LENGTH, "%s", argv[i]);
					if(v >= DIFXIO_NAME_LENGTH)
					{
						fprintf(stderr, "Error: calcif2: delayServerHost name, %s, is too long (more than %d chars)\n",
								argv[i], DIFXIO_HOSTNAME_LENGTH-1);
						++die;
					}
				}
				else if(strcmp(argv[i], "--server-type") == 0)
				{
					++i;
					opts->delayServerType = stringToDelayServerType(argv[i]);
					if(opts->delayServerType == NumDelayServerTypes)
					{
						fprintf(stderr, "Error: calcif2: delayServerType type, %s, is not recognized\n", argv[i]);
						++die;
					}
				}
				else if(strcmp(argv[i], "--server-version") == 0)
				{
					++i;
					opts->delayVersion = strtoul(argv[i], NULL, 0);
				}
				else if(strcmp(argv[i], "--order") == 0 ||
						strcmp(argv[i], "-o") == 0)
				{
					++i;
					opts->polyOrder = atoi(argv[i]);
				}
				else if(strcmp(argv[i], "--oversamp") == 0 ||
						strcmp(argv[i], "-O") == 0)
				{
					++i;
					opts->polyOversamp = atoi(argv[i]);
				}
				else if(strcmp(argv[i], "--interval") == 0 ||
						strcmp(argv[i], "-i") == 0)
				{
					++i;
					opts->polyInterval = atoi(argv[i]);
				}
				else if(strcmp(argv[i], "--delta") == 0 ||
						strcmp(argv[i], "--delta_lmn") == 0 ||
						strcmp(argv[i], "-d") == 0
						)
				{
					++i;
					opts->delta_lmn = atof(argv[i]);
					if(opts->delta_lmn == 0.0)
					{
						opts->perform_uvw_deriv = PerformDirectionDerivativeNone;
						opts->perform_lmn_deriv = PerformDirectionDerivativeNone;
					}
				}
				else if(strcmp(argv[i], "--delta_xyz") == 0)
				{
					++i;
					opts->delta_xyz = atof(argv[i]);
					if(opts->delta_xyz == 0.0)
					{
						opts->perform_xyz_deriv = PerformDirectionDerivativeNone;
					}
				}
				else if(argv[i][0] == '-')
				{
					printf("Error: calcif2: Illegal option : %s\n", argv[i]);
					++die;
				}
			}
			else if(argv[i][0] == '-')
			{
				printf("Error: calcif2: Illegal option : %s\n", argv[i]);
				++die;
			}
		}
		else
		{
			opts->files[opts->nFile] = strdup(argv[i]);
			opts->nFile++;
			if(opts->nFile >= MAX_FILES)
			{
				fprintf(stderr, "Error: calcif2: Too many files (%d max)\n", MAX_FILES);
				++die;
			}
		}
	}

	if(opts->doall == 0 && opts->nFile == 0 && !die)
	{
		fprintf(stderr, "Error: calcif2: No input files!\n");
		++die;
	}

	if(((opts->polyOrder)) && (opts->polyOrder < 2 || opts->polyOrder > MAX_MODEL_ORDER))
	{
		fprintf(stderr, "Error: calcif2 Polynomial order must be in range [2, %d]\n", MAX_MODEL_ORDER);
		++die;
	}

	if(opts->polyOversamp < 1 || opts->polyOversamp > MAX_MODEL_OVERSAMP)
	{
		fprintf(stderr, "Error: calcif2 Polynomial oversample factor must be in range [1, %d]\n", MAX_MODEL_OVERSAMP);
		++die;
	}

	if(opts->interpol == 1 && opts->polyOversamp == 1)
	{
		opts->polyOversamp = 2;
		fprintf(stderr, "Note: oversampling increased to 2 because polynomial fitting is being used.\n");
	}

	if(((opts->polyInterval)) && (opts->polyInterval < 10 || opts->polyInterval > 600))
	{
		fprintf(stderr, "Error: calcif2: Interval must be in range [10, 600] sec\n");
		++die;
	}

	if(opts->nFile > 0 && opts->doall)
	{
		fprintf(stderr, "Error: calcif2: Option '--all' provided with files!\n");
		++die;
	}
	else if(opts->doall > 0)
	{
		glob("*.calc", 0, 0, &globbuf);
		opts->nFile = globbuf.gl_pathc;
		if(opts->nFile >= MAX_FILES)
		{
			fprintf(stderr, "Error: calcif2: Too many files (%d max)\n", MAX_FILES);
			++die;
		}
		else if(opts->nFile <= 0)
		{
			fprintf(stderr, "Error: calcif2: No .calc files found.  Hint: Did you run vex2difx yet???\n");
			++die;
		}
		for(i = 0; i < opts->nFile; ++i)
		{
			opts->files[i] = strdup(globbuf.gl_pathv[i]);
		}
		globfree(&globbuf);
	}

	if(opts->delayServerHost[0] == 0)
	{
		cs = getenv("DIFX_DELAY_SERVER");
		if(cs)
		{
			v = snprintf(opts->delayServerHost, DIFXIO_HOSTNAME_LENGTH, "%s", cs ? cs : "localhost");
			if(v >= DIFXIO_HOSTNAME_LENGTH)
			{
				fprintf(stderr, "Error: calcif2: env var DIFX_DELAY_SERVER is set to a name that is too long, %s (should be < %d chars)\n", cs ? cs : "localhost", DIFXIO_HOSTNAME_LENGTH);
				++die;
			}
		}
	}

	if(opts->delayVersion == 0)
	{
		opts->delayVersion = DIFX_DELAY_SERVER_VERS_1;
	}
	opts->delayHandler = DIFX_DELAY_SERVER_PROG;

	if(die)
	{
		if(die > 1)
		{
			fprintf(stderr, "calcif2 quitting. (%d errors)\n", die);
		}
		else
		{
			fprintf(stderr, "calcif2 quitting.\n");
		}
		fprintf(stderr, "Use -h option for calcif2 help.\n");
		deleteCommandLineOptions(opts);

		return 0;
	}

	return opts;
}

/* return 1 if f2 exists and is older than f1 */
static int skipFile(const char *f1, const char *f2)
{
	struct stat s1, s2;
	int r1, r2;

	r2 = stat(f2, &s2);
	if(r2 != 0)
	{
		return 0;
	}
	r1 = stat(f1, &s1);
	if(r1 != 0)
	{
		return 0;
	}

	if(s2.st_mtime > s1.st_mtime)
	{
		return 1;
	}

	return 0;
}

static void tweakDelays(DifxInput *D, const char *tweakFile, int verbose)
{
	const int MaxLineSize=100;
	FILE *in;
	char line[MaxLineSize];
	int s, a, i, j;
	double mjd, A, B, C;
	DifxPolyModel ***im, *model;
	int nModified = 0;
	int nModel = 0;
	int nLine;
	char *v;

	in = fopen(tweakFile, "r");
	if(!in)
	{
		/* The usual case. */

		return;
	}

	printf("Delay tweaking file %s found!\n", tweakFile);

	for(nLine = 0; ; ++nLine)
	{
		v = fgets(line, MaxLineSize-1, in);
		if(feof(in) || v == 0)
		{
			break;
		}
		if(sscanf(line, "%lf %lf %lf %lf", &mjd, &A, &B, &C) != 4)
		{
			continue;
		}

		for(s = 0; s < D->nScan; ++s)
		{
			im = D->scan[s].im;
			if(!im)
			{
				continue;
			}
			for(a = 0; a < D->nAntenna; ++a)
			{
				if(!im[a])
				{
					continue;
				}
				for(i = 0; i <= D->scan[s].nPhaseCentres; ++i)
				{
					if(!im[a][i])
					{
						continue;
					}
					for(j = 0; j < D->scan[s].nPoly; ++j)
					{
						model = im[a][i] + j;
						if(fabs(model->mjd + model->sec/SEC_DAY_DBL - mjd) < 0.5/SEC_DAY_DBL)	/* a match! */
						{
							++nModified;
							if(verbose > 1)
							{
								printf("Match found: ant=%d mjd=%d sec=%d = %15.9f\n", a, model->mjd, model->sec, mjd);
							}
							model->delay[0] += A;
							if(model->order > 0)
							{
								model->delay[1] += B;
							}
							if(model->order > 1)
							{
								model->delay[2] += C;
							}
						}
						if(nLine == 0)
						{
							++nModel;
						}
					}
				}
			}
		}
	}

	if(nModified != nModel)
	{
		printf("WARNING: calcif2: Only %d of %d models modified!\n", nModified, nModel);
	}
	else
	{
		printf("calcif2: All %d models modified.\n", nModel);
	}

	fclose(in);
}

static int runfile(const char *prefix, const CommandLineOptions *opts, CalcParams *p)
{
	DifxInput *D;
	FILE *in;
	char fn[DIFXIO_FILENAME_LENGTH];
	int v;
	const char *difxVersion;

	difxVersion = getenv("DIFX_VERSION");

	v = snprintf(fn, DIFXIO_FILENAME_LENGTH, "%s.calc", prefix);
	if(v >= DIFXIO_FILENAME_LENGTH)
	{
		fprintf(stderr, "Error: filename %s.calc is too long (max %d chars)\n", prefix, DIFXIO_FILENAME_LENGTH-1);
	}
	in = fopen(fn, "r");
	if(!in)
	{
		fprintf(stderr, "File %s not found or cannot be opened.  Quitting.\n", fn);

		return -1;
	}
	else
	{
		fclose(in);
	}

	D = loadDifxCalc(prefix);

	if(D == 0)
	{
		fprintf(stderr, "Error: loadDifxCalc(\"%s\") returned 0\n", prefix);

		return -1;
	}

	D = updateDifxInput(D);
	if(D == 0)
	{
		fprintf(stderr, "Error: updateDifxInput(\"%s\") returned 0\n", prefix);

		return -1;
	}

	if(opts->useExtraExternalDelay == 1)
	{
		const char calcProgram[] = "calc_sekido";
		const int MaxCommandLength = 1024;
		char command[MaxCommandLength];
		char paramsPath[DIFXIO_FILENAME_LENGTH];
		int a, s, v;
		const char *str;

		str = getenv("CALCPARAMS");
		if(str)
		{
			v = snprintf(paramsPath, DIFXIO_FILENAME_LENGTH, "%s", str);
		}
		else
		{
			str = getenv("DIFXROOT");
			if(str == 0)
			{
				fprintf(stderr, "Error: CALCPARAMS or DIFXROOT env var must be set\n");

				exit(EXIT_FAILURE);
			}
			v = snprintf(paramsPath, DIFXIO_FILENAME_LENGTH, "%s/share/calc_sekido", str);
		}
		if(v >= DIFXIO_FILENAME_LENGTH)
		{
			fprintf(stderr, "Error: paramsPath string too short (%d < %d)\n", DIFXIO_FILENAME_LENGTH, v);

			exit(EXIT_FAILURE);
		}

		v = snprintf(command, MaxCommandLength, "%s/blokq.dat", paramsPath);
		if(v >= MaxCommandLength)
		{
			fprintf(stderr, "Error: command string too short (%d < %d) generating BLOKQ envvar\n", MaxCommandLength, v);

			exit(EXIT_FAILURE);
		}
		if(opts->verbose > 0)
		{
			printf("Setting environment variable BLOKQ = %s\n", command);
		}
		setenv("BLOKQ", command, 1);


		/* 1. generate .skd file and .xyz files as needed */
		v = snprintf(command, MaxCommandLength, "calc2skd %s %s", (opts->force ? "--force" : ""), prefix);
		if(v >= MaxCommandLength)
		{
			fprintf(stderr, "Error: command string too short (%d < %d) generating calc2skd command\n", MaxCommandLength, v);

			exit(EXIT_FAILURE);
		}
		if(opts->verbose > 0)
		{
			printf("Executing: %s\n", command);
		}
		system(command);

		/* 2. run calc_skd for each antenna / source */
		for(s = 0; s < D->nSource; ++s)
		{
			char letter = 'A';
			char srcName[10];

			sprintf(srcName, "SRC%05d", s);
			for(a = 0; a < D->nAntenna; ++a)
			{
				if(D->source[s].spacecraftId >= 0)
				{
					v = snprintf(command, MaxCommandLength, "%s %s.%s.skd %s.%s.%s.delay -baseid G%c -finit %s.%s.xyz %s -calcon %s/decont2.input -extf %s/extfile.eop -prtb -atmout", calcProgram, prefix, D->source[s].name, prefix, D->antenna[a].name, D->source[s].name, letter, prefix, D->source[s].name, srcName, paramsPath, paramsPath);
				}
				else
				{
					v = snprintf(command, MaxCommandLength, "%s %s.%s.skd %s.%s.%s.delay -baseid G%c -skd_src %s --calcon %s/decont2.input -extf %s/extfile.eop -prtb -atmout", calcProgram, prefix, D->source[s].name, prefix, D->antenna[a].name, D->source[s].name, letter, srcName, paramsPath, paramsPath);
				}

				if(v >= MaxCommandLength)
				{
					fprintf(stderr, "Error: command string too short (%d < %d) generating calc_skd command\n", MaxCommandLength, v);

					exit(EXIT_FAILURE);
				}
				if(opts->verbose < 2 && v < MaxCommandLength - 20)
				{
					strcat(command, " > /dev/null 2>&1");
				}

				if(opts->verbose > 0)
				{
					printf("Executing: %s\n", command);
				}
				system(command);

				++letter;
				if(letter == 'G')	/* avoid Geocenter code */
				{
					++letter;
				}
			}
		}
	}

	if(opts->force == 0 && skipFile(D->job->calcFile, D->job->imFile))
	{
		printf("Skipping %s due to file ages.\n", prefix);
		deleteDifxInput(D);

		return 0;
	}

	(void) CheckInputForSpacecraft(D, p);
	if((opts->polyOrder)) {
		D->job->polyOrder = opts->polyOrder;
	}
	if((opts->polyInterval)) {
		D->job->polyInterval = opts->polyInterval;
	}
	if(opts->perform_uvw_deriv != PerformDirectionDerivativeDefault)
	{
		D->job->perform_uvw_deriv = opts->perform_uvw_deriv;
	}
	if(opts->perform_lmn_deriv != PerformDirectionDerivativeDefault)
	{
		D->job->perform_lmn_deriv = opts->perform_lmn_deriv;
	}
	if(opts->delta_lmn != 0.0)
	{
		D->job->delta_lmn = opts->delta_lmn;
	}
	if(opts->perform_xyz_deriv != PerformDirectionDerivativeDefault)
	{
		D->job->perform_xyz_deriv = opts->perform_xyz_deriv;
	}
	if(opts->delta_xyz != 0.0)
	{
		D->job->delta_xyz = opts->delta_xyz;
	}
	if((D->job->delta_lmn == 0.0) && ((D->job->perform_uvw_deriv)))
	{
		fprintf(stderr, "Error: calcif2: D->job->delta_lmn is zero, but D->job->perform_uvw_deriv is not None\n");
		return -1;
	}
	if((D->job->delta_lmn == 0.0) && ((D->job->perform_lmn_deriv)))
	{
		fprintf(stderr, "Error: calcif2: D->job->delta_lmn is zero, but D->job->perform_lmn_deriv is not None\n");
		return -1;
	}
	if((D->job->delta_xyz == 0.0) && ((D->job->perform_xyz_deriv)))
	{
		fprintf(stderr, "Error: calcif2: D->job->delta_xyz is zero, but D->job->perform_xyz_deriv is not None\n");
		return -1;
	}
	
	if(opts->aberCorr != DIFXIO_DEFAULT_ABER_CORR_TYPE)
	{
		D->job->aberCorr = opts->aberCorr;
	}

	if(difxVersion && D->job->difxVersion[0])
	{
		if(strncmp(difxVersion, D->job->difxVersion, DIFXIO_VERSION_LENGTH-1))
		{
			printf("Attempting to run calcif2 from version %s on a job make for version %s\n", difxVersion, D->job->difxVersion);
			if(opts->overrideVersion)
			{
				fprintf(stderr, "Continuing because of --override-version\n");
			}
			else
			{
				fprintf(stderr, "calcif2 won't run on mismatched version without --override-version.\n");
				deleteDifxInput(D);

				return -1;
			}
		}
	}
	else if(!D->job->difxVersion[0])
	{
		printf("Warning: calcif2: working on unversioned job\n");
	}

	if((opts->delayServerHost[0]))
	{
		strncpy(D->job->delayServerHost, opts->delayServerHost, DIFXIO_HOSTNAME_LENGTH);
		D->job->delayServerHost[DIFXIO_HOSTNAME_LENGTH-1] = 0;
	}
	if(opts->delayServerType != UnknownServer)
	{
		D->job->delayServerType = opts->delayServerType;
	}
	D->job->delayProgram = delayServerTypeIds[D->job->delayServerType];
	if((opts->delayVersion))
	{
		D->job->delayVersion = opts->delayVersion;
	}
	if((opts->delayHandler))
	{
		D->job->delayHandler = opts->delayHandler;
	}
	

	if(opts->verbose > 1)
	{
		printDifxInput(D);
	}

	v = difxCalcInit(D, p, opts->verbose);
	if(v < 0)
	{
		deleteDifxInput(D);
		fprintf(stderr, "Error: calcif2: difxCalcInit returned %d\n", v);

		return -1;
	}
	v = difxCalc(D, p, prefix, opts->verbose);
	if(v < 0)
	{
		deleteDifxInput(D);
		fprintf(stderr, "Error: calcif2: difxCalc returned %d\n", v);

		return -1;
	}
	if(opts->verbose > 0)
	{
		printf("About to write IM file\n");
	}
	tweakDelays(D, "calcif2.delay", opts->verbose);
	writeDifxIM(D);
	if(opts->verbose > 0)
	{
		printf("Wrote IM file\n");
	}
	deleteDifxInput(D);

	return 0;
}

void deleteCalcParams(CalcParams *p)
{
	if(p->clnt)
	{
		clnt_destroy(p->clnt);
	}
	free(p->request.station.station_val);
	free(p->request.source.source_val);
	free(p->request.EOP.EOP_val);
	free(p);
}

CalcParams *newCalcParams(const CommandLineOptions *opts)
{
	CalcParams *p;

	p = (CalcParams *)calloc(1, sizeof(CalcParams));

	p->oversamp = opts->polyOversamp;
	p->interpol = opts->interpol;

	p->allowNegDelay = opts->allowNegDelay;
	p->warnSpacecraftPointingSource = opts->warnSpacecraftPointingSource;
	p->useExtraExternalDelay = opts->useExtraExternalDelay;
	p->Num_Allocated_Stations = 0;
	p->Num_Allocated_Sources = 0;

	return p;
}

int run(const CommandLineOptions *opts)
{
	CalcParams *p;
	int i, l;

	if(getenv("DIFX_GROUP_ID"))
	{
		umask(2);
	}

	if(opts == 0)
	{
		return EXIT_FAILURE;
	}
		
	p = newCalcParams(opts);
	if(!p)
	{
		fprintf(stderr, "Error: Cannot initialize CalcParams\n");

		return EXIT_FAILURE;
	}

	for(i = 0; i < opts->nFile; ++i)
	{
		l = strlen(opts->files[i]);
		if(l > 6)
		{
			if(strcmp(opts->files[i]+l-6, ".input") == 0)
			{
				opts->files[i][l-6] = 0;
			}
			else if(strcmp(opts->files[i]+l-5, ".calc") == 0)
			{
				opts->files[i][l-5] = 0;
			}
		}
		if(opts->verbose >= 0)
		{
			printf("%s processing file %d/%d = %s\n", program, i+1, opts->nFile, opts->files[i]);
		}
		runfile(opts->files[i], opts, p);
	}
	deleteCalcParams(p);

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	int status;
	CommandLineOptions *opts;

	opts = newCommandLineOptions(argc, argv);

	status = run(opts);

	deleteCommandLineOptions(opts);
	
	return status;
}
