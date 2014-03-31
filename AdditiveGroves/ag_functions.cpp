// Additive Groves / ag_functions.cpp: implementations of Additive Groves global functions
//
// (c) Daria Sorokina

#include "ag_functions.h"
#include "functions.h"
#include "LogStream.h"
#include "Grove.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <cmath>

//generates all output for train, expand and merge commands except for saving the models themselves
void trainOut(TrainInfo& ti, doublevv& dir, doublevvv& rmsV, doublevvv& surfaceV, doublevvv& predsumsV, 
			  int itemN, doublevv& dirStat, int startAlphaNo, int startTiGNNo)
{
//Generate temp files that can be used by other commands later. (in addition to saved groves)

	int alphaN = getAlphaN(ti.minAlpha, itemN); //number of different alpha values
	int tigNN = getTiGNN(ti.maxTiGN);	//number of different tigN values
	LogStream clog;
	
	//params file is a text file
	fstream fparam;	
	fparam.open("./AGTemp/params.txt", ios_base::out); 
	//general set of parameters
	fparam << ti.seed << '\n' << ti.trainFName << '\n' << ti.validFName << '\n' << ti.attrFName 
		<< '\n' << ti.minAlpha << '\n' << ti.maxTiGN << '\n' << ti.bagN << '\n';
	//speed and directions matrix
	if(ti.mode == FAST)
	{
		fparam << "fast" << endl;

		if(!dir.empty())
		{
			fstream fdir;	
			fdir.open("./AGTemp/dir.txt", ios_base::out); 
			for(int tigNNo = 0; tigNNo < tigNN; tigNNo++)
			{
				for(int alphaNo = 0; alphaNo < alphaN; alphaNo++)
					fdir << dir[tigNNo][alphaNo] << '\t';
				fdir << endl;
			}
			fdir.close();
		}
	}
	else if(ti.mode == SLOW)
		fparam << "slow" << endl;
	else //if(ti.mode == LAYERED)
		fparam << "layered" << endl;

	//performance metric
	if(ti.rms)
		fparam << "rms" << endl;
	else
		fparam << "roc" << endl;

	fparam.close();

	//save rms performance matrices for every bagging iteration
	fstream fbagrms;	
	fbagrms.open("./AGTemp/bagrms.txt", ios_base::out); 
	for(int bagNo = 0; bagNo < ti.bagN; bagNo++)
	{
		fbagrms << endl;
		for(int tigNNo = 0; tigNNo < tigNN; tigNNo++)
		{
			for(int alphaNo = 0; alphaNo < alphaN; alphaNo++)
				fbagrms << rmsV[tigNNo][alphaNo][bagNo] << '\t';
			fbagrms << endl;
		}
	}
	fbagrms.close();

	//same for roc performance, if applicable
	if(!ti.rms)
	{
		fstream fbagroc;	
		fbagroc.open("./AGTemp/bagroc.txt", ios_base::out); 
		for(int bagNo = 0; bagNo < ti.bagN; bagNo++)
		{
			fbagroc << endl;
			for(int tigNNo = 0; tigNNo < tigNN; tigNNo++)
			{
				for(int alphaNo = 0; alphaNo < alphaN; alphaNo++)
					fbagroc << surfaceV[tigNNo][alphaNo][bagNo] << '\t';
				fbagroc << endl;
			}
		}
		fbagroc.close();
	}

	//save sums of predictions on the last bagging iteration
	fstream fsums;	
	fsums.open("./AGTemp/predsums.bin", ios_base::binary | ios_base::out);
	fsums << predsumsV;
	fsums.close();


//Generate the actual output of the program

	//performance table on the last iteration of bagging
	fstream fsurface;	//output text file with performance matrix
	fsurface.open("performance.txt", ios_base::out); 

	double bestPerf;					//best performance on validation set
	int bestTiGNNo, bestAlphaNo;		//ids of parameters that produce best performance
	int bestTiGN; double bestAlpha;		//parameters that produce best performance
	//output performance matrix, find best performance and corresponding parameter values
	for(int tigNNo = 0; tigNNo < tigNN; tigNNo++)
	{
		int tigN = tigVal(tigNNo);
		for(int alphaNo = 0; alphaNo < alphaN; alphaNo++)
		{
			double alpha;
			if(alphaNo < alphaN - 1)
				alpha = alphaVal(alphaNo);
			else //this is a special case because minAlpha can be zero
				alpha = ti.minAlpha;
			
			//output a single grid point with coordinates
			double& curPerf = surfaceV[tigNNo][alphaNo][ti.bagN - 1];
			fsurface << alpha << " \t" << tigN << " \t" << curPerf << endl;

			//check for the best result inside the active output area
			if((tigNNo >= startTiGNNo) && (alphaNo >= startAlphaNo))
				if((tigNNo == startTiGNNo) && (alphaNo == startAlphaNo) || 
					ti.rms && (curPerf < bestPerf) ||
					!ti.rms && (curPerf > bestPerf) ||
					((curPerf == bestPerf) && //if the result is the same, choose the less complex model
						(pow((double)2, alphaNo) * tigN < pow((double)2, bestAlphaNo) * bestTiGN))
					)	
				{
					bestTiGNNo = tigNNo;
					bestTiGN = tigN;
					bestAlphaNo = alphaNo;
					bestAlpha = alpha;
					bestPerf = curPerf;
				}
		}		
	}
	
	fsurface << "\n\n";

	//output the same number in the form of matrix
	for(int alphaNo = 0; alphaNo < alphaN; alphaNo++)
	{
		for(int tigNNo = 0; tigNNo < tigNN; tigNNo++)
			fsurface << surfaceV[tigNNo][alphaNo][ti.bagN - 1] << " \t";
		fsurface << endl;
	}
	fsurface.close();

	fstream fdirStat;	
	fdirStat.open("./AGTemp/dirstat.txt", ios_base::out);
	//output directions stats in the form of matrix
	for(int alphaNo = 0; alphaNo < alphaN; alphaNo++)
	{
		for(int tigNNo = 0; tigNNo < tigNN; tigNNo++)
			fdirStat << dirStat[tigNNo][alphaNo] / ti.bagN << " \t";
		fdirStat << endl;
	}
	fdirStat.close();


	//add bestPerf, bestTiGN, bestAlpha, bagN, trainN to AGTemp\best.txt 
		//bestPerf will be used in the output on the next iteration, others are used by ag_save.exe
	//best.txt file is a text file
	fstream fbest;
	fbest.open("./AGTemp/best.txt", ios_base::out); 
	fbest << bestPerf << '\n' << bestTiGN << '\n' << bestAlpha << '\n' << ti.bagN << '\n' << itemN 
		<< endl;
	fbest.close();	

	//output rms bagging curve in the best (alpha, TiGN) point
	fstream frmscurve;	//output text file 
	frmscurve.open("bagging_rms.txt", ios_base::out); 
	for(int bagNo = 0; bagNo < ti.bagN; bagNo++)
		frmscurve << rmsV[bestTiGNNo][bestAlphaNo][bagNo] << endl;
	frmscurve.close();

	//same for roc curve, if applicable
	if(!ti.rms)
	{
		fstream froccurve;	//output text file 
		froccurve.open("bagging_roc.txt", ios_base::out); 
		for(int bagNo = 0; bagNo < ti.bagN; bagNo++)
			froccurve << surfaceV[bestTiGNNo][bestAlphaNo][bagNo] << endl;
		froccurve.close();
	}

	//analyze whether more bagging should be recommended based on the rms curve in the best point
	bool recBagging = moreBag(rmsV[bestTiGNNo][bestAlphaNo]);
	//and based on the curve in a more complex point (if there is one)
	recBagging |= moreBag(rmsV[min(bestTiGNNo + 1, tigNN - 1)][min(bestAlphaNo + 1, alphaN - 1)]);
		
	//output results and recommendations
	clog << "Best model:\n\tAlpha = " << bestAlpha << "\n\tN = " << bestTiGN;
	if(ti.rms)
		clog << "\nRMSE on validation set = " << bestPerf << "\n";
	else
		clog << "\nROC on validation set = " << bestPerf << "\n";

	//if the best possible performance is not achieved,
	//and the best value is on the border, or bagging has not yet converged, recommend expanding
	if( (ti.rms && (bestPerf != 0) || !ti.rms && (bestPerf != 1)) &&
		(((bestAlpha == ti.minAlpha) && (ti.minAlpha != 0)) || (bestTiGN == ti.maxTiGN) || recBagging))
	{
		clog << "\nRecommendation: relaxing model parameters might produce a better model.\n"
			<< "Suggested action: ag_expand";
		if((bestAlpha == ti.minAlpha) && (ti.minAlpha != 0))
		{
			double recAlpha = ti.minAlpha * 0.1;
			//make sure that you don't recommend alpha that is too small for this data set
			if(1/(double)itemN >= recAlpha)
				recAlpha = 0;
			clog << " -a " << recAlpha;
		}
		if(bestTiGN == ti.maxTiGN)
		{
			int recTiGN = tigVal(getTiGNN(ti.maxTiGN) + 1);
			clog << " -n " << recTiGN;
		}
		if(recBagging)
		{
			int recBagN = ti.bagN + 40;
			clog << " -b " << recBagN;
		}
		clog << "\n";
	}
	else
		clog << "\nYou can save the best model for the further use. \n" 
			<< "Suggested action: ag_save -a " << bestAlpha << " -n " << bestTiGN << "\n";
	clog << "\n";
}

//saves a vector into a binary file
fstream& operator << (fstream& fbin, doublev& vec)
{
	int n = (int) vec.size();
	for(int i = 0; i < n; i++)
		fbin.write((char*) &(vec[i]), sizeof(double)); 
	return fbin;
}

//saves a matrix represented as vector of vectors into a binary file
fstream& operator << (fstream& fbin, doublevv& mx)
{
	int n = (int) mx.size();
	for(int i = 0; i < n; i++)
		fbin << mx[i];
	return fbin;
}

//reads a vector from a binary file
//vector should have the right size already
fstream& operator >> (fstream& fbin, doublev& vec)
{
	int n = (int) vec.size();
	for(int i = 0; i < n; i++)
		fbin.read((char*) &(vec[i]), sizeof(double)); 
	return fbin;
}

//reads a matrix represented as vector of vectors from a binary file
//all vectors should have the required size already
fstream& operator >> (fstream& fbin, doublevv& mx)
{
	int n = (int) mx.size();
	for(int i = 0; i < n; i++)
		fbin >> mx[i];
	return fbin;
}

//saves a vector of vectors of vectors into a binary file
fstream& operator << (fstream& fbin, doublevvv& trivec)
{
	int n = (int) trivec.size();
	for(int i = 0; i < n; i++)
		fbin << trivec[i];
	return fbin;

}

//reads a vector of vectors of vectors from a binary file
fstream& operator >> (fstream& fbin, doublevvv& trivec)
{
	int n = (int) trivec.size();
	for(int i = 0; i < n; i++)
		fbin >> trivec[i];
	return fbin;
}

//converts min alpha value into the number of alpha values
int getAlphaN(double minAlpha, int trainN)
{
	int alphaN; 
	for(alphaN = 0; 
		(minAlpha < alphaVal(alphaN) - 0.000000000000001) && //to adjust for rounding errors
			(1 / (double)trainN < alphaVal(alphaN) - 0.000000000000001); 
		alphaN++)
		;
	alphaN++;

	return alphaN;
}

//converts max tigN value into the number of tigN values
int getTiGNN(int tigN)
{
	int tigValue = 1;
	for(int tigNNo = 0; tigNNo < tigN + 1; tigNNo++)
		if(tigVal(tigNNo) > tigN)
			return tigNNo;

	return -1;
}

//Converts the number of a valid alpha value into the actual value
double alphaVal(int alphaNo)
{
	//ordered valid values: 0.5, 0.2, 0.1, 0.05, 0.02, 0.01, 0.005, ...  
	double alpha = pow(0.1, alphaNo / 3 + 1);
	if(alphaNo % 3 == 0)
		alpha *= 5;
	else if(alphaNo % 3 == 1)
		alpha *= 2;

	return alpha;
}

//Converts the number of a valid TiG value into the actual value
int tigVal(int tigNNo)
{
	return (int) (sqrt(pow(2.0, tigNNo + 1)) + 0.5);
}

//Rounds tigN down to the closest appropriate value
double adjustTiGN(int tigN)
{
	int tigNN = getTiGNN(tigN);
	return tigVal(tigNN - 1);
}

//The following code for linux-compatible string itoa is copied from the web site of Stuart Lowe
//http://www.jb.man.ac.uk/~slowe/cpp/itoa.html
/**
	
 * C++ version std::string style "itoa":
	
 */
	
std::string itoa(int value, int base) {
	

	
	enum { kMaxDigits = 35 };
	
	std::string buf;
	
	buf.reserve( kMaxDigits ); // Pre-allocate enough space.
	

	
	// check that the base if valid
	
	if (base < 2 || base > 16) return buf;
	

	
	int quotient = value;
	

	
	// Translating number to string with base:
	
	do {
	
		buf += "0123456789abcdef"[ std::abs( quotient % base ) ];
	
		quotient /= base;
	
	} while ( quotient );
	

	
	// Append the negative sign for base 10
	
	if ( value < 0 && base == 10) buf += '-';
	

	
	std::reverse( buf.begin(), buf.end() );
	
	return buf;
}


//trains a Layered Groves ensemble (Additive Groves trained in layered style) 
//if modelFName is not empty, saves the model
//returns performance on validation set
double layeredGroves(INDdata& data, TrainInfo& ti, string modelFName)
{
	doublev validTar; //true response values on validation set
	int validN = data.getTargets(validTar, VALID); 
	doublev predsumsV(validN, 0); 	//sums of predictions for each data point
	
	if(!modelFName.empty())
	{//save the model's header
		fstream fmodel(modelFName.c_str(), ios_base::binary | ios_base::out);
		fmodel.write((char*) &ti.mode, sizeof(enum AG_TRAIN_MODE));
		fmodel.write((char*) &ti.maxTiGN, sizeof(int));
		fmodel.write((char*) &ti.minAlpha, sizeof(double));
		fmodel.close();		
	}

	//build bagged models, calculate sums of predictions
	for(int bagNo = 0; bagNo < ti.bagN; bagNo++)
	{
		cout << "\t\tIteration " << bagNo + 1 << " out of " << ti.bagN << endl;
		CGrove grove(ti.minAlpha, ti.maxTiGN, ti.interaction);
		grove.trainLayered();
		for(int itemNo = 0; itemNo < validN; itemNo++)
			predsumsV[itemNo] += grove.predict(itemNo, VALID);

		if(!modelFName.empty())
			grove.save(modelFName.c_str()); 
	}

	//calculate predictions of the whole ensemble on the validation set
	doublev predictions(validN); 
	for(int itemNo = 0; itemNo < validN; itemNo++)
		predictions[itemNo] = predsumsV[itemNo] / ti.bagN;

	if(ti.rms)
		return rmse(predictions, validTar);
	else
		return roc(predictions, validTar);	
}

//runs Layered Groves repeatN times, returns average performance and standard deviation
//saves the model from the last run
double meanLG(INDdata& data, TrainInfo ti, int repeatN, double& resStd, string modelFName)
{
	doublev resVals(repeatN);
	int repeatNo;
	cout << endl << "Estimating distribution of model performance" << endl;
	for(repeatNo = 0; repeatNo < repeatN; repeatNo++)
	{
		cout << "\tTraining model " << repeatNo + 1 << " out of " << repeatN << endl;		
		if(repeatNo == repeatN - 1)
			resVals[repeatNo] = layeredGroves(data, ti, modelFName); //save the last model
		else
			resVals[repeatNo] = layeredGroves(data, ti, string(""));
	}

	//calculate mean
	double resMean = 0;
	for(repeatNo = 0; repeatNo < repeatN; repeatNo++)
		resMean += resVals[repeatNo];
	resMean /= repeatN;

	//calculate standard deviation
	resStd = 0;
	for(repeatNo = 0; repeatNo < repeatN; repeatNo++)
		resStd += (resMean - resVals[repeatNo])*(resMean - resVals[repeatNo]);
	resStd /= repeatN;
	resStd = sqrt(resStd);

	return resMean;
}

//implementation for erase for reverse iterator
void rerase(intv& vec, intv::reverse_iterator& reverse)
{
	intv::iterator straight = vec.begin() + ((&*reverse) - (&*vec.begin()));
	reverse++;
	vec.erase(straight);
}

//calculate and output separate effects of several attributes in a model
void outEffects(INDdata& data, intv attrIds, int quantN, string modelFName, string outFName /*valid only for 1 attribute*/)
{
	int outAttrN = (int)attrIds.size();
	intv quantNs(outAttrN, quantN);

	//1. Create data (create pseudo test points, add them to the data, save their ids)

	dipairvv valCounts(outAttrN);
	intv uValsNs(outAttrN, -1);
	intvv fakePointsIds(outAttrN);
	for(int attrNo = 0; attrNo < outAttrN; attrNo++)
	{
		uValsNs[attrNo] = data.getQuantiles(attrIds[attrNo], quantNs[attrNo], valCounts[attrNo]);
		fakePointsIds[attrNo].resize(uValsNs[attrNo], -1);
		for(int uValNo = 0; uValNo < uValsNs[attrNo]; uValNo++)
		{
			idpairv fakePoint(1);
			fakePoint[0] = idpair(attrIds[attrNo], valCounts[attrNo][uValNo].first);
			fakePointsIds[attrNo][uValNo] = data.addTestItem(fakePoint);
		}
	}

	//2. Open model file, read its header
	fstream fmodel(modelFName.c_str(), ios_base::binary | ios_base::in);
	TrainInfo ti;
	fmodel.read((char*) &ti.mode, sizeof(enum AG_TRAIN_MODE));
	if(ti.mode == FAST)
	{//skip information about fast training - it is not used in this command
		int dirN = 0;
		fmodel.read((char*) &dirN, sizeof(int));
		bool dirStub = false;
		for(int dirNo = 0; dirNo < dirN; dirNo++)
			fmodel.read((char*) &dirStub, sizeof(bool));
	}
	fmodel.read((char*) &ti.maxTiGN, sizeof(int));
	fmodel.read((char*) &ti.minAlpha, sizeof(double));
	if(fmodel.fail() || (ti.maxTiGN < 1))
		throw MODEL_ERR;

	//3. Read models, calculate predictions for fake points

	doublevv pdfVals(outAttrN);
	for(int attrNo = 0; attrNo < outAttrN; attrNo++)
		pdfVals[attrNo].resize(uValsNs[attrNo], 0); //partial dependence function values

	ti.bagN = 0;
	while(fmodel.peek() != char_traits<char>::eof())
	{//load next Grove in the ensemble 
		ti.bagN++;
		CGrove grove(ti.minAlpha, ti.maxTiGN);
		grove.load(fmodel);

		//calculate partial dependence function values (predictions on quantile points)
		for(int attrNo = 0; attrNo < outAttrN; attrNo++)
			for(int uValNo = 0; uValNo < uValsNs[attrNo]; uValNo++)
				pdfVals[attrNo][uValNo] += grove.predict(fakePointsIds[attrNo][uValNo], TEST);
	}
	//normalize bagged predictions of the ensemble
	for(int attrNo = 0; attrNo < outAttrN; attrNo++)
		for(int uValNo = 0; uValNo < uValsNs[attrNo]; uValNo++)
			pdfVals[attrNo][uValNo] /= ti.bagN;

	//4. Output
	for(int attrNo = 0; attrNo < outAttrN; attrNo++)
	{
		if((outFName.size() == 0) || (attrNo > 0))
			outFName = data.getAttrName(attrIds[attrNo]) + ".effect.txt";
		fstream fpdf(outFName.c_str(), ios_base::out);
		fpdf << "counts\t" << data.getAttrName(attrIds[attrNo]) << "_values\taverage_effect_values" << "\n\n";

		for(int uValNo = 0; uValNo < uValsNs[attrNo]; uValNo++)
			fpdf << valCounts[attrNo][uValNo].second << "\t" << valCounts[attrNo][uValNo].first << "\t" 
			<< pdfVals[attrNo][uValNo] << endl;
		fpdf.close();
	}
}

//calculate and output joint effects for pairs of attributes in a model
//the procedure is done for several effects jointly to save on model reading time
void outIPlots(INDdata& data, iipairv interactions, int quantN1, int quantN2, string modelFName, 
			   string outFName, string fixedFName 
			   /*last two parameters are valid only for a list consisting of a single interaction*/)
{
	int iN = (int)interactions.size();
	intv quantNs1(iN, quantN1);
	intv quantNs2(iN, quantN2);

//1. Create data 
	//1.a Extract fixed attributes information
	idpairv fixedPoints;
	if(fixedFName.size() != 0)
	{
		fstream ffixed;
		ffixed.open(fixedFName.c_str(), ios_base::in);
		string fixedAttr;
		double fixedVal;
		ffixed >> fixedAttr >> fixedVal;
		while(!ffixed.fail())
		{
			int fixedAttrId = data.getAttrId(fixedAttr);
			if(!data.isActive(fixedAttrId))
				throw ATTR_NAME_ERR;
			fixedPoints.push_back(idpair(fixedAttrId, fixedVal));
			ffixed >> fixedAttr >> fixedVal;
		}	
	}

	//1.b Create additional data points
	dipairvv valCounts1(iN), valCounts2(iN);
	intv uValsNs1(iN, -1), uValsNs2(iN, -1);
	intvvv quantPointIds(iN);
	for(int iNo = 0; iNo < iN; iNo++)
	{
		uValsNs1[iNo] = data.getQuantiles(interactions[iNo].first, quantNs1[iNo], valCounts1[iNo]);
		uValsNs2[iNo] = data.getQuantiles(interactions[iNo].second, quantNs2[iNo], valCounts2[iNo]);
		quantPointIds[iNo].resize(uValsNs1[iNo], intv(uValsNs2[iNo], -1));
		//create pseudo test points, add them to the data, save their ids
		for(int uValNo1 = 0; uValNo1 < uValsNs1[iNo]; uValNo1++)
			for(int uValNo2 = 0; uValNo2 < uValsNs2[iNo]; uValNo2++)
			{
				idpairv quantPoints = fixedPoints;
				quantPoints.push_back(idpair(interactions[iNo].first, valCounts1[iNo][uValNo1].first));
				quantPoints.push_back(idpair(interactions[iNo].second, valCounts2[iNo][uValNo2].first));
				quantPointIds[iNo][uValNo1][uValNo2] = data.addTestItem(quantPoints);
			}
	}

//2. Open model file, read its header
	fstream fmodel(modelFName.c_str(), ios_base::binary | ios_base::in);
	TrainInfo ti;
	fmodel.read((char*) &ti.mode, sizeof(enum AG_TRAIN_MODE));
	if(ti.mode == FAST)
	{//skip information about fast training - it is not used in this command
		int dirN = 0;
		fmodel.read((char*) &dirN, sizeof(int));
		bool dirStub = false;
		for(int dirNo = 0; dirNo < dirN; dirNo++)
			fmodel.read((char*) &dirStub, sizeof(bool));
	}
	fmodel.read((char*) &ti.maxTiGN, sizeof(int));
	fmodel.read((char*) &ti.minAlpha, sizeof(double));
	if(fmodel.fail() || (ti.maxTiGN < 1))
		throw MODEL_ERR;


//3. Read models, calculate predictions for quantile points

	doublevvv pdfVals(iN); //partial dependence function values
	for(int iNo = 0; iNo < iN; iNo++)
		pdfVals[iNo].resize(uValsNs1[iNo], doublev(uValsNs2[iNo], 0)); 

	ti.bagN = 0;
	while(fmodel.peek() != char_traits<char>::eof())
	{//load next Grove in the ensemble 
		ti.bagN++;
		CGrove grove(ti.minAlpha, ti.maxTiGN);
		grove.load(fmodel);

		//calculate partial dependence function values (predictions on quantile points)
		for(int iNo = 0; iNo < iN; iNo++)
			for(int uValNo1 = 0; uValNo1 < uValsNs1[iNo]; uValNo1++)
				for(int uValNo2 = 0; uValNo2 < uValsNs2[iNo]; uValNo2++)
					pdfVals[iNo][uValNo1][uValNo2] 
						+= grove.predict(quantPointIds[iNo][uValNo1][uValNo2], TEST);
	}
	//normalize bagged predictions of the ensemble
	for(int iNo = 0; iNo < iN; iNo++)
		for(int uValNo1 = 0; uValNo1 < uValsNs1[iNo]; uValNo1++)
			for(int uValNo2 = 0; uValNo2 < uValsNs2[iNo]; uValNo2++)
				pdfVals[iNo][uValNo1][uValNo2] /= ti.bagN;

//4. Output 
	for(int iNo = 0; iNo < iN; iNo++)
	{
		string attrName1 = data.getAttrName(interactions[iNo].first);
		string attrName2 = data.getAttrName(interactions[iNo].second);

		if((outFName.size() == 0) || (iNo > 0))
			outFName = attrName1 + "." + attrName2 + ".iplot.txt";
		fstream fiplot(outFName.c_str(), ios_base::out);


	//4.1 Output joint effect
		fiplot << "Joint effect table\nrows: \t" << attrName1 << "\ncolumns: \t" << attrName2 
			<< "\nFirst row/column - quantile counts. Second row/column - quantile centers. " 
			<< "Ignore four zeros in upper left corner.\n\n";

		//counts of feature 2
		fiplot << "0\t0";	
		for(int uValNo2 = 0; uValNo2 < uValsNs2[iNo]; uValNo2++)
			fiplot << "\t" << valCounts2[iNo][uValNo2].second;
		//quantile values of feature 2
		fiplot << "\n0\t0";	
		for(int uValNo2 = 0; uValNo2 < uValsNs2[iNo]; uValNo2++)
			fiplot << "\t" << valCounts2[iNo][uValNo2].first;

		for(int uValNo1 = 0; uValNo1 < uValsNs1[iNo]; uValNo1++)
		{//feature 1 count value followed by quantile value followed by pdf values
			fiplot << endl << valCounts1[iNo][uValNo1].second << "\t" << valCounts1[iNo][uValNo1].first;
			for(int uValNo2 = 0; uValNo2 < uValsNs2[iNo]; uValNo2++)
				fiplot << "\t" << pdfVals[iNo][uValNo1][uValNo2];
		}
		fiplot << endl;
		fiplot.close();
	
	//4.2 Estimate and output density
		doublev attrVals1, attrVals2; //all values of given attributes in the train set; sorted, w duplicates
		data.getValues(interactions[iNo].first, attrVals1); 
		data.getValues(interactions[iNo].second, attrVals2); 
		int valsN1 = (int)attrVals1.size();
		int valsN2 = (int)attrVals2.size();

		//s(mall) quantile values - values from attrVals on equal distances for quantiles of twice smaller size
		int squantN1 = quantNs1[iNo] * 2 + 1;
		int squantN2 = quantNs2[iNo] * 2 + 1;
		doublev sqVals1(squantN1, QNAN), sqVals2(squantN2, QNAN); 
		for(int squantNo = 0; squantNo < squantN1; squantNo++)
			sqVals1[squantNo] = attrVals1[ (valsN1 - 1) * (squantNo + 1) / (squantN1 + 1) ];
		for(int squantNo = 0; squantNo < squantN2; squantNo++)
			sqVals2[squantNo] = attrVals2[ (valsN2 - 1) * (squantNo + 1) / (squantN2 + 1) ];

		//borders and centers of density blocks
		doublevv triples1, triples2;
		int triN1 = 0;
		int triN2 = 0;
		for(int quantNo = 0; quantNo < quantNs1[iNo]; quantNo++)
		{
			if((quantNo == 0) || (triples1[triN1 - 1][1] != sqVals1[quantNo * 2 + 1]))
			{
				doublev triplet(3, QNAN);
				triplet[0] = sqVals1[quantNo * 2];
				triplet[1] = sqVals1[quantNo * 2 + 1];
				triplet[2] = sqVals1[quantNo * 2 + 2];
				triples1.push_back(triplet);
				triN1++;
			}
			else
				triples1[triN1 - 1][2] = sqVals1[quantNo * 2 + 2];
		}
		for(int quantNo = 0; quantNo < quantNs2[iNo]; quantNo++)
		{
			if((quantNo == 0) || (triples2[triN2 - 1][1] != sqVals2[quantNo * 2 + 1]))
			{
				doublev triplet(3, QNAN);
				triplet[0] = sqVals2[quantNo * 2];
				triplet[1] = sqVals2[quantNo * 2 + 1];
				triplet[2] = sqVals2[quantNo * 2 + 2];
				triples2.push_back(triplet);
				triN2++;
			}
			else
				triples2[triN2 - 1][2] = sqVals2[quantNo * 2 + 2];
		}

		//set flags showing whether border values belong to the block
		bbpairv borders1(triN1, bbpair(true, false)), borders2(triN2, bbpair(true, false));
		borders1[triN1 - 1].second = true;
		borders2[triN2 - 1].second = true;
		for(int triNo1 = 0; triNo1 < triN1 - 1; triNo1++)
			if(triples1[triNo1][1] == triples1[triNo1][2])
			{
				borders1[triNo1].second = true;
				borders1[triNo1 + 1].first = false;
			}
		for(int triNo2 = 0; triNo2 < triN2 - 1; triNo2++)
			if(triples2[triNo2][1] == triples2[triNo2][2])
			{
				borders2[triNo2].second = true;
				borders2[triNo2 + 1].first = false;
			}

		
		//get counts for cells bound by quantile borders
		ddpairv attrVals12;
		data.getValues(interactions[iNo].first, interactions[iNo].second, attrVals12);

		doublevv counts(triN1, doublev(triN2, 0));
		int itemN = (int)attrVals12.size();
		for(int itemNo = 0; itemNo < itemN; itemNo++)
		{
			double& value1 = attrVals12[itemNo].first;
			double& value2 = attrVals12[itemNo].second;
			if((value1 < triples1[0][0]) || (value2 < triples2[0][0]) 
				|| (value1 > triples1[triN1 - 1][2]) || (value2 > triples2[triN2 - 1][2]))
				continue;//first and last half-quantiles should be ignored

			int blockNo1, blockNo2;
			for(blockNo1 = 0; blockNo1 < triN1; blockNo1++)
				if(((value1 > triples1[blockNo1][0]) || borders1[blockNo1].first && (value1 == triples1[blockNo1][0])) 
					&& ((value1 < triples1[blockNo1][2]) || borders1[blockNo1].second && (value1 == triples1[blockNo1][2]))) 
					break;
			for(blockNo2 = 0; blockNo2 < triN2; blockNo2++)
				if(((value2 > triples2[blockNo2][0]) || borders2[blockNo2].first && (value2 == triples2[blockNo2][0])) 
					&& ((value2 < triples2[blockNo2][2]) || borders2[blockNo2].second && (value2 == triples2[blockNo2][2])))
					break;

			counts[blockNo1][blockNo2]++;
		}
		for(int blockNo1 = 0; blockNo1 < triN1; blockNo1++)
			for(int blockNo2 = 0; blockNo2 < triN2; blockNo2++)
				counts[blockNo1][blockNo2] /= itemN;

		//output
		string denFName = insertSuffix(outFName, "dens");
		fstream fdens(denFName.c_str(), ios_base::out);
		fdens << "Density table: proportion of data around each quantile point " 
			<< "(some data on edges is ignored)\nrows: \t" << attrName1 << "\ncolumns: \t" << attrName2 
			<< "\nFirst row/column - quantile borders. Ignore zero in upper left corner.\n\n";
		

		//x border values
		fdens << "0";
		for(int blockNo2 = 0; blockNo2 < triN2; blockNo2++)
			fdens << "\t" << triples2[blockNo2][0];
		fdens << "\t" << triples2[triN2 - 1][2];
		
		for(int blockNo1 = 0; blockNo1 < triN1; blockNo1++)
		{//y border value followed by counts values
			fdens << endl << triples1[blockNo1][0];
			for(int blockNo2 = 0; blockNo2 < triN2; blockNo2++)
				fdens << "\t" << counts[blockNo1][blockNo2];
		}
		fdens << endl << triples1[triN1 - 1][2] << endl;

		fdens.close();
	}
}
