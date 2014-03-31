// functions.h: declarations of global functions
//
// (c) Daria Sorokina

#pragma once
#include "definitions.h"

//deletes spaces from the beginning and from the end of the string
string trimSpace(string& str);

//calculates root mean squared error
double rmse(doublev& predicts, doublev& realvals);

//removes an element from a vector
int erasev(intv* pVec, int value);

//erase first occurence of item from vector, return its number in variable no
intv::iterator erasev(intv* pVec, int item, int& no);

//returns a place of the first significant digit after zero
int sDigit(double number);

//rounds up alpha to the closest appropriate value
double adjustAlpha(double alpha, int trainN);

//converts valid values of alpha to string
string alphaToStr(double alpha);

//checks if more bagging will benefit the performance
bool moreBag(doublev bagPerf);

//extends fstream::getline with check on exceeding the buffer size
std::streamsize getLineExt(fstream& fin, char* buf);

//outputs error messages for shared TreeExtra errors
void te_errMsg(TE_ERROR err);

//calculates probabilistic roc - response can be any probability between 0 and 1
//when response values are 0/1, behaves like a standard roc
double roc(doublev& preds, doublev& tars);

//inserts suffix into a string (usually file name)
string insertSuffix(string fileName, string suffix);

//checks if the first set is a subset of the second set
bool isSubset(intset& set1, intset& set2);

//converts string to int, throws error if the string is unconvertable
int atoiExt(char* str);

//converts string to double, throws error if the string is unconvertable
double atofExt(char* str);

//returns difference, unless it is due to the rounding error
double diff10d(double d1, double d2);

//returns random double between 0 and 1
double rand_coef();