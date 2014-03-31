// INDdata.h: interface for the INDdata class
//
// (c) Daria Sorokina

#pragma once
#include "ItemInfo.h"

class INDdata
{
public:
	//loads data into memory
	INDdata(const char* trainFName, const char* valFName, const char* testFName, 
			const char* attrFName);
	
//private members get functions  
	int getAttrN(){return attrN;}	
	int getTrainN(){return trainN;}
	int getTarColNo(){return tarColNo;}
	int getTargets(doublev& targets, DATA_SET dset);
	int getOutOfBag(intv& oobData_out, doublev& oobTar_out);
	bool getHasMV(){return hasMV;}

//untrivial get functions

	//gets attrID by its name
	int getAttrId(string attrName); 

	//gets column number of the attribute in the data file
	int getColNo(int attrId);

	//gets a list of active attributes
	void getActiveAttrs(intv& attrs);

	//gets current bag of training data 
	void getCurBag(ItemInfov& itemSet);
	int getCurBag(intv& bagData, doublev& bagTar);

	//gets sorted indexes of current training data
	void getSortedData(fipairvv& sorted);

	//gets a value of a given attribute for a given case in a given data set
	double getValue(int itemNo, int attrId, DATA_SET dset);

	//returns the name of the attribute by its number
	string getAttrName(int attrId);

	//returns counts and quantile values
	int getQuantiles(int attrId, int& quantN, dipairv& valCounts);

//"question" functions

	//checks if attribute is boolean
	bool boolAttr(int attrId);

	//checks if all target values in test set are valid
	bool hasTrueTest();

	//checks if the attr number is valid and active
	bool isActive(int attrId);

//action functions

	//deactivates the attribute
	void ignoreAttr(int attrId); 

	//actuvates the attribute
	void useAttr(int attrId);

	//replaces bootstrap in the bag
	void newBag(void);

	//subsampling without replacement
	void newSample(int sampleN);

	//inserts a new data point into the data set
	int addTestItem(idpairv& values); 

	//outputs a version of attribute file where only a predefined set of features is active
	void outAttr(string attrFName);

	//gets all values of two specific attribute in the validation data
	void getValues(int attr1No, int attr2No, ddpairv& values);

	//gets all values of a specific attribute in the validation set
	void getValues(int attrId, doublev& values);

private:
	//gets a line of text, returns a vector with data points
	void readData(char* buf, streamsize buflen, floatv& retv, int retvlen); 

	//create versions of bootstrap data sorted by active continuous attributes 
	void sortItems(); 

	//returns mean and std of response
	double getTarStD(DATA_SET ds);



private:
	int attrN;			//number of attributes
	intset boolAttrs;	//boolean attributes
	intset nomAttrs;	//nominal attributes
	intset ignoreAttrs; //attributes that should be ignored
	stringv attrNames;	//names of attributes
	int tarColNo;		//column number of the response attribute 

	int trainN;			//number of data points in the train set
	floatvv train;		//train set data w/o response
	doublev trainTar;	//train set response

	int validN;			//number of data points in the validation set
	floatvv valid;		//validation set data w/o response
	doublev validTar;	//validation set response

	int testN;			//number of data points in the test set
	floatvv test;		//test set data w/o response
	doublev testTar;		//test set response

	intv bootstrap;		//indexes of data points currently in the bag, can be repeating
	int oobN;			//number of out-of-bag data points
	intv oobData;		//indexes of out-of-bag data points
	doublev oobTar;		//targests for out-of-bag data points

	fipairvv sortedItems; //several copies of sorted data points in the bag
							//separate vector for sorting by each attribute
							//each data point represented as (id, attrvalue) pair

	bool hasMV;			//data has missing values

};
