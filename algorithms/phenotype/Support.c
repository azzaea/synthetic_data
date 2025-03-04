#include "Support.h"

int status;
int statusPop;
int statusTrait;

struct pio_file_t InGenoPlink;
snp_t *SNPbuffer;

gsl_rng * r;

// IO file names
char InCausal[1000];
char InRef[1000];
char InSample[1000];
char InGeno[1000];
char OutCausal[1000];
char OutPheno[1000];

int PolyFlag, BinaryFlag;
double pCausal[nMaxTrait];
int CausalFlag[nMaxTrait];
char tmpPCausal[10000];
char tmpPleio[10000];
char tmpGenoEffProp[10000];
char tmpCovarEffProp[10000];
char tmpPopCorr[10000];
char tmpPrev[10000];
char tmpProb[10000];
char tmpWeight[10000];
char tmpTraitCorr[10000];
int nComp;
double * ProbComp;
double * wComp;

// GenoMat: nPop x nIndPerPop 
double PopMatTmp[nMaxPop][3][nMaxInd];
double GenoMat[nMaxInd];
double CovarMat[nMaxCovar][nMaxInd];
int PopIndicator[nMaxInd];
int nSamplePerPop[nMaxPop];
char PopList[nMaxPop][50];
char SampleList[nMaxInd][100];
char CausalList[nMaxTrait][nMaxCausal][50];

double CausalMAF[nMaxPop];
double CausalLDscore;
double CausalAnnot;
double CausalBeta[nMaxPop][nMaxTrait];
// double CovarBeta[nMaxPop][nMaxTrait];

// Include effects from shared causal variants (effect size generated by genetic correlation) and trait specific effects
double BaseBeta[nMaxPop][nMaxTrait]; // Population * Trait
double * GenoEff[nMaxTrait]; 
double * CovarEff[nMaxTrait]; // Covar Eff includes effect from PCs and non genetic covariates (user input)
double * EnvEff[nMaxTrait]; // Include corrlated noise and trait specific noise 
double * PhenoSim[nMaxTrait][2]; // 1--Continious measurement/liability 2--binary diagnosis

double GenoEffProp[nMaxPop][nMaxTrait];
double CovarEffProp[nMaxPop][nMaxTrait];
double EnvEffProp[nMaxPop][nMaxTrait];
double Prev[nMaxPop][nMaxTrait];
// Pleiotropy model with first trait as reference (how many causal SNPs shared with the first trait), starting with 1
double Pleio[nMaxTrait];
double TraitCorr[nMaxTrait][nMaxTrait]; // Only support positive genetic correlation now
double PopCorr[nMaxPop][nMaxPop];

gsl_matrix * GenoBeta;
gsl_matrix * SigmaTrait;
gsl_matrix * SigmaPop;
gsl_matrix * L;
gsl_vector * mu;

double VarGeno[nMaxPop][nMaxTrait];
double VarCovar[nMaxPop][nMaxTrait];
double VarEnv[nMaxPop][nMaxTrait];
double GCEweight[nMaxPop][3];
long int nCausal[nMaxTrait], nSample, nBetaIndex;
int nPop, nTrait, nItem, nValidItem, nCovar;
double a, b, c, prob;

SNPinfo RefSNP[26][200000];
long int SNPct[26];
char buffer[50000000];

void ReadParam(const char *ParIn) {
	FILE *ParFile;
	char *tok; char *p;
	ParFile = fopen(ParIn,"r");
	memset(GenoEffProp, 0.0, sizeof(double) * nMaxPop * nMaxTrait);
	memset(CovarEffProp, 0.0, sizeof(double) * nMaxPop * nMaxTrait);
	a = 0;
	b = 0; 
	c = 0;
	nComp = 1;
	memset(pCausal, 0, sizeof(double) * nMaxTrait);

	if (ParFile == NULL) {
	    printf("Cannot open parameter file.\n");
	    exit(0);
	}
	else {
		while (fgets(buffer, sizeof(buffer), ParFile) != NULL) {
			p = buffer;
			tok = strtok_r(p, " \t", &p);
	    	if (tok != NULL) {
	    		if (strcmp(tok, "nPopulation") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			nPop = atoi(tok);
	    		}
	    		else if (strcmp(tok, "nTrait") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			nTrait = atoi(tok);
	    		}
	    		// Heritability matrix: flatten nPop * nTrait matrix 
	    		else if (strcmp(tok, "PropotionGeno") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(tmpGenoEffProp,tok);
				}
				// Covariate variance ratio matrix: flatten nPop * nTrait matrix 
				else if (strcmp(tok, "PropotionCovar") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(tmpCovarEffProp,tok);
				}
				// Prevalence matrix: flatten nPop * nTrait matrix 
				else if (strcmp(tok, "Prevalence") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(tmpPrev,tok);
	    		}
	    		// Polygenicity vector: nTrait entries 
	    		else if (strcmp(tok, "Polygenicity") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(tmpPCausal,tok);
	    		}
	    		// Pleiotropy vector: nTrait entries 
	    		else if (strcmp(tok, "Pleiotropy") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(tmpPleio,tok);
	    		}
	    		// Trait correlation matrix: flattern nTrait * nTrait matrix
	    		else if (strcmp(tok, "TraitCorr") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(tmpTraitCorr,tok);
	    		}
	    		// Population correlation matrix: flattern nPop * nPop matrix
	    		else if (strcmp(tok, "PopulationCorr") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(tmpPopCorr,tok);
	    		}
	    		else if (strcmp(tok, "a") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			a = atof(tok);
	    		}
	    		else if (strcmp(tok, "b") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			b = atof(tok);
	    		}
	    		else if (strcmp(tok, "c") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			c = atof(tok);
	    		}
	    		else if (strcmp(tok, "nComponent") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			nComp = atoi(tok);
	    		}
	    		else if (strcmp(tok, "CausalList") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(InCausal,tok);
	    		}
	    		else if (strcmp(tok, "Reference") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(InRef,tok);
	    		}
	    		// Sample list: 1st column is string population code, folllowing columns are numeric covariates
	    		else if (strcmp(tok, "SampleList") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(InSample,tok);
	    		}
	    		else if (strcmp(tok, "GenoFile") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(InGeno,tok);
	    		}
	    		else if (strcmp(tok, "Output") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(OutCausal, tok);
					strcat(OutCausal, ".causal");
					strcpy(OutPheno, tok);
					strcat(OutPheno, ".pheno");
	    		}
	    		else if (strcmp(tok, "CompWeight") == 0) {
	    			tok = strtok_r(p, " \t\n", &p);
	    			strcpy(tmpWeight,tok);
	    		}
	    	}
		}
		if ( !strcmp(InRef, "") || !strcmp(InGeno, "") || !strcmp(InSample, "")) {
			printf("Missing input files!\n");
			exit(0);
		}
		if ( !strcmp(InCausal, "") && pCausal<=0) {
			printf("Needs positive polygenicity or causal SNP list!\n");
			exit(0);
		}
		if ( strcmp(InCausal, "") && pCausal>0) {
			printf("Both polygenicity and causal SNP list input, ignore polygenicity.\n");
		}
		if ( strcmp(InCausal, "") )
			PolyFlag = 0;
		else
			PolyFlag = 1;
		if (!strcmp(OutCausal, "")) {
			strcpy(OutCausal, "Output");
			strcat(OutCausal, ".causal");
			strcpy(OutPheno, "Output");
			strcat(OutPheno, ".pheno");
		}
		if (nComp == 1) {
			strcpy(tmpProb,"1");
			strcpy(tmpWeight,"1");
		}
		else if ( !strcmp(tmpWeight, "") ) {
			printf("Missing component weight!\n");
			exit(0);
		}
	}
}


void ExtractParam() {
	char *tok; char *p;
	char tmpBuff[20];
	long int i, k, m, n;
	double SumProb, tmp;

	nItem = nTrait * nPop;
	statusPop = 1;
	statusTrait = 1;
	status = 1;

	// Pop(row) * Trait(col) genetic effect propotion matrix, flatten
	p = tmpGenoEffProp;
    i = 0; k = 0;
    while ((tok = strtok_r(p, ",", &p))) {
    	if (i < nPop && k < nTrait) {
    		if (atof(tok) > 0.0 && atof(tok) < 1.0) {
	    		GenoEffProp[i][k] = atof(tok);
	    		k += 1;
	    		if (k == nTrait) {
	    			i += 1;
	    			k = 0;
	    		}
    		}
    		else {
    			printf("Genetic effect propotion needs to be between 0.0 and 1.0!");
    			exit(0);
    		}
    	}
    	else {
    		printf("Wrong dimension for input flatten heritability matrix!\n");
    		exit(0);
    	}
	}

	// Pop(row) * Trait(col) covar effect propotion matrix, flatten
	p = tmpCovarEffProp;
    i = 0; k = 0;
    while ((tok = strtok_r(p, ",", &p))) {
    	if (i < nPop && k < nTrait) {
    		if (atof(tok) >= 0.0 && atof(tok) <= 1.0) {
	    		CovarEffProp[i][k] = atof(tok);
	    		k += 1;
	    		if (k == nTrait) {
	    			i += 1;
	    			k = 0;
	    		}
    		}
    		else {
    			printf("Covariate effect propotion needs to be between 0.0 and 1.0!");
    			exit(0);
    		}
    	}
    	else {
    		printf("Wrong dimension for input flatten covariate prop matrix!\n");
    		exit(0);
    	}
	}

	for (i = 0; i < nPop; i++) {
		for (k = 0; k < nTrait; k++) {
			tmp = 1 - GenoEffProp[i][k] - CovarEffProp[i][k];
			if (tmp < 0.0) {
				printf("Genetic effect  plus covariate effect propotion needs to be less than 1.0!");
				exit(0);
			}
			else
				EnvEffProp[i][k] = tmp;
		}
	}

	p = tmpPrev;
    i = 0; k = 0; tmp = 0;
    while ((tok = strtok_r(p, ",", &p))) {
    	if (i < nPop && k < nTrait) {
    		if (atof(tok) > 0.0 && atof(tok) < 1.0) {
	    		Prev[i][k] = atof(tok);
	    		tmp += Prev[i][k];
	    		k += 1;
	    		if (k == nTrait) {
	    			i += 1;
	    			k = 0;
	    		}
    		}
    		else {
    			printf("Prevalence needs to be between 0.0 and 1.0!");
    			exit(0);
    		}
    	}
    	else {
    		printf("Wrong dimension for input flatten prevalence matrix!\n");
    		exit(0);
    	}
	}
	BinaryFlag = (tmp ? 1 : 0);

	p = tmpPleio;
    k = 0;
    while ((tok = strtok_r(p, ",", &p))) {
    	if (k < nTrait) {
    		if (k == 0) {
    			if (atof(tok) != 1.0)
					printf("Trait 1 is reference, pleiotropy for trait 1 is set to be 1.0.\n");
    			Pleio[k] = 1.0;
				k += 1;
    		}
    		else if (atof(tok) >= 0.0 && atof(tok) <= 1.0) {
	    		Pleio[k] = atof(tok);
	    		k += 1;
    		}
    		else {
    			printf("Pleiotropy needs to be between 0.0 and 1.0!\n");
    			exit(0);
    		}
    	}
    	else {
    		printf("Wrong length for input pleiotropy vector!\n");
    		exit(0);
    	}
	}
	
	p = tmpPCausal;
    k = 0;
    while ((tok = strtok_r(p, ",", &p))) {
    	if (k < nTrait) {
    		pCausal[k] = atof(tok);
			pCausal[k] = ((k == 0) ? pCausal[k] : (pCausal[k] - pCausal[0]*Pleio[k]));
			k += 1;
    		if (pCausal[k] < 0.0 || pCausal[k] > 1.0) {
    			printf("Given the pleiotropic model, polygenicity needs to be between 0.0 and 1.0!");
    			exit(0);
    		}
    	}
    	else {
    		printf("Wrong length for input polygenicity vector!\n");
    		exit(0);
    	}
	}

	p = tmpTraitCorr;
	m = 0; n = 0;
	while ((tok = strtok_r(p, ",", &p))) {
		if (m < nTrait && n < nTrait) {
			if (atof(tok) >= -1.0 && atof(tok) <= 1.0) {
				TraitCorr[m][n] = atof(tok);
				n += 1;
				if (n == nTrait) {
					m += 1;
					n = 0;
				}
	    	}
	    	else {
	    		printf("Trait correlation out of bound!\n");
	    		exit(0);
	    	}
	    }
	    else {
    		printf("Wrong dimension for input flatten trait correlation matrix!\n");
    		exit(0);
    	}
	}

	p = tmpPopCorr;
	m = 0; n = 0;
	while ((tok = strtok_r(p, ",", &p))) {
		if (m < nPop && n < nPop) {
			if (atof(tok) >= -1.0 && atof(tok) <= 1.0) {
				PopCorr[m][n] = atof(tok);
				n += 1;
				if (n == nPop) {
					m += 1;
					n = 0;
				}
	    	}
	    	else {
	    		printf("Population correlation out of bound!\n");
	    		exit(0);
	    	}
	    }
	    else {
    		printf("Wrong dimension for input flatten population correlation matrix!\n");
    		exit(0);
    	}
	}

	if (nItem > 1)
		MakeCovMat();
	else
		nValidItem = 1;

	GenoBeta = gsl_matrix_calloc(nValidItem, nMaxBetaGen);

	p = tmpWeight;
    i = 0; //weights counter
    SumProb = 0;
    wComp = malloc(sizeof(double) * nComp);
    memset(wComp, 0, sizeof(double) * nComp);
    ProbComp = malloc(sizeof(double) * nComp);
    memset(ProbComp, 0, sizeof(double) * nComp);
    while ((tok = strtok_r(p, ",", &p))) {
    	if (i < nComp) {
    		if (atof(tok) > 0.0) {
	    		wComp[i] = atof(tok);
	    		ProbComp[i] = 1.0/wComp[i];
	    		SumProb += ProbComp[i];
    		}
    		else
    			printf("Component weight needs to be positive!");
    	}
    	else {
    		printf("Wrong number of component weights!\n");
    		exit(0);
    	}
    	i++;
	}
	for (i = 0; i < nComp; i++) {
    	ProbComp[i] = ProbComp[i]/SumProb;
    	if (i > 0)
    		ProbComp[i] += ProbComp[i-1];
    }
}


// Order of variables: Trait 1 Pop1,2,3 ... Trait 2 Pop 1,2,3 ... Trait 3 ...
void MakeCovMat() {
	int i, j, m, n, nRow, nCol;
	double tmp; 

	gsl_matrix * Sigma = gsl_matrix_calloc(nItem, nItem);
	for (i = 0; i < nTrait; i++) {
		for (j = 0; j < nPop; j++){
			nRow = nPop * i + j;
			for (m = 0; m < nTrait; m++) {
				for (n = 0; n < nPop; n++){
					nCol = nPop * m + n;
					tmp = PopCorr[j][n] * TraitCorr[i][m];
					gsl_matrix_set(Sigma, nRow, nCol, tmp);
				}
			}
		}
	}
	status = gsl_linalg_cholesky_decomp1(Sigma);

	SigmaPop = gsl_matrix_calloc(nPop, nPop);
	for (i = 0; i < nPop; i++) {
		for (j = 0; j < nPop; j++)
			gsl_matrix_set(SigmaPop, i, j, PopCorr[i][j]);
	}
	gsl_permutation *permPop = gsl_permutation_alloc(nPop);
	statusPop = gsl_linalg_cholesky_decomp1(SigmaPop);

	SigmaTrait = gsl_matrix_calloc(nTrait, nTrait);
	for (m = 0; m < nTrait; m++) {
		for (n = 0; n < nTrait; n++)
			gsl_matrix_set(SigmaTrait, m, n, TraitCorr[m][n]);
	}
	statusTrait = gsl_linalg_cholesky_decomp1(SigmaTrait);

	if (!status) {
		printf("Overall correlation matrix is positive definite, ok!\n");
		nValidItem = nItem;
		L = gsl_matrix_calloc(nValidItem, nValidItem);
		gsl_matrix_memcpy(L, Sigma);
	}
	else if (statusPop && !statusTrait) {
		printf("Population correlation matrix is not positive definite. Assuming all population corr = 1.0.\n");
		nValidItem = nTrait;
		L = gsl_matrix_calloc(nValidItem, nValidItem);
		gsl_matrix_memcpy(L, SigmaTrait);
	}
	else if (statusTrait && !statusPop) {
		printf("Trait correlation matrix is not positive definite. Assuming all trait corr = 1.0.\n");
		nValidItem = nPop;
		L = gsl_matrix_calloc(nValidItem, nValidItem);
		gsl_matrix_memcpy(L, SigmaPop);
	}
	else if (statusPop && statusTrait) {
		printf("Neither correlation matrix is positive definite. Assuming all corr = 1.0.\n");
		nValidItem = 1;
	}
	else {
		// If not possible to satisfy both, always try to meet the trait correlation.
		printf("Both correlation matrices are positive definite, but overall correlation matrix is not. Assuming pop corr = 1.0.\n");
		nValidItem = nTrait;
		L = gsl_matrix_calloc(nTrait, nTrait);
		gsl_matrix_memcpy(L, SigmaTrait);
	}
	mu = gsl_vector_calloc(nValidItem);
	gsl_vector_set_zero(mu);
}


int PopIndex(char PopCode[50], int nPopCt) {
	int i;
	for (i = 0; i < nPopCt; i++) {
		if (strcmp(PopCode, PopList[i]) == 0) {
			return(i);
		}
	}
	return(nPopCt);
}


// Sample list: one column of population code, in the order of sample list in the header of .traw input
void ReadPopulation() {
	char *tok; char *p;
	char PopCode[50];
	int nPopCt;
	long int i, j;
	nPopCt = 0;
	nCovar = 0;
	memset(nSamplePerPop, 0, sizeof(int) * nMaxPop);
	FILE *InFileSample;
    InFileSample = fopen(InSample, "r");
    if (InFileSample == NULL) {
        printf("Cannot open the Sample list %s.\n", InSample);
        exit(0);
    }
    else {
		i = 0; // Line counter
		while (fgets(buffer, sizeof(buffer), InFileSample) != NULL) {
			p = buffer;
			tok = strtok_r(p, " ,\t\n", &p); // Population Code
			strcpy(PopCode, tok);
			if (nPopCt == 0) {
				strcpy(PopList[nPopCt], PopCode);
				PopIndicator[i] = nPopCt;
				nPopCt += 1;
			}
			else {
				PopIndicator[i] = PopIndex(PopCode, nPopCt);
				if (PopIndicator[i] == nPopCt) {
					strcpy(PopList[nPopCt], PopCode);
					nPopCt += 1;
				}
			}
			if (nPopCt >= nMaxPop) {
				printf("Increase max Population limit!\n");
				exit(0);
			}
			nSamplePerPop[PopIndicator[i]] += 1;

			j = 0;
			while ((tok = strtok_r(p, " ,\t\n", &p))) {
				if (j < nMaxCovar)
					CovarMat[j++][i] = atof(tok);
				else
					printf("Increase max Covariate limit!\n");
		    }
		    if (nCovar == 0)
		    	nCovar = j;
		    else if (j != nCovar)
		    	printf("Line %ld does not have %d covariates.\n", i+1, nCovar);
		    i++;
		}
		fclose(InFileSample);
		nSample = i;
		printf("Input %ld samples from population list\n", nSample);
	}
	if (nPopCt != nPop) {
		printf("Mismatch population number.\n");
		exit(0);
	}
}


void ReadCausal() {
	long int i, k;
	char *tok; char *p;
	char tmpInCausal[1000];
	char tmpBuff[20];
	FILE *InFileCausal;
	memset(nCausal, 0, sizeof(int) * nMaxTrait);
	for (k = 0; k < nTrait; k++) {
		strcpy(tmpInCausal, InCausal);
		sprintf(tmpBuff, "%d", (int)k+1);
	    InFileCausal = fopen(strcat(tmpInCausal, tmpBuff), "r");
	    if (InFileCausal == NULL) {
	        printf("Cannot open the Causal SNP list %s.\n", InCausal);
	        exit(0);
	    }
	    else {
			while (fgets(buffer, sizeof(buffer), InFileCausal) != NULL) {
				p = buffer;
				tok = strtok_r(p, " ,\t\n", &p);
				if ( strlen(tok) != 0) {
				    strcpy(CausalList[k][nCausal[k]], tok);
				    nCausal[k] += 1;
		    	}
			}
			printf("Trait %ld: input %ld causal SNPs.\n", k, nCausal[k]);
			fclose(InFileCausal);
		}
	}
}


void ReadRef() {
	long int i, SNPindex;
	int chr;
	char *tok; char *p;

	memset(SNPct, 0, sizeof(int) * 26);

	FILE *InFileRef;
    InFileRef = fopen(InRef, "r");
    if (InFileRef == NULL) {
        printf("Cannot open the Reference file %s.\n", InRef);
        exit(0);
    }
    else {
    	i = 0; // Line counter
    	fgets(buffer, sizeof(buffer), InFileRef); //header
		while (fgets(buffer, sizeof(buffer), InFileRef) != NULL) {
			SNPinfo snp;
			p = buffer;
			tok = strtok_r(p, " ,\t\n", &p); // Chr
			if ( strlen(tok) != 0) {
			    chr = atoi(tok)-1;
	    	}
	    	tok = strtok_r(p, " ,\t\n", &p); // Causal SNP
			if ( strlen(tok) != 0) {
			    strcpy(snp.SNP, tok);
	    	}
			tok = strtok_r(p, " ,\t\n", &p); // A1
			tok = strtok_r(p, " ,\t\n", &p); // A2
			tok = strtok_r(p, " ,\t\n", &p); // MAF
			if ( strlen(tok) != 0) {
			    snp.AfricaMAF = atof(tok);
	    	}
	    	tok = strtok_r(p, " ,\t\n", &p); // LDscore
			if ( strlen(tok) != 0) {
			    snp.AfricaLDscore = atof(tok);
	    	}
	    	tok = strtok_r(p, " ,\t\n", &p); // Annot
	    	tok = strtok_r(p, " ,\t\n", &p); // Gene
	    	tok = strtok_r(p, " ,\t\n", &p); // Exon
	    	if ( strlen(tok) != 0) {
			    if (atoi(tok) != 0) 
			    	snp.exon = 1;
			    else
			    	snp.exon = 0;
	    	}
	    	tok = strtok_r(p, " ,\t\n", &p); // DHS
	    	if ( strlen(tok) != 0) {
			    snp.DHS = atoi(tok);
	    	}
			SNPindex = SNPct[chr];
	    	RefSNP[chr][SNPindex] = snp;
	    	SNPct[chr]++;
			i++;
		}
		fclose(InFileRef);
		printf("Read reference file, done. %ld SNPs read from reference.\n", i);
	}
}


void BaseBetaGen() {
	int i, j;
	nBetaIndex = 0;
	gsl_matrix_set_zero(GenoBeta);
	for (i = 0; i < nValidItem; i++) {
		for (j = 0; j < nMaxBetaGen; j++)
			gsl_matrix_set(GenoBeta, i, j, gsl_ran_gaussian(r, 1.0));
	}

	if (nValidItem > 1) {
		gsl_matrix * tmpCorrGenoBeta = gsl_matrix_calloc(nValidItem, nMaxBetaGen);
		gsl_matrix * tmpL = gsl_matrix_calloc(nValidItem, nValidItem);
		gsl_matrix_set_zero(tmpL);
		for (i = 0; i < nValidItem; i++) {
			for (j = 0; j <= i; j++) {
				gsl_matrix_set(tmpL, i, j, gsl_matrix_get(L, i, j));
			}
		}
		gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, tmpL, GenoBeta, 0.0, tmpCorrGenoBeta);
		gsl_matrix_memcpy(GenoBeta, tmpCorrGenoBeta);
		gsl_matrix_free(tmpCorrGenoBeta);
		gsl_matrix_free(tmpL);
	}
}



void BaseBetaGet(double sigma) {
	int i, j;
	memset(BaseBeta, 0.0, sizeof(double)*nMaxPop*nMaxTrait);

	if (!status) {
		for (i = 0; i < nTrait; i++) {
			for (j = 0; j < nPop; j++)
				BaseBeta[j][i] = gsl_matrix_get(GenoBeta, nPop*i+j, nBetaIndex) * sigma;
		}
	}
	else if (!statusTrait) {
		for (i = 0; i < nTrait; i++) {
			for (j = 0; j < nPop; j++) {
				BaseBeta[j][i] = gsl_matrix_get(GenoBeta, i, nBetaIndex) * sigma;
			}
		}
	}
	else if (!statusPop) {
		for (i = 0; i < nPop; i++) {
			for (j = 0; j < nTrait; j++) {
				BaseBeta[i][j] = gsl_matrix_get(GenoBeta, i, nBetaIndex) * sigma;
			}
		}
	}
	else {
		for (i = 0; i < nPop; i++) {
			for (j = 0; j < nTrait; j++)
				BaseBeta[i][j] = gsl_matrix_get(GenoBeta, 0, nBetaIndex) * sigma;
		}
	}
	nBetaIndex++;
	if (nBetaIndex == nMaxBetaGen) 
		BaseBetaGen();
}



void GetCovarEff() {
	int i, j, k, popIndex;
	long int l;
	// memset(CovarBeta, 0.0, sizeof(double)*nMaxPop*nMaxTrait);
	for (k = 0; k < nCovar; k++) {
		BaseBetaGet(1.0);
		for (i = 0; i < nTrait; i++) {
			for (l = 0; l < nSample; l++) {
				popIndex = PopIndicator[l];
				CovarEff[i][l] += BaseBeta[popIndex][i] * CovarMat[k][l];
			}
		}
	}
}


void GetEnvEff() {
	int i, j, k;
	long int l;
	gsl_matrix * tmpEnvEff = gsl_matrix_calloc(nTrait, nSample);
	for (l = 0; l < nSample; l++) {
		for (k = 0; k < nTrait; k++)
			gsl_matrix_set(tmpEnvEff, k, l, gsl_ran_gaussian(r, 1.0));
	}

	if (!statusTrait) {
		gsl_matrix * tmpCorrEnvEff = gsl_matrix_calloc(nTrait, nSample);
		gsl_matrix * tmpL = gsl_matrix_calloc(nTrait, nTrait);
		gsl_matrix_set_zero(tmpL);
		for (i = 0; i < nTrait; i++) {
			for (j = 0; j <= i; j++)
				gsl_matrix_set(tmpL, i, j, gsl_matrix_get(SigmaTrait, i, j));
		}
		gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, tmpL, tmpEnvEff, 0.0, tmpCorrEnvEff);
		gsl_matrix_memcpy(tmpEnvEff, tmpCorrEnvEff);
		gsl_matrix_free(tmpCorrEnvEff);
		gsl_matrix_free(tmpL);
	}
	else if (statusTrait && nTrait > 1) {
		printf("Trait correlation matrix is not positive definite, assuming enveromental effect independent.\n");
	}

	for (l = 0; l < nSample; l++) {
		for (k = 0; k < nTrait; k++)
			EnvEff[k][l] = gsl_matrix_get(tmpEnvEff, k, l);
	}
	gsl_matrix_free(tmpEnvEff);
}


double IsCausal(char SNP[50]) {
	memset(CausalFlag, 0, sizeof(int)*nMaxTrait);
	int i, k, flag;
	flag = 0;
	if (PolyFlag) {
		prob = gsl_rng_uniform(r);
		if (prob < pCausal[0]) {
			CausalFlag[0] = 1;
			for (k = 1; k < nTrait; k++) {
				prob = gsl_rng_uniform(r);
				CausalFlag[k] = ((prob < Pleio[k]) ? 1 : 0);
			}
		}
		else {
			CausalFlag[0] = 0;
			for (k = 1; k < nTrait; k++) {
				prob = gsl_rng_uniform(r);
				CausalFlag[k] = ((prob < pCausal[k]) ? 1 : 0);
			}
		}
	}
	else {
		for (k = 0; k < nTrait; k++) {		
			for (i = 0; i < nCausal[k]; i++) {
				if (!strcmp(SNP, CausalList[k][i])) {
					CausalFlag[k] = 1;
					break;
				}
			}
		}
	}
	for (k = 0; k < nTrait; k++)
		flag += CausalFlag[k];
	flag = ((flag > 0) ? 1.0 : 0.0);
	return(flag);
}


long int FindSNPinRef(char SNP[50], int chr) {
	int i; 
	for (i = 0; i < SNPct[chr]; i++) {
		// printf("i = %d, SNP = %s, total SNP on chr = %d\n", i, RefSNP[chr][i].SNP, SNPct[chr]);
		if (strcmp(SNP, RefSNP[chr][i].SNP) == 0)
			return(i);
	}
	return(-1);
}


double GetMAF(int popIndex) {
	double freq;
	long int i, j;
	double * tmpGeno;
	tmpGeno = malloc(sizeof(double) * nSamplePerPop[popIndex]);
	j = 0;
	for (i = 0; i < nSamplePerPop[popIndex]; i++) {
		if (PopMatTmp[popIndex][0][i] != 3.0) {
			tmpGeno[j] = PopMatTmp[popIndex][0][i];
			j++;
		}
	}
	freq = gsl_stats_mean(tmpGeno, 1, j)/2.0;
	freq = (freq > 0.5) ? (1-freq) : freq;
	free(tmpGeno);
	return(freq);
}


double GetBeta(double BaseBeta, int popIndex) {
	if (CausalMAF[popIndex] == 0.0)
		return(0.0);
	else if (CausalLDscore == 0.0)
		CausalLDscore = 0.0001; // will run into problem if divide 0; therefore use 0.0001 as minimum
	double fMAF = pow(CausalMAF[popIndex]*(1-CausalMAF[popIndex]), a);
	double fLD = pow(CausalLDscore, b);
	double fScore = pow(CausalAnnot, c);
	double sigma2 = fMAF*fLD*fScore;
	return(BaseBeta * sqrt(sigma2));
}


void AnalyzeSNP(int chr, char SNP[50]) {
	double prob;
	long int i, j, k, n, SNPindex, popIndex, tmpMU;
	char tmp[50];
	char tmpMAF[500];
	char tmpBeta[500];
	char tmpBaseBeta[500];
	char tmpOutCausal[1000];
	char tmpBuff[20];
	FILE *OutFileCausal;
	memset(CausalBeta, 0.0, sizeof(double)*nMaxPop*nMaxTrait);
	memset(CausalMAF, 0.0, sizeof(double)*nMaxPop);

	SNPindex = FindSNPinRef(SNP, chr);
    if (SNPindex == -1) {
		CausalLDscore = 0.25;
		CausalAnnot = 1.0;
	}
	else {
    	CausalLDscore = RefSNP[chr][SNPindex].AfricaLDscore;
    	CausalAnnot = RefSNP[chr][SNPindex].DHS + 1;
    }

    memset(tmpMAF, '\0', sizeof tmpMAF);
    for (n = 0; n < nPop; n++) {
    	CausalMAF[n] = GetMAF(n);
    	sprintf(tmp, "%f,", CausalMAF[n]);
		strcat(tmpMAF, tmp);
    }
    tmpMAF[strlen(tmpMAF)-1] = '\0';

    prob = gsl_rng_uniform(r);
    if (prob < ProbComp[0])
    	k = 0;
    else {
		for (j = 1; j < nComp; j++) {
			if ((prob > ProbComp[j-1]) && (prob < ProbComp[j])) {
				k = j;
				break;
			}
		}
    }
    BaseBetaGet(wComp[k]);
	for (k = 0; k < nTrait; k++) {
		if (CausalFlag[k]) {
			memset(tmpBeta, '\0', sizeof tmpBeta);
		    memset(tmpBaseBeta, '\0', sizeof tmpBaseBeta);
			for (i = 0; i < nPop; i++) {
				CausalBeta[k][i] = GetBeta(BaseBeta[i][k], i);
				sprintf(tmp, "%f,", CausalBeta[k][i]);
				strcat(tmpBeta, tmp);
				sprintf(tmp, "%f,", BaseBeta[i][k]);
				strcat(tmpBaseBeta, tmp);
			}
			tmpBeta[strlen(tmpBeta)-1] = '\0';
	    	tmpBaseBeta[strlen(tmpBaseBeta)-1] = '\0';
	    	strcpy(tmpOutCausal, OutCausal);
	    	sprintf(tmpBuff, "%d", (int)k+1);
			OutFileCausal = fopen(strcat(tmpOutCausal, tmpBuff),"a");
		    fprintf(OutFileCausal, "%s\t%s\t%s\t%lf\t%s\n", SNP, tmpBaseBeta, tmpMAF, CausalLDscore, tmpBeta);
			fclose(OutFileCausal);
	    }
	    else {
	    	for (i = 0; i < nPop; i++)
				CausalBeta[k][i] = 0.0;
	    }
	}

    for (i = 0; i < nSample; i++) {
    	popIndex = PopIndicator[i];
    	for (k = 0; k < nTrait; k++)
    		GenoEff[k][i] += CausalBeta[k][popIndex]*GenoMat[i];
    }
}

