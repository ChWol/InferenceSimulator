/*******************************************************************************
* Copyright (c) 2015-2017
* School of Electrical, Computer and Energy Engineering, Arizona State University
* PI: Prof. Shimeng Yu
* All rights reserved.
* 
* This source code is part of NeuroSim - a device-circuit-algorithm framework to benchmark 
* neuro-inspired architectures with synaptic devices(e.g., SRAM and emerging non-volatile memory). 
* Copyright of the model is maintained by the developers, and the model is distributed under 
* the terms of the Creative Commons Attribution-NonCommercial 4.0 International Public License 
* http://creativecommons.org/licenses/by-nc/4.0/legalcode.
* The source code is free and you can redistribute and/or modify it
* by providing that the following conditions are met:
* 
*  1) Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
* 
*  2) Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
* Developer list: 
*   Pai-Yu Chen	    Email: pchen72 at asu dot edu 
*                    
*   Xiaochen Peng   Email: xpeng15 at asu dot edu
********************************************************************************/

#include <cstdio>
#include <random>
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <vector>
#include <sstream>
#include <chrono>
#include <algorithm>
#include "constant.h"
#include "formula.h"
#include "Param.h"
#include "Tile.h"
#include "Chip.h"
#include "ProcessingUnit.h"
#include "SubArray.h"
#include "Definition.h"

using namespace std;

vector<vector<double> > getNetStructure(const string &inputfile);

int main(int argc, char * argv[]) {   

	auto start = chrono::high_resolution_clock::now();
	
	gen.seed(0);
	
	vector<vector<double> > netStructure;
	netStructure = getNetStructure(argv[1]);
	
	// define weight/input/memory precision from wrapper
	param->synapseBit = atoi(argv[2]);              // precision of synapse weight
	param->numBitInput = atoi(argv[3]);             // precision of input neural activation
	param->cellBit = atoi(argv[4]);
	param->technode = atoi(argv[5]);
	switch(param->technode) {
		case 7:	        param->wireWidth = 18; break;
		case 10:	    param->wireWidth = 18; break;
		case 14:	    param->wireWidth = 25; break;
		case 22:	    param->wireWidth = 40; break;
		case 32:	    param->wireWidth = 56; break;
		case 45:	    param->wireWidth = 80; break;
		case 65:	    param->wireWidth = 105; break;
		case 90:	    param->wireWidth = 110; break;
		case 130:	    param->wireWidth = 175; break;
	}
	param->reLu = atoi(argv[6]);
	param->memcelltype = atoi(argv[7]);
	param->levelOutput = atoi(argv[8]);
	param->resistanceOff = 6e3*atoi(argv[9]);

	param->recalculate_Params(param->wireWidth, param->memcelltype, param->resistanceOff);

	if (param->cellBit > param->synapseBit) {
		cout << "ERROR!: Memory precision is even higher than synapse precision, please modify 'cellBit' in Param.cpp!" << endl;
		param->cellBit = param->synapseBit;
	}
	
	/*** initialize operationMode as default ***/
	param->conventionalParallel = 0;
	param->conventionalSequential = 0;
	param->BNNparallelMode = 0;                // parallel BNN
	param->BNNsequentialMode = 0;              // sequential BNN
	param->XNORsequentialMode = 0;           // Use several multi-bit RRAM as one synapse
	param->XNORparallelMode = 0;         // Use several multi-bit RRAM as one synapse
	switch(param->operationmode) {
		case 6:	    param->XNORparallelMode = 1;               break;     
		case 5:	    param->XNORsequentialMode = 1;             break;     
		case 4:	    param->BNNparallelMode = 1;                break;     
		case 3:	    param->BNNsequentialMode = 1;              break;    
		case 2:	    param->conventionalParallel = 1;           break;     
		case 1:	    param->conventionalSequential = 1;         break;    
		case -1:	break;
		default:	exit(-1);
	}
	
	if (param->XNORparallelMode || param->XNORsequentialMode) {
		param->numRowPerSynapse = 2;
	} else {
		param->numRowPerSynapse = 1;
	}
	if (param->BNNparallelMode) {
		param->numColPerSynapse = 2;
	} else if (param->XNORparallelMode || param->XNORsequentialMode || param->BNNsequentialMode) {
		param->numColPerSynapse = 1;
	} else {
		param->numColPerSynapse = ceil((double)param->synapseBit/(double)param->cellBit); 
	}
	
	double maxPESizeNM, maxTileSizeCM, numPENM;
	vector<int> markNM;
	vector<int> pipelineSpeedUp;
	markNM = ChipDesignInitialize(inputParameter, tech, cell, false, netStructure, &maxPESizeNM, &maxTileSizeCM, &numPENM);
	pipelineSpeedUp = ChipDesignInitialize(inputParameter, tech, cell, true, netStructure, &maxPESizeNM, &maxTileSizeCM, &numPENM);
	
	double desiredNumTileNM, desiredPESizeNM, desiredNumTileCM, desiredTileSizeCM, desiredPESizeCM;
	int numTileRow, numTileCol;
	
	vector<vector<double> > numTileEachLayer;
	vector<vector<double> > utilizationEachLayer;
	vector<vector<double> > speedUpEachLayer;
	vector<vector<double> > tileLocaEachLayer;
	
	numTileEachLayer = ChipFloorPlan(true, false, false, netStructure, markNM, 
					maxPESizeNM, maxTileSizeCM, numPENM, pipelineSpeedUp,
					&desiredNumTileNM, &desiredPESizeNM, &desiredNumTileCM, &desiredTileSizeCM, &desiredPESizeCM, &numTileRow, &numTileCol);	
	
	utilizationEachLayer = ChipFloorPlan(false, true, false, netStructure, markNM, 
					maxPESizeNM, maxTileSizeCM, numPENM, pipelineSpeedUp,
					&desiredNumTileNM, &desiredPESizeNM, &desiredNumTileCM, &desiredTileSizeCM, &desiredPESizeCM, &numTileRow, &numTileCol);
	
	speedUpEachLayer = ChipFloorPlan(false, false, true, netStructure, markNM,
					maxPESizeNM, maxTileSizeCM, numPENM, pipelineSpeedUp,
					&desiredNumTileNM, &desiredPESizeNM, &desiredNumTileCM, &desiredTileSizeCM, &desiredPESizeCM, &numTileRow, &numTileCol);
					
	tileLocaEachLayer = ChipFloorPlan(false, false, false, netStructure, markNM,
					maxPESizeNM, maxTileSizeCM, numPENM, pipelineSpeedUp,
					&desiredNumTileNM, &desiredPESizeNM, &desiredNumTileCM, &desiredTileSizeCM, &desiredPESizeCM, &numTileRow, &numTileCol);
	
	cout << "------------------------------ FloorPlan --------------------------------" <<  endl;
	cout << endl;
	cout << "Tile and PE size are optimized to maximize memory utilization ( = memory mapped by synapse / total memory on chip)" << endl;
	cout << endl;
	if (!param->novelMapping) {
		cout << "Desired Conventional Mapped Tile Storage Size: " << desiredTileSizeCM << "x" << desiredTileSizeCM << endl;
		cout << "Desired Conventional PE Storage Size: " << desiredPESizeCM << "x" << desiredPESizeCM << endl;
	} else {
		cout << "Desired Conventional Mapped Tile Storage Size: " << desiredTileSizeCM << "x" << desiredTileSizeCM << endl;
		cout << "Desired Conventional PE Storage Size: " << desiredPESizeCM << "x" << desiredPESizeCM << endl;
		cout << "Desired Novel Mapped Tile Storage Size: " << numPENM << "x" << desiredPESizeNM << "x" << desiredPESizeNM << endl;
	}
	cout << "User-defined SubArray Size: " << param->numRowSubArray << "x" << param->numColSubArray << endl;
	cout << endl;
	cout << "----------------- # of tile used for each layer -----------------" <<  endl;
	double totalNumTile = 0;
	for (int i=0; i<netStructure.size(); i++) {
		cout << "layer" << i+1 << ": " << numTileEachLayer[0][i] * numTileEachLayer[1][i] << endl;
		totalNumTile += numTileEachLayer[0][i] * numTileEachLayer[1][i];
	}
	cout << endl;

	cout << "----------------- Speed-up of each layer ------------------" <<  endl;
	for (int i=0; i<netStructure.size(); i++) {
		cout << "layer" << i+1 << ": " << speedUpEachLayer[0][i] * speedUpEachLayer[1][i] << endl;
	}
	cout << endl;
	
	cout << "----------------- Utilization of each layer ------------------" <<  endl;
	double realMappedMemory = 0;
	for (int i=0; i<netStructure.size(); i++) {
		cout << "layer" << i+1 << ": " << utilizationEachLayer[i][0] << endl;
		realMappedMemory += numTileEachLayer[0][i] * numTileEachLayer[1][i] * utilizationEachLayer[i][0];
	}
	cout << "Memory Utilization of Whole Chip: " << realMappedMemory/totalNumTile*100 << " % " << endl;
	cout << endl;
	cout << "---------------------------- FloorPlan Done ------------------------------" <<  endl;
	cout << endl;
	cout << endl;
	cout << endl;
	
	double numComputation = 0;
	for (int i=0; i<netStructure.size(); i++) {
		numComputation += 2*(netStructure[i][0] * netStructure[i][1] * netStructure[i][2] * netStructure[i][3] * netStructure[i][4] * netStructure[i][5]);
	}

	ChipInitialize(inputParameter, tech, cell, netStructure, markNM, numTileEachLayer,
					numPENM, desiredNumTileNM, desiredPESizeNM, desiredNumTileCM, desiredTileSizeCM, desiredPESizeCM, numTileRow, numTileCol);
			
	double chipHeight, chipWidth, chipArea, chipAreaIC, chipAreaADC, chipAreaAccum, chipAreaOther, chipAreaArray;
	double CMTileheight = 0;
	double CMTilewidth = 0;
	double NMTileheight = 0;
	double NMTilewidth = 0;
	vector<double> chipAreaResults;
		 			
	chipAreaResults = ChipCalculateArea(inputParameter, tech, cell, desiredNumTileNM, numPENM, desiredPESizeNM, desiredNumTileCM, desiredTileSizeCM, desiredPESizeCM, numTileRow, 
					&chipHeight, &chipWidth, &CMTileheight, &CMTilewidth, &NMTileheight, &NMTilewidth);		
	chipArea = chipAreaResults[0];
	chipAreaIC = chipAreaResults[1];
	chipAreaADC = chipAreaResults[2];
	chipAreaAccum = chipAreaResults[3];
	chipAreaOther = chipAreaResults[4];
	chipAreaArray = chipAreaResults[5];

	double clkPeriod = 0;
	double layerclkPeriod = 0;
	
	double chipReadLatency = 0;
	double chipReadDynamicEnergy = 0;
	double chipLeakageEnergy = 0;
	double chipLeakage = 0;
	double chipbufferLatency = 0;
	double chipbufferReadDynamicEnergy = 0;
	double chipicLatency = 0;
	double chipicReadDynamicEnergy = 0;
	
	double chipLatencyADC = 0;
	double chipLatencyAccum = 0;
	double chipLatencyOther = 0;
	double chipEnergyADC = 0;
	double chipEnergyAccum = 0;
	double chipEnergyOther = 0;
	
	double layerReadLatency = 0;
	double layerReadDynamicEnergy = 0;
	double tileLeakage = 0;
	double layerbufferLatency = 0;
	double layerbufferDynamicEnergy = 0;
	double layericLatency = 0;
	double layericDynamicEnergy = 0;
	
	double coreLatencyADC = 0;
	double coreLatencyAccum = 0;
	double coreLatencyOther = 0;
	double coreEnergyADC = 0;
	double coreEnergyAccum = 0;
	double coreEnergyOther = 0;
	
	if (param->synchronous){
		// calculate clkFreq
		for (int i=0; i<netStructure.size(); i++) {		
			ChipCalculatePerformance(inputParameter, tech, cell, i, argv[2*i+11], argv[2*i+11], argv[2*i+12], netStructure[i][6],
						netStructure, markNM, numTileEachLayer, utilizationEachLayer, speedUpEachLayer, tileLocaEachLayer,
						numPENM, desiredPESizeNM, desiredTileSizeCM, desiredPESizeCM, CMTileheight, CMTilewidth, NMTileheight, NMTilewidth,
						&layerReadLatency, &layerReadDynamicEnergy, &tileLeakage, &layerbufferLatency, &layerbufferDynamicEnergy, &layericLatency, &layericDynamicEnergy,
						&coreLatencyADC, &coreLatencyAccum, &coreLatencyOther, &coreEnergyADC, &coreEnergyAccum, &coreEnergyOther, true, &layerclkPeriod);
			if(clkPeriod < layerclkPeriod){
				clkPeriod = layerclkPeriod;
			}			
		}		
		if(param->clkFreq > 1/clkPeriod){
			param->clkFreq = 1/clkPeriod;
		}
	}

	cout << "-------------------------------------- Hardware Performance --------------------------------------" <<  endl;	
	if (! param->pipeline) {
	    ofstream layerfile;
        layerfile.open("Layer.csv", ios::out);
        layerfile << "# of Tiles, Speed-up, Utilization, Read Latency (ns), Read Dynamic Energy (pJ), Leakage Power (uW), " <<
        "Leakage Energy (pJ), Buffer Latency (ns), Buffer Read Dynamic Energy (pJ), IC Latency (ns), IC Read Dynamic Energy (ns), " <<
        "ADC Read Latency (ns), Accumulation Circuits Read Latency (ns), Other Peripheries Read Latency (ns), " <<
        "ADC Read Dynamic Energy (pJ), Accumulation Circuits Read Dynamic Energy (pJ), Other Peripheries Read Dynamic Energy (pJ)" << endl;


		// layer-by-layer process
		// show the detailed hardware performance for each layer
		for (int i=0; i<netStructure.size(); i++) {
		    layerfile << numTileEachLayer[0][i] * numTileEachLayer[1][i] << ", ";
            layerfile << speedUpEachLayer[0][i] * speedUpEachLayer[1][i] << ", ";
            layerfile << utilizationEachLayer[i][0] << ", ";

			cout << "-------------------- Estimation of Layer " << i+1 << " ----------------------" << endl;

			ChipCalculatePerformance(inputParameter, tech, cell, i, argv[2*i+11], argv[2*i+11], argv[2*i+12], netStructure[i][6],
						netStructure, markNM, numTileEachLayer, utilizationEachLayer, speedUpEachLayer, tileLocaEachLayer,
						numPENM, desiredPESizeNM, desiredTileSizeCM, desiredPESizeCM, CMTileheight, CMTilewidth, NMTileheight, NMTilewidth,
						&layerReadLatency, &layerReadDynamicEnergy, &tileLeakage, &layerbufferLatency, &layerbufferDynamicEnergy, &layericLatency, &layericDynamicEnergy,
						&coreLatencyADC, &coreLatencyAccum, &coreLatencyOther, &coreEnergyADC, &coreEnergyAccum, &coreEnergyOther, false, &layerclkPeriod);
			if (param->synchronous) {
				layerReadLatency *= clkPeriod;
				layerbufferLatency *= clkPeriod;
				layericLatency *= clkPeriod;
				coreLatencyADC *= clkPeriod;
				coreLatencyAccum *= clkPeriod;
				coreLatencyOther *= clkPeriod;
			}
			
			double numTileOtherLayer = 0;
			double layerLeakageEnergy = 0;		
			for (int j=0; j<netStructure.size(); j++) {
				if (j != i) {
					numTileOtherLayer += numTileEachLayer[0][j] * numTileEachLayer[1][j];
				}
			}
			layerLeakageEnergy = numTileOtherLayer*layerReadLatency*tileLeakage;
			
			cout << "layer" << i+1 << "'s readLatency is: " << layerReadLatency*1e9 << "ns" << endl;
			layerfile << layerReadLatency*1e9 << ", ";
			cout << "layer" << i+1 << "'s readDynamicEnergy is: " << layerReadDynamicEnergy*1e12 << "pJ" << endl;
			layerfile << layerReadDynamicEnergy*1e12 << ", ";
			cout << "layer" << i+1 << "'s leakagePower is: " << numTileEachLayer[0][i] * numTileEachLayer[1][i] * tileLeakage*1e6 << "uW" << endl;
			layerfile << numTileEachLayer[0][i] * numTileEachLayer[1][i] * tileLeakage*1e6 << ", ";
			cout << "layer" << i+1 << "'s leakageEnergy is: " << layerLeakageEnergy*1e12 << "pJ" << endl;
			layerfile << layerLeakageEnergy*1e12 << ", ";
			cout << "layer" << i+1 << "'s buffer latency is: " << layerLeakageEnergy*1e12 << "ns" << endl;
			layerfile << layerLeakageEnergy*1e12 << ", ";
			cout << "layer" << i+1 << "'s buffer readDynamicEnergy is: " << layerbufferDynamicEnergy*1e12 << "pJ" << endl;
			layerfile << layerbufferDynamicEnergy*1e12 << ", ";
			cout << "layer" << i+1 << "'s ic latency is: " << layericLatency*1e9 << "ns" << endl;
			layerfile << layericLatency*1e9 << ", ";
			cout << "layer" << i+1 << "'s ic readDynamicEnergy is: " << layericDynamicEnergy*1e12 << "pJ" << endl;
			layerfile << layericDynamicEnergy*1e12 << ", ";
			
			
			cout << endl;
			cout << "************************ Breakdown of Latency and Dynamic Energy *************************" << endl;
			cout << endl;
			cout << "----------- ADC (or S/As and precharger for SRAM) readLatency is : " << coreLatencyADC*1e9 << "ns" << endl;
			layerfile << coreLatencyADC*1e9 << ", ";
			cout << "----------- Accumulation Circuits (subarray level: adders, shiftAdds; PE/Tile/Global level: accumulation units) readLatency is : " << coreLatencyAccum*1e9 << "ns" << endl;
			layerfile << coreLatencyAccum*1e9 << ", ";
			cout << "----------- Other Peripheries (e.g. decoders, mux, switchmatrix, buffers, IC, pooling and activation units) readLatency is : " << coreLatencyOther*1e9 << "ns" << endl;
			layerfile << coreLatencyOther*1e9 << ", ";
			cout << "----------- ADC (or S/As and precharger for SRAM) readDynamicEnergy is : " << coreEnergyADC*1e12 << "pJ" << endl;
			layerfile << coreEnergyADC*1e12 << ", ";
			cout << "----------- Accumulation Circuits (subarray level: adders, shiftAdds; PE/Tile/Global level: accumulation units) readDynamicEnergy is : " << coreEnergyAccum*1e12 << "pJ" << endl;
			layerfile << coreEnergyAccum*1e12 << ", ";
			cout << "----------- Other Peripheries (e.g. decoders, mux, switchmatrix, buffers, IC, pooling and activation units) readDynamicEnergy is : " << coreEnergyOther*1e12 << "pJ" << endl;
			layerfile << coreEnergyOther*1e12 << endl;
			cout << endl;
			cout << "************************ Breakdown of Latency and Dynamic Energy *************************" << endl;
			cout << endl;
			
			chipReadLatency += layerReadLatency;
			chipReadDynamicEnergy += layerReadDynamicEnergy;
			chipLeakageEnergy += layerLeakageEnergy;
			chipLeakage += tileLeakage*numTileEachLayer[0][i] * numTileEachLayer[1][i];
			chipbufferLatency += layerbufferLatency;
			chipbufferReadDynamicEnergy += layerbufferDynamicEnergy;
			chipicLatency += layericLatency;
			chipicReadDynamicEnergy += layericDynamicEnergy;
			
			chipLatencyADC += coreLatencyADC;
			chipLatencyAccum += coreLatencyAccum;
			chipLatencyOther += coreLatencyOther;
			chipEnergyADC += coreEnergyADC;
			chipEnergyAccum += coreEnergyAccum;
			chipEnergyOther += coreEnergyOther;
		}
		layerfile.close();
	} else {
		// pipeline system
		// firstly define system clock
		double systemClock = 0;
		
		vector<double> readLatencyPerLayer;
		vector<double> readDynamicEnergyPerLayer;
		vector<double> leakagePowerPerLayer;
		vector<double> bufferLatencyPerLayer;
		vector<double> bufferEnergyPerLayer;
		vector<double> icLatencyPerLayer;
		vector<double> icEnergyPerLayer;
		
		vector<double> coreLatencyADCPerLayer;
		vector<double> coreEnergyADCPerLayer;
		vector<double> coreLatencyAccumPerLayer;
		vector<double> coreEnergyAccumPerLayer;
		vector<double> coreLatencyOtherPerLayer;
		vector<double> coreEnergyOtherPerLayer;
		
		for (int i=0; i<netStructure.size(); i++) {
			ChipCalculatePerformance(inputParameter, tech, cell, i, argv[2*i+11], argv[2*i+11], argv[2*i+12], netStructure[i][6],
						netStructure, markNM, numTileEachLayer, utilizationEachLayer, speedUpEachLayer, tileLocaEachLayer,
						numPENM, desiredPESizeNM, desiredTileSizeCM, desiredPESizeCM, CMTileheight, CMTilewidth, NMTileheight, NMTilewidth,
						&layerReadLatency, &layerReadDynamicEnergy, &tileLeakage, &layerbufferLatency, &layerbufferDynamicEnergy, &layericLatency, &layericDynamicEnergy,
						&coreLatencyADC, &coreLatencyAccum, &coreLatencyOther, &coreEnergyADC, &coreEnergyAccum, &coreEnergyOther, false, &layerclkPeriod);
			if (param->synchronous) {
				layerReadLatency *= clkPeriod;
				layerbufferLatency *= clkPeriod;
				layericLatency *= clkPeriod;
				coreLatencyADC *= clkPeriod;
				coreLatencyAccum *= clkPeriod;
				coreLatencyOther *= clkPeriod;
			}			
			
			systemClock = MAX(systemClock, layerReadLatency);
			
			readLatencyPerLayer.push_back(layerReadLatency);
			readDynamicEnergyPerLayer.push_back(layerReadDynamicEnergy);
			leakagePowerPerLayer.push_back(numTileEachLayer[0][i] * numTileEachLayer[1][i] * tileLeakage);
			bufferLatencyPerLayer.push_back(layerbufferLatency);
			bufferEnergyPerLayer.push_back(layerbufferDynamicEnergy);
			icLatencyPerLayer.push_back(layericLatency);
			icEnergyPerLayer.push_back(layericDynamicEnergy);
			
			coreLatencyADCPerLayer.push_back(coreLatencyADC);
			coreEnergyADCPerLayer.push_back(coreEnergyADC);
			coreLatencyAccumPerLayer.push_back(coreLatencyAccum);
			coreEnergyAccumPerLayer.push_back(coreEnergyAccum);
			coreLatencyOtherPerLayer.push_back(coreLatencyOther);
			coreEnergyOtherPerLayer.push_back(coreEnergyOther);
		}
		
		for (int i=0; i<netStructure.size(); i++) {
			
			cout << "-------------------- Estimation of Layer " << i+1 << " ----------------------" << endl;

			cout << "layer" << i+1 << "'s readLatency is: " << readLatencyPerLayer[i]*1e9 << "ns" << endl;
			cout << "layer" << i+1 << "'s readDynamicEnergy is: " << readDynamicEnergyPerLayer[i]*1e12 << "pJ" << endl;
			cout << "layer" << i+1 << "'s leakagePower is: " << leakagePowerPerLayer[i]*1e6 << "uW" << endl;
			cout << "layer" << i+1 << "'s leakageEnergy is: " << leakagePowerPerLayer[i] * (systemClock-readLatencyPerLayer[i]) *1e12 << "pJ" << endl;
			cout << "layer" << i+1 << "'s buffer latency is: " << bufferLatencyPerLayer[i]*1e9 << "ns" << endl;
			cout << "layer" << i+1 << "'s buffer readDynamicEnergy is: " << bufferEnergyPerLayer[i]*1e12 << "pJ" << endl;
			cout << "layer" << i+1 << "'s ic latency is: " << icLatencyPerLayer[i]*1e9 << "ns" << endl;
			cout << "layer" << i+1 << "'s ic readDynamicEnergy is: " << icEnergyPerLayer[i]*1e12 << "pJ" << endl;

			cout << endl;
			cout << "************************ Breakdown of Latency and Dynamic Energy *************************" << endl;
			cout << endl;
			cout << "----------- ADC (or S/As and precharger for SRAM) readLatency is : " << coreLatencyADCPerLayer[i]*1e9 << "ns" << endl;
			cout << "----------- Accumulation Circuits (subarray level: adders, shiftAdds; PE/Tile/Global level: accumulation units) readLatency is : " << coreLatencyAccumPerLayer[i]*1e9 << "ns" << endl;
			cout << "----------- Other Peripheries (e.g. decoders, mux, switchmatrix, buffers, IC, pooling and activation units) readLatency is : " << coreLatencyOtherPerLayer[i]*1e9 << "ns" << endl;
			cout << "----------- ADC (or S/As and precharger for SRAM) readDynamicEnergy is : " << coreEnergyADCPerLayer[i]*1e12 << "pJ" << endl;
			cout << "----------- Accumulation Circuits (subarray level: adders, shiftAdds; PE/Tile/Global level: accumulation units) readDynamicEnergy is : " << coreEnergyAccumPerLayer[i]*1e12 << "pJ" << endl;
			cout << "----------- Other Peripheries (e.g. decoders, mux, switchmatrix, buffers, IC, pooling and activation units) readDynamicEnergy is : " << coreEnergyOtherPerLayer[i]*1e12 << "pJ" << endl;
			cout << endl;
			cout << "************************ Breakdown of Latency and Dynamic Energy *************************" << endl;
			cout << endl;
			
			chipReadLatency = systemClock;
			chipReadDynamicEnergy += readDynamicEnergyPerLayer[i];
			chipLeakageEnergy += leakagePowerPerLayer[i] * (systemClock-readLatencyPerLayer[i]);
			chipLeakage += leakagePowerPerLayer[i];
			chipbufferLatency = MAX(chipbufferLatency, bufferLatencyPerLayer[i]);
			chipbufferReadDynamicEnergy += bufferEnergyPerLayer[i];
			chipicLatency = MAX(chipicLatency, icLatencyPerLayer[i]);
			chipicReadDynamicEnergy += icEnergyPerLayer[i];
			
			chipLatencyADC = MAX(chipLatencyADC, coreLatencyADCPerLayer[i]);
			chipLatencyAccum = MAX(chipLatencyAccum, coreLatencyAccumPerLayer[i]);
			chipLatencyOther = MAX(chipLatencyOther, coreLatencyOtherPerLayer[i]);
			chipEnergyADC += coreEnergyADCPerLayer[i];
			chipEnergyAccum += coreEnergyAccumPerLayer[i];
			chipEnergyOther += coreEnergyOtherPerLayer[i];
		}
		
	}

	ofstream summaryfile;
    summaryfile.open("Summary.csv", ios::out);
    summaryfile << "Memory Utilization (%), ChipArea (um^2), Chip total CIM array (um^2), Total IC Area on chip (um^2), Total ADC Area on chip (um^2I), " <<
    "Total Accumulation Circuits on chip (um^2), Other Peripheries (um^2), Chip layer-by-layer readLatency (ns), " <<
    "Chip total readDynamicEnergy (pJ), Chip total leakage Energy (pJ), Chip total leakage Power (uW), Chip buffer readLatency (ns), " <<
    "Chip buffer readDynamicEnergy (pJ), Chip ic readLatency (ns), Chip ic readDynamicEnergy (pJ), ADC readLatency (ns), " <<
    "Accumulation Circuits readLatency (ns), Other Peripheries readLatency (ns), ADC readDynamicEnergy (pJ), " <<
    "Accumulation Circuits readDynamicEnergy (pJ), Other Peripheries readDynamicEnergy (pJ), Energy Efficiency TOPS/W, " <<
    "Throughput TOPS, Throughput FPS, Compute efficiency TOPS/mm^2" << endl;
    summaryfile << realMappedMemory/totalNumTile*100 << ", ";
	
	cout << "------------------------------ Summary --------------------------------" <<  endl;
	cout << endl;
	cout << "ChipArea : " << chipArea*1e12 << "um^2" << endl;
	summaryfile << chipArea*1e12 << ",";
	cout << "Chip total CIM array : " << chipAreaArray*1e12 << "um^2" << endl;
	summaryfile << chipAreaArray*1e12 << ",";
	cout << "Total IC Area on chip (Global and Tile/PE local): " << chipAreaIC*1e12 << "um^2" << endl;
	summaryfile << chipAreaIC*1e12 << ",";
	cout << "Total ADC (or S/As and precharger for SRAM) Area on chip : " << chipAreaADC*1e12 << "um^2" << endl;
	summaryfile << chipAreaADC*1e12 << ",";
	cout << "Total Accumulation Circuits (subarray level: adders, shiftAdds; PE/Tile/Global level: accumulation units) on chip : " << chipAreaAccum*1e12 << "um^2" << endl;
	summaryfile << chipAreaAccum*1e12 << ",";
	cout << "Other Peripheries (e.g. decoders, mux, switchmatrix, buffers, pooling and activation units) : " << chipAreaOther*1e12 << "um^2" << endl;
	summaryfile << chipAreaOther*1e12 << ",";
	cout << endl;
	if (! param->pipeline) {
		if (param->synchronous) cout << "Chip clock period is: " << clkPeriod*1e9 << "ns" <<endl;
		cout << "Chip layer-by-layer readLatency (per image) is: " << chipReadLatency*1e9 << "ns" << endl;
		summaryfile << chipReadLatency*1e9 << ",";
		cout << "Chip total readDynamicEnergy is: " << chipReadDynamicEnergy*1e12 << "pJ" << endl;
		summaryfile << chipReadDynamicEnergy*1e12 << ",";
		cout << "Chip total leakage Energy is: " << chipLeakageEnergy*1e12 << "pJ" << endl;
		summaryfile << chipLeakageEnergy*1e12 << ",";
		cout << "Chip total leakage Power is: " << chipLeakage*1e6 << "uW" << endl;
		summaryfile << chipLeakage*1e6 << ",";
		cout << "Chip buffer readLatency is: " << chipbufferLatency*1e9 << "ns" << endl;
		summaryfile << chipbufferLatency*1e9 << ",";
		cout << "Chip buffer readDynamicEnergy is: " << chipbufferReadDynamicEnergy*1e12 << "pJ" << endl;
		summaryfile << chipbufferReadDynamicEnergy*1e12 << ",";
		cout << "Chip ic readLatency is: " << chipicLatency*1e9 << "ns" << endl;
		summaryfile << chipicLatency*1e9 << ",";
		cout << "Chip ic readDynamicEnergy is: " << chipicReadDynamicEnergy*1e12 << "pJ" << endl;
		summaryfile << chipicReadDynamicEnergy*1e12 << ",";
	} else {
		if (param->synchronous) cout << "Chip clock period is: " << clkPeriod*1e9 << "ns" <<endl;
		cout << "Chip pipeline-system-clock-cycle (per image) is: " << chipReadLatency*1e9 << "ns" << endl;
		cout << "Chip pipeline-system readDynamicEnergy (per image) is: " << chipReadDynamicEnergy*1e12 << "pJ" << endl;
		cout << "Chip pipeline-system leakage Energy (per image) is: " << chipLeakageEnergy*1e12 << "pJ" << endl;
		cout << "Chip pipeline-system leakage Power (per image) is: " << chipLeakage*1e6 << "uW" << endl;
		cout << "Chip pipeline-system buffer readLatency (per image) is: " << chipbufferLatency*1e9 << "ns" << endl;
		cout << "Chip pipeline-system buffer readDynamicEnergy (per image) is: " << chipbufferReadDynamicEnergy*1e12 << "pJ" << endl;
		cout << "Chip pipeline-system ic readLatency (per image) is: " << chipicLatency*1e9 << "ns" << endl;
		cout << "Chip pipeline-system ic readDynamicEnergy (per image) is: " << chipicReadDynamicEnergy*1e12 << "pJ" << endl;
	}
	
	cout << endl;
	cout << "************************ Breakdown of Latency and Dynamic Energy *************************" << endl;
	cout << endl;
	cout << "----------- ADC (or S/As and precharger for SRAM) readLatency is : " << chipLatencyADC*1e9 << "ns" << endl;
	summaryfile << chipLatencyADC*1e9 << ",";
	cout << "----------- Accumulation Circuits (subarray level: adders, shiftAdds; PE/Tile/Global level: accumulation units) readLatency is : " << chipLatencyAccum*1e9 << "ns" << endl;
	summaryfile << chipLatencyAccum*1e9 << ",";
	cout << "----------- Other Peripheries (e.g. decoders, mux, switchmatrix, buffers, IC, pooling and activation units) readLatency is : " << chipLatencyOther*1e9 << "ns" << endl;
	summaryfile << chipLatencyOther*1e9 << ",";
	cout << "----------- ADC (or S/As and precharger for SRAM) readDynamicEnergy is : " << chipEnergyADC*1e12 << "pJ" << endl;
	summaryfile << chipEnergyADC*1e12 << ",";
	cout << "----------- Accumulation Circuits (subarray level: adders, shiftAdds; PE/Tile/Global level: accumulation units) readDynamicEnergy is : " << chipEnergyAccum*1e12 << "pJ" << endl;
	summaryfile << chipEnergyAccum*1e12 << ",";
	cout << "----------- Other Peripheries (e.g. decoders, mux, switchmatrix, buffers, IC, pooling and activation units) readDynamicEnergy is : " << chipEnergyOther*1e12 << "pJ" << endl;
	summaryfile << chipEnergyOther*1e12 << ",";
	cout << endl;
	cout << "************************ Breakdown of Latency and Dynamic Energy *************************" << endl;
	cout << endl;
	
	cout << endl;
	cout << "----------------------------- Performance -------------------------------" << endl;
	if (! param->pipeline) {
		if(param->validated){
			cout << "Energy Efficiency TOPS/W (Layer-by-Layer Process): " << numComputation/(chipReadDynamicEnergy*1e12+chipLeakageEnergy*1e12)/param->zeta << endl;	// post-layout energy increase, zeta = 1.23 by default
		    summaryfile << numComputation/(chipReadDynamicEnergy*1e12+chipLeakageEnergy*1e12)/param->zeta << ",";
		}else{
			cout << "Energy Efficiency TOPS/W (Layer-by-Layer Process): " << numComputation/(chipReadDynamicEnergy*1e12+chipLeakageEnergy*1e12) << endl;
		    summaryfile << numComputation/(chipReadDynamicEnergy*1e12+chipLeakageEnergy*1e12) << ",";
		}
		cout << "Throughput TOPS (Layer-by-Layer Process): " << numComputation/(chipReadLatency*1e12) << endl;
		summaryfile << numComputation/(chipReadLatency*1e12) << ",";
		cout << "Throughput FPS (Layer-by-Layer Process): " << 1/(chipReadLatency) << endl;
		summaryfile << 1/(chipReadLatency) << ",";
		cout << "Compute efficiency TOPS/mm^2 (Layer-by-Layer Process): " << numComputation/(chipReadLatency*1e12)/(chipArea*1e6) << endl;
	    summaryfile << numComputation/(chipReadLatency*1e12)/(chipArea*1e6) << endl;
	} else {
		if(param->validated){
			cout << "Energy Efficiency TOPS/W (Pipelined Process): " << numComputation/(chipReadDynamicEnergy*1e12+chipLeakageEnergy*1e12)/param->zeta << endl;	// post-layout energy increase, zeta = 1.23 by default
		}else{
			cout << "Energy Efficiency TOPS/W (Pipelined Process): " << numComputation/(chipReadDynamicEnergy*1e12+chipLeakageEnergy*1e12) << endl;
		}
		cout << "Throughput TOPS (Pipelined Process): " << numComputation/(chipReadLatency*1e12) << endl;
		cout << "Throughput FPS (Pipelined Process): " << 1/(chipReadLatency) << endl;
		cout << "Compute efficiency TOPS/mm^2 (Pipelined Process): " << numComputation/(chipReadLatency*1e12)/(chipArea*1e6) << endl;
	}
	cout << "-------------------------------------- Hardware Performance Done --------------------------------------" <<  endl;
	cout << endl;
	auto stop = chrono::high_resolution_clock::now();
	auto duration = chrono::duration_cast<chrono::seconds>(stop-start);
    cout << "------------------------------ Simulation Performance --------------------------------" <<  endl;
	cout << "Total Run-time of NeuroSim: " << duration.count() << " seconds" << endl;
	cout << "------------------------------ Simulation Performance --------------------------------" <<  endl;
	summaryfile.close();
	return 0;
}

vector<vector<double> > getNetStructure(const string &inputfile) {
	ifstream infile(inputfile.c_str());      
	string inputline;
	string inputval;
	
	int ROWin=0, COLin=0;      
	if (!infile.good()) {        
		cerr << "Error: the input file cannot be opened!" << endl;
		exit(1);
	}else{
		while (getline(infile, inputline, '\n')) {       
			ROWin++;                                
		}
		infile.clear();
		infile.seekg(0, ios::beg);      
		if (getline(infile, inputline, '\n')) {        
			istringstream iss (inputline);      
			while (getline(iss, inputval, ',')) {       
				COLin++;
			}
		}	
	}
	infile.clear();
	infile.seekg(0, ios::beg);          

	vector<vector<double> > netStructure;               
	for (int row=0; row<ROWin; row++) {	
		vector<double> netStructurerow;
		getline(infile, inputline, '\n');             
		istringstream iss;
		iss.str(inputline);
		for (int col=0; col<COLin; col++) {       
			while(getline(iss, inputval, ',')){	
				istringstream fs;
				fs.str(inputval);
				double f=0;
				fs >> f;				
				netStructurerow.push_back(f);			
			}			
		}		
		netStructure.push_back(netStructurerow);
	}
	infile.close();
	
	return netStructure;
	netStructure.clear();
}	



