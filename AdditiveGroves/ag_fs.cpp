//Additive Groves / ag_fs.cpp: main function of executable ag_fs
//
//(c) Daria Sorokina

//ag_fs -t _train_set_ -v _validation_set_ -r _attr_file_ -a _alpha_value_ -n _N_value_ 
//		-b _bagging_iterations_ [-c rms|roc] [-i seed] [-m _model_file_name_] 

#include "ag_definitions.h"
#include "functions.h"
#include "ag_functions.h"
#include "Grove.h"
#include "LogStream.h"
#include "ErrLogStream.h"
#include <errno.h>

#ifndef _WIN32
#include "thread_pool.h"
#endif

int main(int argc, char* argv[])
{	
	try{
//0. Set log file
	LogStream clog;
	clog << "\n-----\nag_fs ";
	for(int argNo = 1; argNo < argc; argNo++)
		clog << argv[argNo] << " ";
	clog << "\n\n";

//1. Analyze input parameters
	//convert input parameters to string from char*
	stringv args(argc); 
	for(int argNo = 0; argNo < argc; argNo++)
		args[argNo] = string(argv[argNo]);
	
	//check that the number of arguments is even (flags + value pairs)
	if(argc % 2 == 0)
		throw INPUT_ERR;

	TrainInfo ti;
	string modelFName = "model.bin";	//name of the output file for the model
	ti.mode = LAYERED;

#ifndef _WIN32
	int threadN = 6;	//number of threads
#endif

	//parse and save input parameters
	//indicators of presence of required flags in the input
	bool hasTrain = false;
	bool hasVal = false; 
	bool hasAttr = false; 
	bool hasAlpha = false;
	bool hasTiGN = false;
	bool hasBagN = false;

	for(int argNo = 1; argNo < argc; argNo += 2)
	{
		if(!args[argNo].compare("-t"))
		{
			ti.trainFName = args[argNo + 1];
			hasTrain = true;
		}
		else if(!args[argNo].compare("-v"))
		{
			ti.validFName = args[argNo + 1];
			hasVal = true;
		}
		else if(!args[argNo].compare("-r"))
		{
			ti.attrFName = args[argNo + 1];
			hasAttr = true;
		}
		else if(!args[argNo].compare("-a"))
		{
			ti.minAlpha = atofExt(argv[argNo + 1]);
			hasAlpha = true;
		}
		else if(!args[argNo].compare("-n"))
		{
			ti.maxTiGN = atoiExt(argv[argNo + 1]);
			hasTiGN = true;
		}
		else if(!args[argNo].compare("-b"))
		{
			ti.bagN = atoiExt(argv[argNo + 1]);
			hasBagN = true;
		}
		else if(!args[argNo].compare("-c"))
		{
			if(!args[argNo + 1].compare("roc"))
				ti.rms = false;
			else if(!args[argNo + 1].compare("rms"))
				ti.rms = true;
			else
				throw INPUT_ERR;
		}
		else if(!args[argNo].compare("-i"))
			ti.seed = atoiExt(argv[argNo + 1]);
		else if(!args[argNo].compare("-m"))
		{
			modelFName = args[argNo + 1];
			if(modelFName.empty())
				throw EMPTY_MODEL_NAME_ERR;
		}
		else if(!args[argNo].compare("-h"))
#ifndef _WIN32 
			threadN = atoiExt(argv[argNo + 1]);
#else
			throw WIN_ERR;
#endif
		else
			throw INPUT_ERR;
	}//end for(int argNo = 1; argNo < argc; argNo += 2) //parse and save input parameters

	if(!(hasTrain && hasVal && hasAttr && hasAlpha && hasTiGN && hasBagN))
		throw INPUT_ERR;

	if(ti.trainFName.compare(ti.validFName) == 0)
		throw TRAIN_EQ_VALID_ERR;

	if((ti.minAlpha < 0) || (ti.minAlpha > 1))
		throw ALPHA_ERR;

	if(ti.maxTiGN < 1)
		throw TIGN_ERR;

//1.b) Initialize random number generator. 
	srand(ti.seed);

//2. Load data
	INDdata data(ti.trainFName.c_str(), ti.validFName.c_str(), ti.testFName.c_str(), ti.attrFName.c_str());
	CGrove::setData(data);
	CTreeNode::setData(data);

	doublev validTar;
	int validN = data.getTargets(validTar, VALID);
	int itemN = data.getTrainN();

	//adjust minAlpha, if needed
	double newAlpha = adjustAlpha(ti.minAlpha, itemN);
	if(ti.minAlpha != newAlpha)
	{
		if(newAlpha == 0)
			clog << "Warning: due to small train set size value of alpha was changed to 0"; 
		else 
			clog << "Warning: alpha value was rounded to the closest valid value " << newAlpha;
		clog << ".\n\n";
		ti.minAlpha = newAlpha;	
	}
	//adjust maxTiGN, if needed
	double newTiGN = adjustTiGN(ti.maxTiGN);
	if(ti.maxTiGN != newTiGN)
	{
		clog << "Warning: N value was rounded to the closest smaller valid value " << newTiGN << ".\n\n";
		ti.maxTiGN = newTiGN;	
	}
	clog << "Alpha = " << ti.minAlpha << "\nN = " << ti.maxTiGN << "\n" 
		<< ti.bagN << " bagging iterations\n";

//2.a) Start thread pool
#ifndef _WIN32
	TThreadPool pool(threadN);
	CGrove::setPool(pool);
#endif

//3. main part - feature selection
	intv attrs; //numbers of current active attributes
	data.getActiveAttrs(attrs);
	ifmap importance; // importance values (rmse after removal) of all active attributes
	double mean = -1;
	double std = -1;

	bool removedAny = true;
	bool impValid = false; //shows whether we can reuse current values in impMeas
	while(removedAny)
	{
		removedAny = false;	
		int firstNotRemoved = -1; //first attribute after the last that was removed

		mean = meanLG(data, ti, 10, std, modelFName);
		clog << "\n\nAverage performance: " << mean << ", std: " << std << ", importance threshold: " << std*3 << "\n\n";

		intv::reverse_iterator attrRIt = attrs.rbegin();
		while(attrRIt != attrs.rend())
		{//circle through all active features
			
			if(*attrRIt == firstNotRemoved) //we have run the full cycle without any change; time to stop
			{	
				impValid = true;
				break;
			}

			data.ignoreAttr(*attrRIt);
			double testPerf; 
			if(impValid)
			{
				clog << "Reusing results for " << data.getAttrName(*attrRIt) << ":\n";
				testPerf = importance[*attrRIt];
			}
			else
			{
				clog << "Testing " << data.getAttrName(*attrRIt) << ":\n"; 
				testPerf = layeredGroves(data, ti, string(""));
			}
			if((ti.rms && (testPerf <= mean + std * 3)) || (!ti.rms && (testPerf >= mean - std * 3)))
			{//remove attribute completely
				clog << "\tPerformance: " << testPerf << ".\tImportance: " 
					<< (ti.rms ? (testPerf - mean) : (mean - testPerf)) << ".\tEliminate.\n",

				rerase(attrs, attrRIt);
				firstNotRemoved = -1;
				removedAny = true;
				impValid = false;

				if((ti.rms && (testPerf <= mean - std * 3)) || (!ti.rms && (testPerf >= mean + std * 3)))
				{//current performance is too good; need to reevaluate mean
					clog << "Significant improvement of performance; need to reevaluate distribution\n";
					break;
				}
			} else 
			{//attribute is important, leave it
				clog << "\tPerformance: " << testPerf << ".\tImportance: " 
					<< (ti.rms ? (testPerf - mean) : (mean - testPerf)) << ".\tLeave.\n";

				data.useAttr(*attrRIt);
				importance[*attrRIt] = testPerf;
				if(firstNotRemoved == -1)
					firstNotRemoved = *attrRIt;
				attrRIt++;
			}
	
			if(attrRIt == attrs.rend()) //move to the first element in the circle
				attrRIt = attrs.rbegin();
		}// end while(attrRIt != attrs.rend())
	}// end while(removedAny)

	//output attributes and their partial dependence functions (effect plots)
	clog << "\nResulting set of attributes:\n";
	for(intv::iterator attrIt = attrs.begin(); attrIt != attrs.end(); attrIt++)
		clog << data.getAttrName(*attrIt) << '\n';
	clog << '\n';

	//output new attribute file
	data.outAttr(ti.attrFName);

	//output separate effects of all important attributes
	outEffects(data, attrs, 10, modelFName, "");

	//output mean and std
	fstream fdistr("distribution.txt", ios_base::out);
	fdistr << mean << "\n" << std << "\n";
	fdistr.close();


	}catch(TE_ERROR err){
		te_errMsg((TE_ERROR)err);
		return 1;
	}catch(AG_ERROR err){
		ErrLogStream errlog;
		switch(err)
		{
			case INPUT_ERR:
				errlog << "Usage: ag_fs -t _train_set_ -v _validation_set_ -r _attr_file_ "
					<< "-a _alpha_value_ -n _N_value_ -b _bagging_iterations_ " 
					<< "[-i _init_random_] [-c rms|roc] [-m _model_file_name_] \n";
				break;
			case ALPHA_ERR:
				errlog << "Input error: alpha value is out of [0;1] range.\n";
				break;
			case TIGN_ERR:
				errlog << "Input error: N value is less than 1.\n"; 
				break;
			case TRAIN_EQ_VALID_ERR:
				errlog << "Error: train set should be different from the validation set.\n"; 
				break;
			case WIN_ERR:
				errlog << "Input error: TreeExtra currently does not support multithreading for Windows.\n"; 
				break;
		}
		return 1;
	}catch(exception &e){
		ErrLogStream errlog;
		string errstr(e.what());
		errlog << "Error: " << errstr << "\n";
		return 1;
	}catch(...){
		string errstr = strerror(errno);
		ErrLogStream errlog;
		errlog << "Error: " << errstr << "\n";
		return 1;
	}
	return 0;
}


