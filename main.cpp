#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
using namespace std;

class ImagePP {
public:
    class Box {
    public:
        int minR;
        int minC;
        int maxR;
        int maxC;

        Box() {minR = 0; minC = 0; maxR = 0; maxC = 0;}

        Box(int minRow, int minCol, int maxRow, int maxCol) : minR(minRow), minC(minCol), maxR(maxRow), maxC(maxCol) {}

        ~Box() = default;
    };

    class BoxNode {
    public:
        int boxType; // 0 - dummy node; 1 - doc box; 2 - paragraph; 3 - textLine;
        Box BBox;
        BoxNode* next = nullptr;

        BoxNode() {boxType = 0;}

        BoxNode(int minR, int minC, int maxR, int maxC, int type) : boxType(type), BBox(minR, minC, maxR, maxC) {}

        ~BoxNode() = default;
    };

    class BoxQ {
    public:
        BoxNode* qFront = nullptr;
        BoxNode* qBack = nullptr;
        int length;

        BoxQ() {
            qFront = new BoxNode();
            qBack = qFront;
            length = 0;
        }

        ~BoxQ() {
            BoxNode* current = qFront;
            while(current != nullptr) {
                BoxNode* next = current->next;
                delete current;
                current = next;
            }
            qFront = nullptr;
        }

        void insert(BoxNode& newNode) {
            qBack->next = &newNode;
            qBack = &newNode;
            length++;
        }

        void insert(int minR, int minC, int maxR, int maxC, int type) {
            BoxNode* newNode = new BoxNode(minR, minC, maxR, maxC, type);
            qBack->next = newNode;
            qBack = newNode;
            length++;
        }
    };

    static const int structElem[3];
    static int structLength;
    static int structOrigin;

    static int numRows;
    static int numCols;
    static int minVal;
    static int maxVal;
    static int HPPmin;
    static int HPPmax;
    static int HPPbinMin; // morph values will be the same as bin values.
    static int HPPbinMax;
    static int VPPmin;
    static int VPPmax;
    static int VPPbinMin;
    static int VPPbinMax;
    static int** imgAry;
    static int thrVal;
    static int* HPP;
    static int* VPP;
    static int* HPPbin;
    static int* VPPbin;
    static int* HPPmorph;
    static int* VPPmorph;
    static int* HPPtemp;
    static int* VPPtemp;

    static BoxQ boxQueue;

    ImagePP(int rows, int cols, int min, int max, int threshold) {
        numRows = rows;
        numCols = cols;
        minVal = min;
        maxVal = max;
        thrVal = threshold;
        imgAry = new int*[rows+2];
        HPP = new int[rows+2];
        HPPbin = new int[rows+2];
        HPPmorph = new int[rows+2];
        HPPtemp = new int[rows+2];
        VPP = new int[cols+2];
        VPPbin = new int[cols+2];
        VPPmorph = new int[cols+2];
        VPPtemp = new int[cols+2];
        for(int i = 0; i < rows+2; i++) {
            imgAry[i] = new int[cols+2];
        }
    }

    ~ImagePP() {
        for(int i = 0; i < numRows+2; i++) {
            delete[] imgAry[i];
        }
        delete[] imgAry;
        delete[] HPP;
        delete[] HPPbin;
        delete[] HPPmorph;
        delete[] HPPtemp;
        delete[] VPP;
        delete[] VPPbin;
        delete[] VPPmorph;
        delete[] VPPtemp;
    }

    static void zeroFramed() {
        for(int i = 0; i <= numRows+1; i++) {
            for(int j = 0; j <= numCols+1; j++) {
                imgAry[i][j] = 0;
            }
        }
    }

    static void loadImage(ifstream& imgFile) {
        for (int i = 1; i < numRows+1; i++) {
            for(int j = 1; j < numCols+1; j++) {
                imgFile >> imgAry[i][j];
            }
        }
    }

    static void computeHPPandVPP() {
        for(int i = 1; i < numRows+1; i++) {
            for(int j = 1; j < numCols+1; j++) {
                if(imgAry[i][j] > 0) {
                    HPP[i]++;
                    VPP[j]++;
                }
            }
        }
        HPPmin = 999;
        HPPmax = 0;
        for(int i = 1; i < numRows+1; i++) {
            if(HPP[i] < HPPmin) HPPmin = HPP[i];
            if(HPP[i] > HPPmax) HPPmax = HPP[i];
        }
        VPPmin = 999;
        VPPmax = 0;
        for(int i = 1; i < numCols+1; i++) {
            if(VPP[i] < VPPmin) VPPmin = VPP[i];
            if(VPP[i] > VPPmax) VPPmax = VPP[i];
        }
    }

    static void threshold(const int* ary, int* binAry, int length, int& min, int& max) {
        if(length < 1) return; // ary is empty
        bool aboveThreshold = false;
        bool belowThreshold = false;
        for (int i = 1; i < length+1; i++) {
            if(ary[i] >= thrVal) { // there are values at or above the threshold
                binAry[i] = 1;
                aboveThreshold = true;
            }
            else {
                binAry[i] = 0;
                belowThreshold = true;
            }
        }
        if(aboveThreshold && belowThreshold) { // there are both 0s and 1s
            min = 0;
            max = 1;
        }
        else if(aboveThreshold) { // there are only 1s
            min = 1;
            max = 1;
        }
        else if(belowThreshold) { // there are only 0s
            min = 0;
            max = 0;
        }
    }

    static void printPP(int* ary, ofstream& outFile, int length, const string& optionalCaption, int min, int max) {
        // print headers first
        outFile << numRows << " " << numCols << " " << min << " " << max << endl;
        for(int i = 1; i < length+1; i++) {
            outFile << i << " " << ary[i] << endl;
        }
        if(!optionalCaption.empty()) {
            outFile << optionalCaption << endl << endl;
        }
        else outFile << endl;
    }

    // applies dilation on PPbin to PPtemp and then erosion on PPtemp to PPmorph
    static void morphClosing(int* ary, int* tempAry, int* outAry, int length) {
        for(int i = 1; i < length+1; i++) {
            dilation(ary, tempAry, i);
        }
        for(int i = 1; i < length+1; i++) {
            erosion(tempAry, outAry, i);
        }
    }

    static void dilation(const int* inAry, int* outAry, int i) {
        if(inAry[i] == structElem[structOrigin]) {
            int aryIter = i - structOrigin;
            for(int k = 0; k < structLength; k++) {
                if(aryIter == i) {
                    aryIter++;
                    continue;
                }
                outAry[aryIter] = structElem[k];
                aryIter++;
            }
        }
    }

    static void erosion(const int* inAry, int* outAry, int i) {
        if(inAry[i] == structElem[structOrigin]) {
            bool structMatch = true;
            int aryIter = i - structOrigin;
            for(int k = 0; k < structLength; k++) {
                if(aryIter == i) {
                    aryIter++;
                    continue;
                }
                if(inAry[aryIter] != structElem[k]) structMatch = false;
                aryIter++;
            }
            if(structMatch) outAry[i] = structElem[structOrigin];
            else outAry[i] = 0;
        }
    }

    // if direction = -1, horizontal
    // 1, vertical
    // 0, indeterminate
    static void findLineBoxes(int direction) {
        if(direction == 0) return;
        else if(direction == -1) {
            bool boxFound = false;
            for(int k = 1; k < numRows+1; k++) { // a text line is a series of 1s between 0s in HPPmorph
                int minR = 0;
                int minC = 0;
                int maxR = 0;
                int maxC = 0;
                while(HPPmorph[k] > 0 && k <= numRows) { // determine run coordinates
                    boxFound = true;
                    if(minR == 0 || minR > k) {
                        minR = k;
                    }
                    if(maxR == 0 || maxR < k) {
                        maxR = k;
                    }
                    k++;
                }
                for(int i = minR; i <= maxR && boxFound; i++) { // determine column coordinates
                    for(int j = 1; j < numCols+1; j++) {
                        if(imgAry[i][j] > 0) {
                            if (minC == 0 || minC > j) {
                                minC = j;
                            }
                            if (maxC == 0 || maxC < j) {
                                maxC = j;
                            }
                        }
                    }
                }
                if(boxFound) {
                    boxQueue.insert(minR, minC, maxR, maxC, 3);
                    boxFound = false;
                }
            }
        }
        else if(direction == 1) {
            bool boxFound = false;
            for(int k = 1; k < numCols+1; k++) { // a text line is a series of 1s between 0s in VPPmorph
                int minR = 0;
                int minC = 0;
                int maxR = 0;
                int maxC = 0;
                while(VPPmorph[k] > 0 && k <= numCols) { // determine run coordinates
                    boxFound = true;
                    if(minC == 0 || minC > k) {
                        minC = k;
                    }
                    if(maxC == 0 || maxC < k) {
                        maxC = k;
                    }
                    k++;
                }
                for(int i = 1; i < numRows+1 && boxFound; i++) { // determine row coordinates
                    for(int j = minC; j <= maxC; j++) {
                        if (imgAry[i][j] > 0) {
                            if (minR == 0 || minR > i) {
                                minR = i;
                            }
                            if (maxR == 0 || maxR < i) {
                                maxR = i;
                            }
                        }
                    }
                }
                if(boxFound) {
                    boxQueue.insert(minR, minC, maxR, maxC, 3);
                    boxFound = false;
                }
            }
        }
    }

    // input is binary so runs are all sequences of 1s
    // returns -1 if direction is horizontal
    // 0 if direction is indeterminate
    // 1 if direction is vertical
    static int determineReadingDirection(int factor) {
        int HPPruns = 0;
        bool run = false;
        for(int i = 1; i < numRows+1; i++) {
            while(HPPmorph[i] > 0 && i <= numRows) {
                run = true;
                i++;
            }
            if(run) {
                run = false;
                HPPruns++;
            }
        }
        int VPPruns = 0;
        run = false;
        for(int i = 1; i < numCols+1; i++) {
            while(VPPmorph[i] > 0 && i <= numCols) {
                run = true;
                i++;
            }
            if(run) {
                run = false;
                VPPruns++;
            }
        }
        if(HPPruns >= factor * VPPruns) return -1;
        else if(VPPruns >= factor * HPPruns) return 1;
        else return 0;
    }

    static void printReadingDirection(ofstream& outFile, int factor) {
        int i = determineReadingDirection(factor);
        if(i == -1) {
            outFile << "Reading direction: horizontal." << endl << endl;
        }
        else if(i == 1) {
            outFile << "Reading direction: vertical." << endl << endl;
        }
        else {
            outFile << "Reading direction: indeterminate." << endl << endl;
        }
    }

    static void printBoxInfo(BoxNode& node, ofstream& outFile) {
        outFile << node.boxType << endl;
        outFile << node.BBox.minR << " " << node.BBox.minC << " " << node.BBox.maxR << " " << node.BBox.maxC << endl;
    }

    static void drawBoxes() {
        if(boxQueue.length > 0) {
            BoxNode* current = boxQueue.qFront->next;
            while(current != nullptr) {
                int minRow = current->BBox.minR;
                int minCol = current->BBox.minC;
                int maxRow = current->BBox.maxR;
                int maxCol = current->BBox.maxC;
                for(int m = minCol; m <= maxCol; m++) {
                    if(imgAry[minRow][m] == 0) {
                        imgAry[minRow][m] = 1;
                    }
                    if(imgAry[maxRow][m] == 0) {
                        imgAry[maxRow][m] = 1;
                    }
                }
                for(int n = minRow; n <= maxRow; n++) {
                    if(imgAry[n][minCol] == 0) {
                        imgAry[n][minCol] = 1;
                    }
                    if(imgAry[n][maxCol] == 0) {
                        imgAry[n][maxCol] = 1;
                    }
                }
                current = current->next;
            }
        }
    }

    static void printBoxQueue(ofstream &outFile) {
        if(boxQueue.length > 0) {
            BoxNode* current = boxQueue.qFront->next;
            while(current != nullptr) {
                printBoxInfo(*current, outFile);
                current = current->next;
            }
            outFile << endl;
        }
    }

    static void prettyPrint(ofstream& outFile, const string& optionalCaption, int **ary) {
        for (int i = 1; i < numRows+1; i++) {
            for (int j = 1; j < numCols+1; j++) {
                if(ary[i][j] > 0) outFile << ary[i][j] << " ";
                else outFile << "  ";
            }
            outFile << endl;
        }
        if(!optionalCaption.empty()) {
            outFile << optionalCaption << endl << endl;
        }
        else {
            outFile << endl;
        }
    }
};

int ImagePP::structLength = 3;
int ImagePP::structOrigin = 1;

const int ImagePP::structElem[3] = {1, 1, 1};

int ImagePP::numRows;
int ImagePP::numCols;
int ImagePP::minVal;
int ImagePP::maxVal;
int ImagePP::HPPmin;
int ImagePP::HPPmax;
int ImagePP::HPPbinMin;
int ImagePP::HPPbinMax;
int ImagePP::VPPmin;
int ImagePP::VPPmax;
int ImagePP::VPPbinMin;
int ImagePP::VPPbinMax;
int** ImagePP::imgAry;
int ImagePP::thrVal;
int* ImagePP::HPP;
int* ImagePP::VPP;
int* ImagePP::HPPbin;
int* ImagePP::VPPbin;
int* ImagePP::HPPmorph;
int* ImagePP::VPPmorph;
int* ImagePP::HPPtemp;
int* ImagePP::VPPtemp;

ImagePP::BoxQ ImagePP::boxQueue;


int main(int argc, char* argv[]) {
    ifstream inFile;
    int threshold;
    ofstream outFile1, outFile2;
    // 0
    inFile.open(argv[1]);
    threshold = stoi(argv[2]);
    outFile1.open(argv[3]);
    outFile2.open(argv[4]);
    // 1, 2, 3
    int rows, cols, min, max;
    inFile >> rows;
    inFile >> cols;
    inFile >> min;
    inFile >> max;
    ImagePP initializer(rows, cols, min, max, threshold);
    ImagePP::zeroFramed();
    ImagePP::loadImage(inFile);
    // 4
    ImagePP::computeHPPandVPP();
    // 5
    ImagePP::printPP(ImagePP::HPP, outFile2, rows, "HPP", ImagePP::HPPmin, ImagePP::HPPmax);
    ImagePP::printPP(ImagePP::VPP, outFile2, cols, "VPP", ImagePP::VPPmin, ImagePP::VPPmax);
    // 6
    ImagePP::threshold(ImagePP::HPP, ImagePP::HPPbin, rows, ImagePP::HPPbinMin, ImagePP::HPPbinMax);
    ImagePP::threshold(ImagePP::VPP, ImagePP::VPPbin, cols, ImagePP::VPPbinMin, ImagePP::VPPbinMax);
    // 7
    ImagePP::printPP(ImagePP::HPPbin, outFile2, rows, "HPPbin", ImagePP::HPPbinMin, ImagePP::HPPbinMax);
    ImagePP::printPP(ImagePP::VPPbin, outFile2, cols, "VPPbin", ImagePP::VPPbinMin, ImagePP::VPPbinMax);
    // 8
    ImagePP::morphClosing(ImagePP::HPPbin, ImagePP::HPPtemp, ImagePP::HPPmorph, rows);
    ImagePP::morphClosing(ImagePP::VPPbin, ImagePP::VPPtemp, ImagePP::VPPmorph, cols);
    // extra debug step for PPmorph
    ImagePP::printPP(ImagePP::HPPmorph, outFile2, rows, "HPPmorph", ImagePP::HPPbinMin, ImagePP::HPPbinMax);
    ImagePP::printPP(ImagePP::VPPmorph, outFile2, cols, "VPPmorph", ImagePP::HPPbinMin, ImagePP::HPPbinMax);
    // 9, 10
    ImagePP::printReadingDirection(outFile2, 3);
    // 11, 12
    ImagePP::findLineBoxes(ImagePP::determineReadingDirection(3));
    // 13
    ImagePP::drawBoxes();
    // 14
    ImagePP::prettyPrint(outFile1, "with bounding boxes", ImagePP::imgAry);
    // 15
    ImagePP::printBoxQueue(outFile2);
    //
    inFile.close();
    outFile1.close();
    outFile2.close();
    return 0;
}
