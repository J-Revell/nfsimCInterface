#include <cstdlib>

#include "nfsim_c.h"
#include "nfsim_c_structs.h"
#include <NFapi.hh>
#include <iostream>
#include <fstream>

typedef std::map<std::string, std::string> Map;
typedef std::vector<Map*> MapVector;
typedef std::map<std::string, MapVector*> MapVectorMap;


map<string,int> preInitMap;
vector<map<string, double>> observableLog;
map<string, int> currentReactions;

vector<double> observableTimes;

map<map<string,int>, map<string, double>> preInitMapCollection;

static std::string inputFile;


#ifdef __cplusplus
extern "C" {
#endif


// Inside this "extern C" block, I can define C functions that are able to call C++ code
int setupNFSim_c(const char* filename, int verbose) {
    //nfapi returns bool
    inputFile = std::string(filename);
    if(NFapi::setupNFSim(filename, verbose))
        return 0;
    return -1;
}

int resetSystem_c(){
    if(NFapi::resetSystem())
        return 0;
    return -1;
}

int deleteNFSimSystem_c(){
    NFapi::deleteSystem();
}

int initSystemXML_c(const char * initXML){
    if(NFapi::initSystemXML(std::string(initXML)))
        return 0;
    return -1;
}


int initSystemNauty_c(const char** nautyString, const int* seedNumber, int numberOfElements){
    map<std::string, int> initMap;
    for(int i=0; i< numberOfElements; i++){
        initMap[std::string(nautyString[i])] = seedNumber[i];
    }
    if(NFapi::initSystemNauty(initMap))
        return 0;
    return -1;
}

int initFromConstruct_c(){
    if(NFapi::initSystemNauty(preInitMap))
        return 0;
    return -1;
        
}


int constructNauty_c(const char* nautyString, const int seedNumber){
    if(preInitMap.find(nautyString) == preInitMap.end()){
        preInitMap[nautyString] = 0;
    }

    preInitMap[nautyString] = preInitMap[nautyString] + seedNumber;
    preInitMap[nautyString] = max(preInitMap[nautyString], 0);
    return 0;
}

 
int logNFSimObservables_c(double timePoint){

    //memoization
    map<string, double> currentObservables;

    if(preInitMapCollection.find(preInitMap) != preInitMapCollection.end()){
        currentObservables = preInitMapCollection[preInitMap];
    }
    else{
        //init system and query observables
        resetSystem_c();
        initFromConstruct_c();
        //query system
        NFapi::queryObservables(currentObservables);
        //store for later queries
        preInitMapCollection[preInitMap] = currentObservables;
    }


    observableLog.push_back(currentObservables);
    observableTimes.push_back(timePoint);
    //reactionLog.push_back(currentReactions);

}

int logNFSimReactions_c(const char* reactionName){
    currentReactions[std::string(reactionName)] += 1;
}



int outputNFSimObservables_c(){
    const size_t last_slash_idx = inputFile.find_last_of("\\/");
    if (std::string::npos != last_slash_idx)
    {
        inputFile.erase(0, last_slash_idx + 1);
    }
    // Remove extension if present.
    outputNFSimObservablesF_c((inputFile + ".gdat").c_str());
    outputNFSimReactionsF_c((inputFile + "_reactions.gdat").c_str());

}


int outputNFSimObservablesF_c(const char* outputfilename){
    ofstream gdatFile;

    gdatFile.open(outputfilename);
    vector<string> keys;
    gdatFile << "time, ";
    for(auto it: observableLog[0]){
        gdatFile << it.first << ", ";
        keys.push_back(it.first);
    }
    gdatFile <<"\n";
    for(int i=0; i < observableLog.size(); i++){
        auto line = observableLog[i];
        gdatFile << observableTimes[i] << ", ";
        for(auto key: keys){
            gdatFile << line[key] << ", ";
        }
        gdatFile <<"\n";
    }

    keys.clear();

}

int outputNFSimReactionsF_c(const char* outputfilename){
    ofstream gdatFile;

    gdatFile.open(outputfilename);
    vector<string> keys;
    for(auto it: currentReactions){
        gdatFile << it.first << " fired " << it.second <<"\n";
    }

    keys.clear();

}


void querySystemStatus_c(const char* option, void* results){
    
    MapVector* tmpResults = reinterpret_cast<MapVector*>(results);
    //std::vector<map<string, string>> tmpResults;
    NFapi::querySystemStatus(std::string(option), *tmpResults);

    /*queryResults query;
    //query.results = map_create();
    query.results = (void**) malloc(tmpResults.size() * sizeof(Map*));
    //query.results = (char**) malloc(tmpResults.size() * sizeof(char *));
    query.numOfResults = tmpResults.size();
    int index = 0;
    for(auto it: tmpResults){
        query.results[index] = reinterpret_cast<void*>(&it);
        //memcpy(query.results[index].label, it["label"].c_str(), it["label"].size() + 1);
        //index++;
    }*/


    //return query;
}



reactantQueryResults map2ReactantQueryResults(const std::map<std::string, vector<map<string,string>*>*> &queryResults){

    reactantQueryResults finalResults;
    finalResults.numOfResults = queryResults.size();
    finalResults.keys = (char**) malloc(queryResults.size() * sizeof(char *));
    finalResults.numOfAssociatedReactions = (int*) malloc(queryResults.size() * sizeof(int));
    finalResults.associatedReactions = (reactionResult*) malloc(queryResults.size() * sizeof(reactionResult));

    int idx = 0;
    int idx2 = 0;
    for(auto it: queryResults){
        finalResults.keys[idx] = (char *) malloc(it.first.size() + 1);
        memcpy(finalResults.keys[idx], it.first.c_str(), it.first.size() + 1);
        finalResults.numOfAssociatedReactions[idx] = it.second->size();
        finalResults.associatedReactions[idx].reactionNames = (char**) malloc(it.second->size() * sizeof(char*));
        finalResults.associatedReactions[idx].rates = (double*) malloc(it.second->size() * sizeof(double));
        idx2 = 0;


        for(auto rxn: *(it.second)){
            finalResults.associatedReactions[idx].reactionNames[idx2] = (char*) malloc(rxn->find("name")->second.size() + 1);
            memcpy(finalResults.associatedReactions[idx].reactionNames[idx2], rxn->find("name")->second.c_str(), rxn->find("name")->second.size() + 1);

            finalResults.associatedReactions[idx].rates[idx2] = std::stod(rxn->find("rate")->second);

            //finalResults.associatedReactions[idx].numReactants[idx2] = std::stoul(rxn["numReactants"],0,10);
            //finalResults.associatedReactions[idx].numProducts[idx2] = std::stoul(rxn["numProducts"],0,10);
            ++idx2;
        }
        ++idx;
    }
    return finalResults;    
}

int delete_reactantQueryResults(reactantQueryResults finalResults){
    for(int i =0; i<finalResults.numOfResults;i++){
        free(finalResults.keys[i]);
        for(int j=0;j<finalResults.numOfAssociatedReactions[i];j++){
            free(finalResults.associatedReactions[i].reactionNames[j]);    
        }
        free(finalResults.associatedReactions[i].reactionNames);
        free(finalResults.associatedReactions[i].rates);

    }
    free(finalResults.keys);
    free(finalResults.numOfAssociatedReactions);
    free(finalResults.associatedReactions);

    return 0;
}


const char* extractSpeciesCompartmentFromNauty_c(const char* nauty){
    string compartment = NFapi::extractSpeciesCompartmentFromNauty(string(nauty));


    char* result = strdup(compartment.c_str());
    return result;
}


int delete_compartmentStructs(compartmentStruct compartment){
    free(compartment.name);
    free(compartment.outside);
}


void queryByNumReactant_c(const int numReactants, void* results){
    //std::map<std::string, vector<map<string,string>*>*> queryResults;
    MapVectorMap* queryResults = reinterpret_cast<MapVectorMap*>(results);
    NFapi::queryByNumReactant(*queryResults, numReactants);

    //reactantQueryResults finalResults = map2ReactantQueryResults(queryResults);

    //cleanup
    //for(auto it: queryResults){
    //    it.second->clear();
    //}
    //queryResults.clear();
}

observableResults queryObservables_c(){
    std::map<std::string, double> observables;
    observableResults results;
    results.observableNames = (char**) malloc(observables.size() * sizeof(char *));
    results.observableValues = (double*) malloc(observables.size() * sizeof(double));

    NFapi::queryObservables(observables);
    int idx = 0;
    for (auto it: observables){
        results.observableNames[idx] = (char *) malloc(it.first.size() + 1);
        memcpy(results.observableNames[idx], it.first.c_str(), it.first.size() + 1);
        results.observableValues[idx] = it.second;
        ++idx;
    }

    results.numResults = observables.size();
    return results;
}


void initAndQuerySystemStatus_c(const queryOptions options_c, void* results){
    NFapi::numReactantQueryIndex options;
    for(int i=0;i < options_c.numOfInitElements; i++)
    {
        if(options.initMap.find(options_c.initKeys[i]) == options.initMap.end())
            options.initMap[options_c.initKeys[i]] = 0;

        options.initMap[options_c.initKeys[i]] += options_c.initValues[i];
    }

    for(int i=0; i< options_c.numOfOptions; i++)
    {
        options.options[options_c.optionKeys[i]] = std::string(options_c.optionValues[i]);
        if(std::string(options_c.optionKeys[i]) == "reaction"){
            if(currentReactions.find(std::string(options_c.optionValues[i])) == currentReactions.end()){
                currentReactions[std::string(options_c.optionValues[i])] = 0;
            }
        }

    }


    MapVector* tmpResults = reinterpret_cast<MapVector*>(results);
    NFapi::initAndQuerySystemStatus(options, *tmpResults);


    options.initMap.clear();
    options.options.clear();

}


void initAndQueryByNumReactant_c(const queryOptions options_c, void* results){
    NFapi::numReactantQueryIndex options;
    for(int i=0;i < options_c.numOfInitElements; i++)
    {
        if(options.initMap.find(options_c.initKeys[i]) == options.initMap.end())
            options.initMap[options_c.initKeys[i]] = 0;

        options.initMap[options_c.initKeys[i]] += options_c.initValues[i];
    }

    for(int i=0; i< options_c.numOfOptions; i++)
    {
        options.options[options_c.optionKeys[i]] = options_c.optionValues[i];
    }
    //std::map<std::string, vector<map<string,string>*>*> queryResults;
    MapVectorMap* queryResults = reinterpret_cast<MapVectorMap*>(results);
    NFapi::initAndQueryByNumReactant(options, *queryResults);

    //translate results to a C friendly form
    //reactantQueryResults finalResults = map2ReactantQueryResults(queryResults);
    //cleanup
    //for(auto it: queryResults){
    //    it.second->clear();
    //}

    //queryResults.clear();
    options.initMap.clear();
    options.options.clear();

}


int stepSimulation_c(){
    if(NFapi::stepSimulation())
        return 0;
    return 1;

}

int stepSimulationRxn_c(const char* rxn){
    if(NFapi::stepSimulation(string(rxn))){
        return 0;
    }
    return 1;
}

compartmentStruct getCompartmentInformation_c(const char* name){
    string nameStr(name);
    shared_ptr<Compartment> tmp = NFapi::getCompartmentInformation(nameStr);
    compartmentStruct result;

    result.name = (char *)malloc(tmp->getName().size()+1);
    memcpy(result.name, tmp->getName().c_str(), tmp->getName().size() + 1);

    result.spatialDimensions = tmp->getSpatialDimensions();
    result.size = tmp->getSize();

    result.outside = (char *)malloc(tmp->getOutside().size() + 1);
    memcpy(result.outside, tmp->getOutside().c_str(), tmp->getOutside().size() + 1);

    return result;
}

void freeCompartmentInformation_c(compartmentStruct* compartment){
    free(compartment->name);
    free(compartment->outside);
}



#ifdef __cplusplus
}
#endif