#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

using namespace std;
namespace fs = std::filesystem;

/**
 * Display an image using WxWidgets.
 * https://www.wxwidgets.org/
 */

/** Declarations*/

/**
 * Class that implements wxApp
 */
class MyApp : public wxApp {
 public:
  bool OnInit() override;
};

/**
 * Class that implements wxFrame.
 * This frame serves as the top level window for the program
 */
class MyFrame : public wxFrame {
 public:
  MyFrame(const wxString &title, string imagePath, int newWidth, int newHeight, int channelBits, int mode);

 private:
  void OnPaint(wxPaintEvent &event);
  wxImage inImage;
  wxScrolledWindow *scrolledWindow;
  int width;
  int height;
};

/** Utility function to read image data */
unsigned char *readImageData(string imagePath, int width, int height);

/** Definitions */

/**
 * Init method for the app.
 * Here we process the command line arguments and
 * instantiate the frame.
 */
bool MyApp::OnInit() {
  wxInitAllImageHandlers();

  // deal with command line arguments here
  cout << "Number of command line arguments: " << wxApp::argc << endl;
  if (wxApp::argc != 5) {
    cerr << "usage: ./exec imgpath channelbits filtermode"
         << endl;
    exit(1);
  }
  
  cout << "First argument: " << wxApp::argv[0] << endl;
  cout << "Second argument: " << wxApp::argv[1] << endl;
  string imagePath = wxApp::argv[1].ToStdString();
  double *scale = new double;
  wxApp::argv[2].ToDouble(scale);
  int *channelBits = new int;
  wxApp::argv[3].ToInt(channelBits);
  int *mode = new int;
  wxApp::argv[4].ToInt(mode);
  cout << "3nd argument: " << wxApp::argv[2] << endl;
  cout << "4rd argument: " << wxApp::argv[3] << endl;
  cout << "5th argument: " << wxApp::argv[4] << endl;
  int dimensions = *scale * 512;
  MyFrame *frame = new MyFrame("Image Display", imagePath, dimensions, dimensions, *channelBits, *mode);

  frame->Show(true);

  // return true to continue, false to exit the application
  return true;
}

/**
 * Constructor for the MyFrame class.
 * Here we read the pixel data from the file and set up the scrollable window.
 */

void CalculateKernel(unsigned char* image, int height, int width, int row, int col, unsigned char* outputCellRGB) {
  int totalSum[3] = {0, 0, 0};
  int numCells = 0;
  for (int a = -1; a <= 1; a++){
    for (int b = -1; b <= 1; b++){
      int curRow = row + a;
      int curCol = col + b;
      if (curRow < 0 || curRow >= height || curCol < 0 || curCol >= width){
        continue;
      }
      int pixelIndex = curRow*width*3 + curCol*3;
      for (int c=0; c<3; c++){
        totalSum[c] += image[pixelIndex + c];
      }
      numCells++;
    } 
  }
  for (int c=0; c<3; c++){
    totalSum[c] /= numCells;
    outputCellRGB[c] = totalSum[c];
  }
  return;
}


void QuantizePixel(unsigned char* data, int pixelIndex, int * intervals, int numBuckets){
  unsigned char origVal = data[pixelIndex];
  for (int i=0; i < numBuckets-1; i++){
    if (origVal >= intervals[i] && origVal <= intervals[i+1]){
      if (origVal <= (intervals[i]+intervals[i+1]) / 2) {
        data[pixelIndex] = intervals[i];
        return;
      }
      else{
        data[pixelIndex] = intervals[i+1];
        return;
      }
      //data[pixelIndex] = intervals[i];


    }
  }
  // cout << "Quantizing pixel at index " << pixelIndex << " with value " << (int)origVal << " to " << intervals[numBuckets-1] << endl;
  data[pixelIndex] = intervals[numBuckets-1];
  return;
}
double LogMap(double value, double n, double numLevels) {
  double scale = numLevels / 256.0;

  int convImg = static_cast<int>(exp2(floor(log2(round(scale * value)))));
  cout << "LogMap function called with value: " << value << ", n: " << n << ", numLevels: " << numLevels << endl;
  cout << "Scale calculated as: " << scale << endl;
  cout << "Converted image value: " << convImg << endl;
  return convImg;
}
  
MyFrame::MyFrame(const wxString &title, string imagePath, int newWidth, int newHeight, int channelBits, int mode)
    : wxFrame(NULL, wxID_ANY, title) {
  cout << "entered myframe function" << endl;

  // Modify the height and width values here to read and display an image with
  // different dimensions.    
  int ORIGINAL_WIDTH = 512;
  int ORIGINAL_HEIGHT = 512;


  unsigned char *inData = readImageData(imagePath, ORIGINAL_WIDTH, ORIGINAL_HEIGHT);
  unsigned char kernel[3];
  cout << "before kernel function" << endl;
  try {
    CalculateKernel(inData, ORIGINAL_HEIGHT, ORIGINAL_WIDTH, 1, 1, kernel);
  } catch (const std::exception &e) {
    cerr << "Error calculating kernel: " << e.what() << endl;
    exit(1);
  }
  cout << "after kernel funuctoins" << endl;

  cout << "kernel " << (int)kernel[0] << " " << (int)kernel[1] << " " << (int)kernel[2] << endl;

  //start rescaling here
  unsigned char *resizedData = 
  (unsigned char *)malloc(newWidth * newHeight * 3 * sizeof(unsigned char));

  double newRowRatio = newHeight / (double) ORIGINAL_HEIGHT;
  double newColRatio = newWidth / (double) ORIGINAL_WIDTH;
  cout << "newRowRatio " << newRowRatio << " newColRatio " << newColRatio << endl;
  for (int row = 0; row < ORIGINAL_HEIGHT; row++){
    for (int col = 0; col < ORIGINAL_WIDTH; col++){
      int newRow = row * newRowRatio;
      int newCol = col*newColRatio;
      int newIndex = newRow*newWidth*3 + newCol*3;
      //int oldIndex = row*ORIGINAL_WIDTH*3 + col*3;
      CalculateKernel(inData, ORIGINAL_HEIGHT, ORIGINAL_WIDTH, row, col, &(resizedData[newIndex]));
    }
  }

  //quantization
  
  int numBuckets = (1<<channelBits);
  int originalBitSize = (1<<8);
  int intervals[numBuckets+1];
  cout << "numBuckets" << numBuckets << " mode " << mode << endl; 
  if (channelBits != 8 ){
    
    if (mode == -1){
      double bitsPerBucket = originalBitSize / (double)numBuckets;
      double bucketVal = 0;
      for (int i = 0; i < numBuckets; i++ ){
        intervals[i] = (int)bucketVal;
        bucketVal += bitsPerBucket;
      }
      for (int i = 0; i < numBuckets; i++) {
        cout << "intervals[" << i << "] = " << intervals[i] << endl;
      }
      
    }
    else{
        int lb[numBuckets+1];
        int rlb[numBuckets+1];
        int slb[numBuckets+1];
        int srlb[numBuckets+1]; 
        lb[0] = 0;
        slb[0] = 0;
        mode = 255 - mode; // invert because we want to have more buckets near the pivot
        //followed a piazza answer 
        double d = log(256) / (double) numBuckets;
        for (int i=1; i < numBuckets+1; i++){
          lb[i] = exp(((double) i) * d);
          slb[i] = ceil(((double) lb[i]) * (mode / 256.0));
        }

        rlb[0] = lb[numBuckets];
        srlb[0] = ceil(((double) rlb[0]) * ((256-mode) / 256.0));
        
        int shift = slb[0] - srlb[0];
        intervals[0] = slb[0] - srlb[0] - shift;

        for (int i=1; i<numBuckets+1; i++){
          rlb[i] = lb[numBuckets-i];
          srlb[i] = ceil(((double) rlb[i]) * ((256-mode) / 256.0));
          intervals[i] = (slb[i] - srlb[i]) - shift;
        }
        
        
        // for (int i=0; i<numBuckets+1; i++){
        //   cout << "index: " << i << " lb " << lb[i] << " slb " << slb[i] << " rlb " << rlb[i] << " srlb " << srlb[i] << " intervals " << intervals[i] << endl; 
        // }
        // return;
    }

    for (int row = 0; row < newHeight; row++){
      for (int col = 0; col < newWidth; col++){
        int newIndex = row*newWidth*3 + col*3;
        for (int c=0; c < 3; c++){
          QuantizePixel(resizedData, newIndex+c, intervals, numBuckets);
        }
      }
    } 
  }

  
  // the last argument is static_data, if it is false, after this call the
  // pointer to the data is owned by the wxImage object, which will be
  // responsible for deleting it. So this means that you should not delete the
  // data yourself.
  width = newWidth;
  height = newHeight;
  inImage.SetData(resizedData, width, height, false);

  // Set up the scrolled window as a child of this frame
  scrolledWindow = new wxScrolledWindow(this, wxID_ANY);
  scrolledWindow->SetScrollbars(10, 10, width, height);
  scrolledWindow->SetVirtualSize(width, height);

  // Bind the paint event to the OnPaint function of the scrolled window
  scrolledWindow->Bind(wxEVT_PAINT, &MyFrame::OnPaint, this);

  // Set the frame size
  SetClientSize(width, height);

  // Set the frame background color
  SetBackgroundColour(*wxBLACK);
}

/**
 * The OnPaint handler that paints the UI.
 * Here we paint the image pixels into the scrollable window.
 */
void MyFrame::OnPaint(wxPaintEvent &event) {
  wxBufferedPaintDC dc(scrolledWindow);
  scrolledWindow->DoPrepareDC(dc);

  wxBitmap inImageBitmap = wxBitmap(inImage);
  dc.DrawBitmap(inImageBitmap, 0, 0, false);
}

/** Utility function to read image data */
unsigned char *readImageData(string imagePath, int width, int height) {

  // Open the file in binary mode
  ifstream inputFile(imagePath, ios::binary);

  if (!inputFile.is_open()) {
    cerr << "Error Opening File for Reading" << endl;
    exit(1);
  }

  // Create and populate RGB buffers
  vector<char> Rbuf(width * height);
  vector<char> Gbuf(width * height);
  vector<char> Bbuf(width * height);

  /**
   * The input RGB file is formatted as RRRR.....GGGG....BBBB.
   * i.e the R values of all the pixels followed by the G values
   * of all the pixels followed by the B values of all pixels.
   * Hence we read the data in that order.
   */

  inputFile.read(Rbuf.data(), width * height);
  inputFile.read(Gbuf.data(), width * height);
  inputFile.read(Bbuf.data(), width * height);

  inputFile.close();

  /**
   * Allocate a buffer to store the pixel values
   * The data must be allocated with malloc(), NOT with operator new. wxWidgets
   * library requires this.
   */
  unsigned char *inData =
      (unsigned char *)malloc(width * height * 3 * sizeof(unsigned char));
      
  for (int i = 0; i < height * width; i++) {
    // We populate RGB values of each pixel in that order
    // RGB.RGB.RGB and so on for all pixels
    inData[3 * i] = Rbuf[i];
    inData[3 * i + 1] = Gbuf[i];
    inData[3 * i + 2] = Bbuf[i];
  }

  // for (int i = 0; i < 512; i ++ ){
  //   for(int j = 0; j < 512; j ++ ){
  //     cout << (int)inData[3*i]  << ".";
  //     cout << (int)inData[3*i+1] << ".";
  //     cout << (int)inData[3*i+2]<<"|";
  //   }
  //   cout << endl;
  // }
  cout << "finished reading img data" << endl;
  return inData;
}

wxIMPLEMENT_APP(MyApp);