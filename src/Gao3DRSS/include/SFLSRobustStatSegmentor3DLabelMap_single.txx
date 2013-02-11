#ifndef SFLSRobustStatSegmentor3DLabelMap_single_txx_
#define SFLSRobustStatSegmentor3DLabelMap_single_txx_

#include "SFLSRobustStatSegmentor3DLabelMap_single.h"
#include "SFLSSegmentor3D.h"
#include <algorithm>
#include <ctime>

#include <limits>
#include <fstream>
#include "Logger.h"


#include "itkImageRegionConstIteratorWithIndex.h"
#include "itkImageRegionConstIterator.h"
#include "HistogramPlot.h"
using std::cout;
using std::endl;
using std::vector;
using std::list;
using std::cerr;
using namespace vrcl;

/* ============================================================   */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::basicInit()
{
  SuperClassType::basicInit();

  m_statNeighborX = 1;
  m_statNeighborY = 1;
  m_statNeighborZ = 1;
  m_kernelWidthFactor = 10.0;
  m_inputImageIntensityMin = 0;
  m_inputImageIntensityMax = 0;

  return;
}  


/* ============================================================  */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::setInputLabelImage(TLabelImagePointer l)
{
  m_inputLabelImage = l;

  TSize size = m_inputLabelImage->GetLargestPossibleRegion().GetSize();


  TIndex start = m_inputLabelImage->GetLargestPossibleRegion().GetIndex();
  TIndex origin = {{0, 0, 0}};
  if (start != origin)
    {
      std::cout<<"Warrning: Force mask start to be (0, 0, 0)\n";
      TRegion region = m_inputLabelImage->GetLargestPossibleRegion();
      region.SetIndex(origin);
      m_inputLabelImage->SetRegions(region);
    }


  if (this->m_nx + this->m_ny + this->m_nz == 0)
    {
      this->m_nx = size[0];
      this->m_ny = size[1];
      this->m_nz = size[2];
    }
  else if ( this->m_nx != (long)size[0] || this->m_ny != (long)size[1] || this->m_nz != (long)size[2] )
    {
      std::cerr<<"Error: image sizes do not match with label image size.\n";
      raise(SIGABRT);
    }

  return;
}




/* ============================================================  */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::inputLableImageToSeeds()
{
  typedef itk::ImageRegionConstIteratorWithIndex<TLabelImage> ImageRegionConstIteratorWithIndex_t;
  ImageRegionConstIteratorWithIndex_t it(m_inputLabelImage, m_inputLabelImage->GetLargestPossibleRegion() );
  it.GoToBegin();

  {
    std::vector<long> thisSeed(3);
    for (; !it.IsAtEnd(); ++it)
      {
        if (it.Get() != 0) // 0 for bkgd, every label else is obj
          {
            TIndex idx = it.GetIndex();
            thisSeed[0] = idx[0];
            thisSeed[1] = idx[1];
            thisSeed[2] = idx[2];
            m_seeds.push_back(thisSeed);
          }
      }
  }

  //sf.close();


  return;
}

/* ============================================================  */
template< typename TPixel > void CSFLSRobustStatSegmentor3DLabelMap< TPixel >::getThingsReady()
{
  /*
   1. Generate mp_mask from seeds
   2. Compute feature at each point
   3. Extract feature at/around the seeds
  */
  IFLOG("Gao3D_TimingDebug", cout << "Gao3D_TimingDebug: inputLableImageToSeeds()" << endl; )
  inputLableImageToSeeds(); //record into vector called m_seeds the i,j,k coordinates of all seed points

   IFLOG("Gao3D_TimingDebug", cout << "Gao3D_TimingDebug: seedToMask()" << endl; )
  seedToMask(); //make a binary mask showing all seed points and the neighbors (front,back, side to side, up,down) to 1

  IFLOG("Gao3D_TimingDebug", cout << "Gao3D_TimingDebug: initFeatureComputedImage()" << endl; )
  initFeatureComputedImage(); //make an all-zero image that keeps track of where we have computed features before


  IFLOG("Gao3D_TimingDebug", cout << "Gao3D_TimingDebug: initFeatureImage()" << endl; )
  initFeatureImage(); //make an array with "n" copies of an all-zero image where n=number of features we're using


  //computeFeature();
  IFLOG("Gao3D_TimingDebug", cout << "Gao3D_TimingDebug: getFeatureAroundSeeds()" << endl; )
  getFeatureAroundSeeds(); //compute features around all seed points, mark that we now have stats at this pixel

  IFLOG("Gao3D_TimingDebug", cout << "Gao3D_TimingDebug: estimateFeatureStdDevs" << endl; )
  estimateFeatureStdDevs(); //get std.dev. of features to determine kernel size

  IFLOG("Gao3D_TimingDebug", cout << "Gao3D_TimingDebug: estimatePDFs()" << endl; )
  estimatePDFs(); //

  IFLOG("Gao3D_TimingDebug", cout << "Gao3D_TimingDebug: Setup is done!" << endl; )
  return;
}


/* ============================================================ */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::initFeatureImage()
{
  if (!(this->mp_img))
    {
      std::cerr<<"Error: set input image first.\n";
      raise(SIGABRT);
    }


  for (long ifeature = 0; ifeature < m_numberOfFeature; ++ifeature)
    {
      TDoubleImagePointer fimg = TDoubleImage::New();
      fimg->SetRegions(this->mp_img->GetLargestPossibleRegion() );
      fimg->Allocate();
      fimg->CopyInformation(this->mp_img);

      m_featureImageList.push_back(fimg);
    }

  return;
}


/* ============================================================ */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::initFeatureComputedImage()
{
  if (!(this->mp_img))
    {
      std::cerr<<"Error: set input image first.\n";
      raise(SIGABRT);
    }


  m_featureComputed = TLabelImage::New();
  m_featureComputed->SetRegions(this->mp_img->GetLargestPossibleRegion());
  m_featureComputed->Allocate();
  m_featureComputed->CopyInformation(this->mp_img);
  m_featureComputed->FillBuffer(0);

  return;
}


/* ============================================================ */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::computeFeatureAt(TIndex idx, std::vector<double>& f)
{
  f.resize(m_numberOfFeature);

  if (m_featureComputed->GetPixel(idx))
    {
      // the feature at this pixel is computed, just retrive

      for (long i = 0; i < m_numberOfFeature; ++i)
        {
          f[i] = (m_featureImageList[i])->GetPixel(idx);
        }
    }
  else
    {
      // compute the feature
      std::vector< double > neighborIntensities;

      long ix = idx[0];
      long iy = idx[1];
      long iz = idx[2];

      for (long iiz = iz - m_statNeighborZ; iiz <= iz + m_statNeighborZ; ++iiz)
        {
          for (long iiy = iy - m_statNeighborY; iiy <= iy + m_statNeighborY; ++iiy)
            {
              for (long iix = ix - m_statNeighborX; iix <= ix + m_statNeighborX; ++iix)
                {
                  if (0 <= iix && iix < this->m_nx    \
                      && 0 <= iiy && iiy < this->m_ny    \
                      && 0 <= iiz && iiz < this->m_nz)
                    {
                      TIndex idxa = {{iix, iiy, iiz}};
                      neighborIntensities.push_back(this->mp_img->GetPixel(idxa)); //store all intensities in
                                                                                   //nbhd then compute nbhd stats
                    }
                }
            }
        }

      getRobustStatistics(neighborIntensities, f);

      for (long ifeature = 0; ifeature < m_numberOfFeature; ++ifeature)
        {
          m_featureImageList[ifeature]->SetPixel(idx, f[ifeature]);
        }
      
      m_featureComputed->SetPixel(idx, 1); // mark as computed
    }
  
  return;
}






/* ============================================================ */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::getRobustStatistics(std::vector<double>& samples, std::vector<double>& robustStat)
{
  /* note, sample is sorted, so the order is changed */
  robustStat.resize(m_numberOfFeature);

  std::sort(samples.begin(), samples.end() );

  double n = samples.size();

  double q1 = n/4.0;
  double q1_floor;
  double l1 = modf(q1, &q1_floor);

  double q2 = n/2.0;
  double q2_floor;
  double l2 = modf(q2, &q2_floor);

  double q3 = 3.0*n/4.0;
  double q3_floor;
  double l3 = modf(q3, &q3_floor);

  double median = (1 - l2)*samples[static_cast<long>(q2_floor)] + l2*samples[static_cast<long>(q2_floor) + 1];

  double iqr = ( (1 - l3)*samples[static_cast<long>(q3_floor)] + l3*samples[static_cast<long>(q3_floor) + 1] ) \
    - ( (1 - l1)*samples[static_cast<long>(q1_floor)] + l1*samples[static_cast<long>(q1_floor) + 1] );

  robustStat[0] = median;
  robustStat[1] = iqr;

  /* next compute MAD */
  long nn = samples.size();
  std::vector<double> samplesDeMedian(nn);
  for (long i = 0; i < nn; ++i)
    {
      samplesDeMedian[i] = fabs(samples[i] - median);
    }

  std::sort(samplesDeMedian.begin(), samplesDeMedian.end() );

  double mad = (1 - l2)*samplesDeMedian[static_cast<long>(q2_floor)] + l2*samplesDeMedian[static_cast<long>(q2_floor) + 1];  
  robustStat[2] = mad;


  return;
}


/* ============================================================ */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::seedToMask()
{
  if (!(this->mp_img))
    {
      std::cerr<<"Error: set input image first.\n";
      raise(SIGABRT);
    }


  if (this->mp_mask)
    {
      /* Sometimes, the mask is not corresponding to the seed, like
         using the mean shape as the mask and just some other seed as
         the samples for feature. In such cases, do not touch mask
         and just return. */

      return;
    }


  long n = m_seeds.size();
  if (n == 0)
    {
      std::cerr << "Error: No seeds specified." << std::endl;
      raise(SIGABRT);
    }


  this->mp_mask = TMaskImage::New();
  this->mp_mask->SetRegions(this->mp_img->GetLargestPossibleRegion());
  this->mp_mask->Allocate();
  this->mp_mask->CopyInformation(this->mp_img);
  this->mp_mask->FillBuffer(0);

  for (long i = 0; i < n; ++i)
    {
      if (3 != m_seeds[i].size())
        {
          std::cerr<<"Error: 3 != m_seeds[i].size() " << endl;
          raise(SIGABRT);
        }

      long ix = m_seeds[i][0];
      long iy = m_seeds[i][1];
      long iz = m_seeds[i][2];

      TIndex idx = {{ix, iy, iz}};
      this->mp_mask->SetPixel(idx, 1);

    }

  return;
}


/* ============================================================ */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::dilateSeeds()
{
  /* For each seed, add its 26 neighbors into the seed list. */

  if (!(this->mp_img))
    {
      std::cerr<<"Error: set input image first.\n";
      raise(SIGABRT);
    }


  long n = m_seeds.size();
  std::vector<std::vector<long> > newSeeds;

  if (n == 0)
    {
      std::cerr << "Error: No seeds specified." << std::endl;
      raise(SIGABRT);
    }


  for (long i = 0; i < n; ++i)
    {
      if (3 != m_seeds[i].size())
        {
          std::cerr<<"Error: 3 != m_seeds[i].size()\n";
          raise(SIGABRT);
        }

      long ix = m_seeds[i][0];
      long iy = m_seeds[i][1];
      long iz = m_seeds[i][2];

      for (long iiz = iz - 1; iiz <= iz + 1; ++iiz)
        {
          for (long iiy = iy - 1; iiy <= iy + 1; ++iiy)
            {
              for (long iix = ix - 1; iix <= ix + 1; ++iix)
                {
                  if (0 <= iix && iix < this->m_nx    \
                      && 0 <= iiy && iiy < this->m_ny    \
                      && 0 <= iiz && iiz < this->m_nz)
                    {
                      /* Some locations may be added multiple times,
                         if the original seeds are close. But I think
                         this is fine */

                      std::vector<long> s(3);
                      s[0] = iix;
                      s[1] = iiy;
                      s[2] = iiz;

                      newSeeds.push_back(s);
                    }
                }
            }
        }
    }
  
  m_seeds.assign(newSeeds.begin(), newSeeds.end() );

  return;
}


/* ============================================================  */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::getFeatureAroundSeeds()
{
    //m_featureImageList has d arrays m*n arrays m,n=imgDimensions , d=#features we're using and contains all
    //zeros at the beginnning

  if (!m_featureImageList[m_numberOfFeature-1])
    {
      // last feature image is not constructed
      std::cerr<<"Error: construct feature images first.\n";
      raise(SIGABRT);
    }


  long n = m_seeds.size();
  if (n == 0)
    {
      std::cerr << "Error: No seeds specified." << std::endl;
      raise(SIGABRT);
    }

  for (long i = 0; i < n; ++i)
    {
      if (3 != m_seeds[i].size())
        {
          std::cerr<<"Error: 3 != m_seeds[i].size()\n";
          raise(SIGABRT);
        }

      long ix = m_seeds[i][0];
      long iy = m_seeds[i][1];
      long iz = m_seeds[i][2];

      TIndex idx = {{ix, iy, iz}};
                      
      std::vector<double> featureHere(m_numberOfFeature); // make a d dimensional feature vector at this pixel location
      computeFeatureAt(idx, featureHere);


      m_featureAtTheSeeds.push_back(featureHere);//needed to quickly determine kernel std.dev



    }


  return;
}

/* ============================================================ */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::estimateFeatureStdDevs()
{
  m_kernelStddev.assign(m_numberOfFeature, 0.0);
  
  long n = m_seeds.size(); // == m_featureAtTheSeeds.size()

  for (long i = 0; i < m_numberOfFeature; ++i)
    {
      double m = 0;
      for (long ii = 0; ii < n; ++ii)
        {
          m += m_featureAtTheSeeds[ii][i];
        }
      m /= n;

      for (long ii = 0; ii < n; ++ii)
        {
          m_kernelStddev[i] += (m_featureAtTheSeeds[ii][i] - m)*(m_featureAtTheSeeds[ii][i] - m);
        }

      m_kernelStddev[i] /= (n-1);
      m_kernelStddev[i] = sqrt(m_kernelStddev[i]);
    }

  return;
}

template< typename TPixel >
double
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::kernelEvaluationUsingPDF(const std::vector<double>& newFeature)
{
  double p = 1;

  for (long i = 0; i < m_numberOfFeature; ++i)
    {
      long idx = static_cast<long>(newFeature[i] - m_inputImageIntensityMin);

      double probOfThisFeature = m_PDFlearnedFromSeeds[i][idx];

      p *= probOfThisFeature;
    }

  return p;
}

/* ============================================================  */
template< typename TPixel >
double
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::kernelEvaluation(const std::vector<double>& newFeature)
{
  long n = m_seeds.size(); // == m_featureAtTheSeeds.size()

  double p = 1;
  //double p = 0;

  for (long i = 0; i < m_numberOfFeature; ++i)
    {
      double pp = 0.0;

      double stdDev = m_kernelStddev[i]/m_kernelWidthFactor; // /10 as in Eric's appendix

      double var2 = -1.0/(2*stdDev*stdDev);
      double c = 1.0/sqrt(2*(vnl_math::pi))/stdDev;

      for (long ii = 0; ii < n; ++ii)
        {
          pp += exp(var2*(newFeature[i] - m_featureAtTheSeeds[ii][i])*(newFeature[i] - m_featureAtTheSeeds[ii][i]));
        }
      
      pp *= c;
      pp /= n;

      p *= pp;
    }

  return p;
}


/* ============================================================  */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::setKernelWidthFactor(double f)
{
  if (f < 0.1)
    {
      m_kernelWidthFactor = 0.1;
    }

  if (f > 20.0)
    {
      m_kernelWidthFactor = 20.0;
    }

  m_kernelWidthFactor = f;



  return;
}


/* ============================================================  */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::setIntensityHomogeneity(double h)
{

  double f = h*(20.0 - 0.1) + 0.1;

  setKernelWidthFactor(f);

  return;
}

/* ============================================================  */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::estimatePDFs()
{
  m_PDFlearnedFromSeeds.clear();
  
  computeMinMax(); // compute min, max intensity on the original image

  long number_of_seeds = m_seeds.size();


  for (long ifeature = 0; ifeature < m_numberOfFeature; ++ifeature)
  {
    // TODO: this pdf has way too much dynamic range, we don't need this many entries!!!!
    std::vector<double> thisPDF(m_inputImageIntensityMax - m_inputImageIntensityMin + 1);
    // assumption: TPixel are of integer types.

    double stdDev = m_kernelStddev[ifeature]/m_kernelWidthFactor; // /10 as in Eric's appendix

// TODO: replace this manual routine with something efficient. See cv::Histogram for example. Fast and blazing.

    double var2 = -1.0/(2*stdDev*stdDev);
    double c = 1.0/sqrt(2*(vnl_math::pi))/stdDev;

    for (TPixel a = m_inputImageIntensityMin; a <= m_inputImageIntensityMax; ++a)
      {
        long ia = (a - m_inputImageIntensityMin);

        double pp = 0.0;
        int seeds_used = number_of_seeds;
        int SEEDS_MAX = 1e2 ; // TODO: this is a good GUI param ?!
        seeds_used     = ( seeds_used > SEEDS_MAX ) ?  SEEDS_MAX : seeds_used ;
        for (long ii = 0; ii < number_of_seeds; ii=ii+100 ) {
            int idx = rand() % number_of_seeds ;
            double voxel_value = (a - m_featureAtTheSeeds[idx][ifeature]);
            pp += exp( var2 * voxel_value * voxel_value );
        }
        pp *= c;
        pp /= seeds_used;


        thisPDF[ia] = pp;
      }

    m_PDFlearnedFromSeeds.push_back(thisPDF);
  }


  return;
}


/* ============================================================  */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::computeMinMax()
{
  if (!(this->mp_img))
    {
      std::cerr<<"Error: set input image first.\n";
      raise(SIGABRT);
    }

  typedef itk::Image<TPixel, 3> itkImage_t;

  typedef itk::ImageRegionConstIterator<itkImage_t> itkImageRegionConstIterator_t;

  itkImageRegionConstIterator_t it((this->mp_img), (this->mp_img)->GetLargestPossibleRegion() );
  it.GoToBegin();

  m_inputImageIntensityMin = std::numeric_limits<unsigned>::max(); // yes, it's twisted so easity to compute.
  m_inputImageIntensityMax = std::numeric_limits<unsigned>::min();

  for (; !it.IsAtEnd(); ++it)
    {
      TPixel v = it.Get();
      
      m_inputImageIntensityMin = m_inputImageIntensityMin<v?m_inputImageIntensityMin:v;
      m_inputImageIntensityMax = m_inputImageIntensityMax>v?m_inputImageIntensityMax:v;
    }

  return;
}



template< typename TPixel >
void CSFLSRobustStatSegmentor3DLabelMap< TPixel >::computeMeans(){
    long numElements= this->m_nx * this->m_ny * this->m_nz;
    this->sumin=0;
    this->sumout=0;

    typedef itk::ImageRegionConstIterator<ImageType> imgIter;
    imgIter itIm( (this->mp_img), (this->mp_img)->GetRequestedRegion() );
    itIm.GoToBegin();

    typedef itk::ImageRegionConstIterator<LSImageType> phiIter;
    phiIter itPhi( (this->mp_phi), (this->mp_phi)->GetRequestedRegion() );
    itPhi.GoToBegin();

    //TPixel I=//this->mp_img->GetBufferPointer(); //the value of the voxel at the current index
    //float phi=//this->mp_phi->GetBufferPointer(); //TODO: fix the type to get templated type

    for (itIm; !itIm.IsAtEnd(); ++itIm)
    {
        //this->mp_img->GetPixel(i);
        if(itPhi.Get()<=0){
            this->sumin= this->sumin + itIm.Get();
        }
        else{
            this->sumout=this->sumout + itIm.Get();
        }
        ++itPhi;
    }

    this->uin=this->sumin/this->m_insideVoxelCount; //m_insideVoxelCount gets updated when pixels move in/out of Lp2,Lm2
    this->uout=this->sumout/(numElements -this->m_insideVoxelCount);
    cout<<"The mean inside is " <<this->uin<<endl;
    cout<<"The mean outside is " <<this->uout<<endl;
}

template< typename TPixel >
void CSFLSRobustStatSegmentor3DLabelMap< TPixel >::updateMeans(){
  typename CSFLSLayer::iterator iteratorIn = this->m_lOut2in.begin();
  typename CSFLSLayer::iterator iteratorOut = this->m_lIn2out.begin();
  long numElements= this->m_nx * this->m_ny * this->m_nz;



  for ( iteratorIn; iteratorIn != this->m_lOut2in.end(); ++iteratorIn ){
    long ix = (*iteratorIn)[0];
    long iy = (*iteratorIn)[1];
    long iz = (*iteratorIn)[2];
    TIndex idx = {{ix, iy, iz}};

    this->sumin=this->sumin + this->mp_img->GetPixel(idx);
    this->sumout=this->sumout - this->mp_img->GetPixel(idx);
  }

  for ( iteratorOut; iteratorOut != this->m_lIn2out.end(); ++iteratorOut  ){
    long ix = (*iteratorOut)[0];
    long iy = (*iteratorOut)[1];
    long iz = (*iteratorOut)[2];
    TIndex idx = {{ix, iy, iz}};

    this->sumin=this->sumin - this->mp_img->GetPixel(idx);
    this->sumout=this->sumout + this->mp_img->GetPixel(idx);
  }
    this->uin=this->sumin/this->m_insideVoxelCount; //m_insideVoxelCount gets updated when pixels move in/out of Lp2,Lm2
    this->uout=this->sumout/(numElements -this->m_insideVoxelCount);

    cout<<"The difference of the means is " << fabs(this->uin - this->uout)<<endl;
}

template< typename TPixel >
void CSFLSRobustStatSegmentor3DLabelMap< TPixel >::computeHist(){
    long numElements= this->m_nx * this->m_ny * this->m_nz;

    this->computeMinMax();
    this->binWidth=(this->m_inputImageIntensityMax - this->m_inputImageIntensityMin)/this->m_numberOfHistBins;

    this->histIn = new double[ this->m_numberOfHistBins ];
    this->histOut = new double[ this->m_numberOfHistBins ];


    this->cumSumIn=0;
    this->cumSumOut=0;


    int currBin;

    for (int i=0; i<this->m_numberOfHistBins;i++){
        this->histIn[i]=0;
        this->histOut[i]=0;
    }


    typedef itk::ImageRegionConstIterator<ImageType> imgIter;
    imgIter itIm( (this->mp_img), (this->mp_img)->GetRequestedRegion() );
    itIm.GoToBegin();

    typedef itk::ImageRegionConstIterator<LSImageType> phiIter;
    phiIter itPhi( (this->mp_phi), (this->mp_phi)->GetRequestedRegion() );
    itPhi.GoToBegin();

    for (itIm; !itIm.IsAtEnd(); ++itIm)
    {
        //this->mp_img->GetPixel(i);
        currBin=std::floor((itIm.Get() - this->m_inputImageIntensityMin)/this->binWidth);
        if(itPhi.Get()<=0){
            this->histIn[currBin]++;
            this->cumSumIn++;
        }
        else{
            this->histOut[currBin]++;
            this->cumSumOut++;
        }
        ++itPhi;
    }

}

template< typename TPixel >
void CSFLSRobustStatSegmentor3DLabelMap< TPixel >::updateHist(){
  typename CSFLSLayer::iterator iteratorIn = this->m_lOut2in.begin();
  typename CSFLSLayer::iterator iteratorOut = this->m_lIn2out.begin();
  long numElements= this->m_nx * this->m_ny * this->m_nz;
  int currBin;


  for ( iteratorIn; iteratorIn != this->m_lOut2in.end(); ++iteratorIn ){
    long ix = (*iteratorIn)[0];
    long iy = (*iteratorIn)[1];
    long iz = (*iteratorIn)[2];
    TIndex idx = {{ix, iy, iz}};

    TPixel currPix=this->mp_img->GetPixel(idx); //this is the pixel coming inside contour
    currBin=std::floor((currPix - this->m_inputImageIntensityMin)/this->binWidth);
    this->histIn[currBin]++;
    this->histOut[currBin]--;
    this->cumSumIn++;
    this->cumSumOut--;

  }

  for ( iteratorOut; iteratorOut != this->m_lIn2out.end(); ++iteratorOut  ){
    long ix = (*iteratorOut)[0];
    long iy = (*iteratorOut)[1];
    long iz = (*iteratorOut)[2];
    TIndex idx = {{ix, iy, iz}};

    TPixel currPix=this->mp_img->GetPixel(idx); //this is the pixel coming inside contour
    currBin=std::floor((currPix - this->m_inputImageIntensityMin)/this->binWidth);
    this->histIn[currBin]--;
    this->histOut[currBin]++;
    this->cumSumIn--;
    this->cumSumOut++;
  }

}

/* =========ALL IMPLEMENTED ENEGRIES ARE BELOW=================  */
/* =========**********************************=================  */
/* ============================================================  */

template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::Segmentation_rss()
{
  double startingTime = clock();



  getThingsReady(); //this is only for RSS setup

  IFLOG("Gao3D_WriteAlways",
    std::ofstream f("/tmp/d.txt", std::ios_base::app);
    f<<"m_maxRunningTime = "<<this->m_maxRunningTime<<std::endl;
    f.close(); )

  /*============================================================
   * From the initial mask, generate: 1. SFLS, 2. mp_label and
   * 3. mp_phi.
   */

  IFLOG("Gao3D_TimingDebug", cout<<"Gao3D_TimingDebug: initializeSFLS()" << endl; )
  this->initializeSFLS(); //need this for all segmentations

  for (unsigned int it = 0; it < this->m_numIter; ++it)
  {
      std::cout<<"In iteration "<<it<<std::endl<<std::flush;

      // keep current zero contour as history is required
      if (this->m_keepZeroLayerHistory)
      {
          (this->m_zeroLayerHistory).push_back(this->m_lz);
      }

      double oldVoxelCount = this->m_insideVoxelCount;

      ComputeForce_rss();

      this->normalizeForce();

      this->oneStepLevelSetEvolution();

      /*********If the inside physical volume exceed expected volume, stop******/
      double volumeIn = (this->m_insideVoxelCount)*(this->m_dx)*(this->m_dy)*(this->m_dz);
      if (volumeIn > (this->m_maxVolume))
        {
          std::ofstream f("/tmp/o.txt");
          f<<"In the "<<it<<"-th iteration\n";
          f<<"reach max volume\n";

          f<<"m_maxVolume = "<<this->m_maxVolume<<std::endl;
          f<<"volumeIn = "<<volumeIn<<std::endl;

          f.close();

          break;
        }
      /*********If the inside physical volume exceed expected volume, stop******/


      double ellapsedTime = (clock() - startingTime)/static_cast<double>(CLOCKS_PER_SEC);
      if (ellapsedTime > (this->m_maxRunningTime))
        {
          std::ofstream f("/tmp/o.txt");
          f<<"running time = "<<ellapsedTime<<std::endl;
          f<<"m_maxRunningTime = "<<this->m_maxRunningTime<<std::endl;
          f.close();

          break;
        }


    }

  return;
}


/* =========================================================================================================  */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::ComputeForce_rss()
{
  double fmax = std::numeric_limits<double>::min();
  double kappaMax = std::numeric_limits<double>::min();

  long n = this->m_lz.size();
  double* kappaOnZeroLS = new double[ n ];
  double* cvForce = new double[ n ];


  std::vector<typename CSFLSLayer::iterator> m_lzIterVct( n );
  {
    long iiizzz = 0;
    for (typename CSFLSLayer::iterator itz = this->m_lz.begin(); itz != this->m_lz.end(); ++itz)
      m_lzIterVct[iiizzz++] = itz;
  }

  for (long i = 0; i < n; ++i)
    {
      typename CSFLSLayer::iterator itz = m_lzIterVct[i];

      long ix = (*itz)[0];
      long iy = (*itz)[1];
      long iz = (*itz)[2];

      TIndex idx = {{ix, iy, iz}};

      kappaOnZeroLS[i] = this->computeKappa(ix, iy, iz);

      std::vector<double> f(m_numberOfFeature);

      computeFeatureAt(idx, f);

      //double a = -kernelEvaluation(f);
      double a = -kernelEvaluationUsingPDF(f);

      fmax = fmax>fabs(a)?fmax:fabs(a);
      kappaMax = kappaMax>fabs(kappaOnZeroLS[i])?kappaMax:fabs(kappaOnZeroLS[i]);

      cvForce[i] = a;
    }


  //std::cout<<"fmax = "<<fmax<<std::endl;

    this->m_force.resize(n);
    for (long i = 0; i < n; ++i)
      {
        //this->m_force.push_back(cvForce[i]/(fmax + 1e-10) +  (this->m_curvatureWeight)*kappaOnZeroLS[i]);
        this->m_force[i] = (1 - (this->m_curvatureWeight))*cvForce[i]/(fmax + 1e-10) \
          +  (this->m_curvatureWeight)*kappaOnZeroLS[i]/(kappaMax + 1e-10);
      }


    delete[] kappaOnZeroLS;
    delete[] cvForce;
}

/* =========================================================================================================  */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::Segmentation_chan_vese()
{
  double startingTime = clock();

  //getThingsReady(); //this is only for RSS setup

  std::ofstream f("/tmp/d.txt", std::ios_base::app);
  f<<"m_maxRunningTime = "<<this->m_maxRunningTime<<std::endl;
  f.close();

  inputLableImageToSeeds(); //record into vector called m_seeds the i,j,k coordinates of all seed points 
  seedToMask(); //make a binary mask showing all seed points and the neighbors (front,back, side to side, up,down) to 1

        typedef itk::ImageFileWriter<TMaskImage> WriterType2;
        typename WriterType2::Pointer outputWriter2 = WriterType2::New();
        outputWriter2->SetFileName("itkFormat_seedToMask.mha");
        outputWriter2->SetInput(this->mp_mask);
        outputWriter2->Update();
        outputWriter2->Write();


  /*============================================================
   * From the initial mask, generate: 1. SFLS, 2. mp_label and
   * 3. mp_phi.
   */
  this->initializeSFLS(); //need this for all segmentations

  computeMeans();
  //douher::saveAsImage2< double >(mp_phi, "initPhi.nrrd");

  for (unsigned int it = 0; it < this->m_numIter; ++it)
  {
      std::cout<<"In iteration "<<it<<std::endl<<std::flush;

      // keep current zero contour as history is required
      if (this->m_keepZeroLayerHistory)
      {
          (this->m_zeroLayerHistory).push_back(this->m_lz);
      }

      double oldVoxelCount = this->m_insideVoxelCount;

      updateMeans();
      ComputeForce_chan_vese();

      this->normalizeForce();

      this->oneStepLevelSetEvolution();

    }

  return;
}

/* =========================================================================================================  */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::ComputeForce_chan_vese()
{
  double fmax = std::numeric_limits<double>::min();
  double kappaMax = std::numeric_limits<double>::min();

  long n = this->m_lz.size();
  double* kappaOnZeroLS = new double[ n ];
  double* cvForce = new double[ n ];
  TPixel I; //the value of the voxel at the current index

  std::vector<typename CSFLSLayer::iterator> m_lzIterVct( n );
  {
    long iiizzz = 0;
    for (typename CSFLSLayer::iterator itz = this->m_lz.begin(); itz != this->m_lz.end(); ++itz )
      m_lzIterVct[iiizzz++] = itz; //take out nodes in a linked list and store them
  }

  for (long i = 0; i < n; ++i)  //n is the number of voxels on the zero level set
    {
      typename CSFLSLayer::iterator itz = m_lzIterVct[i];

      long ix = (*itz)[0];
      long iy = (*itz)[1];
      long iz = (*itz)[2];

      TIndex idx = {{ix, iy, iz}};

      kappaOnZeroLS[i] = this->computeKappa(ix, iy, iz);

      I=this->mp_img->GetPixel(idx);
      double a=(I - this->uin)*(I - this->uin) - (I - this->uout)*(I - this->uout);

      fmax = fmax>fabs(a)?fmax:fabs(a);
      kappaMax = kappaMax>fabs(kappaOnZeroLS[i])?kappaMax:fabs(kappaOnZeroLS[i]);

      cvForce[i] = a;
    }

    this->m_force.resize(n);
    for (long i = 0; i < n; ++i)
      {
        this->m_force[i] = (1 - (this->m_curvatureWeight))*cvForce[i]/(fmax + 1e-10) \
          +  (this->m_curvatureWeight)*kappaOnZeroLS[i]/(kappaMax + 1e-10);
      }


    delete[] kappaOnZeroLS;
    delete[] cvForce;
}



/* =========================================================================================================  */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::Segmentation_bhattacharyya()
{
  double startingTime = clock();

  //getThingsReady(); //this is only for RSS setup





  std::ofstream f("/tmp/d.txt", std::ios_base::app);
  f<<"m_maxRunningTime = "<<this->m_maxRunningTime<<std::endl;
  f.close();



  inputLableImageToSeeds(); //record into vector called m_seeds the i,j,k coordinates of all seed points
  seedToMask(); //make a binary mask showing all seed points and the neighbors (front,back, side to side, up,down) to 1

  /*============================================================
   * From the initial mask, generate: 1. SFLS, 2. mp_label and
   * 3. mp_phi.
   */
  this->initializeSFLS(); //need this for all segmentations

  computeHist();

  for (unsigned int it = 0; it < this->m_numIter; ++it)
  {
      std::cout<<"In iteration "<<it<<std::endl<<std::flush;

      // keep current zero contour as history is required
      if (this->m_keepZeroLayerHistory)
      {
          (this->m_zeroLayerHistory).push_back(this->m_lz);
      }

      double oldVoxelCount = this->m_insideVoxelCount;

      updateHist();
      ComputeForce_bhattacharyya();

      this->normalizeForce();

      this->oneStepLevelSetEvolution();

    }

  return;
}

/* =========================================================================================================  */
template< typename TPixel >
void
CSFLSRobustStatSegmentor3DLabelMap< TPixel >
::ComputeForce_bhattacharyya()
{
  double fmax = std::numeric_limits<double>::min();
  double kappaMax = std::numeric_limits<double>::min();

  long n = this->m_lz.size();
  double* kappaOnZeroLS = new double[ n ];
  double* cvForce = new double[ n ];
  TPixel I; //the value of the voxel at the current index


  double B=0;
  for (int j=0;j<this->m_numberOfHistBins;j++){
      B=B+sqrt( (this->histIn[j]/this->cumSumIn)*(this->histOut[j]/this->cumSumOut) );
  }

  std::cout<< "the B is : " << B << std::endl;

  double p,q;
  double integral;
  double lookupCompute[this->m_numberOfHistBins];
  for (int j=0;j<this->m_numberOfHistBins;j++){
      p=this->histIn[j]/this->cumSumIn + 0.000000001;
      q=this->histOut[j]/this->cumSumOut + 0.000000001;
      integral= sqrt(p/q)/this->cumSumOut - sqrt(q/p)/this->cumSumIn;
      lookupCompute[j]=B/2*(1/this->cumSumIn - 1/this->cumSumOut) + integral/2;
  }

  std::vector<typename CSFLSLayer::iterator> m_lzIterVct( n );
  {
    long iiizzz = 0;
    for (typename CSFLSLayer::iterator itz = this->m_lz.begin(); itz != this->m_lz.end(); ++itz )
      m_lzIterVct[iiizzz++] = itz; //take out nodes in a linked list and store them
  }

  int currBin;
  for (long i = 0; i < n; ++i)  //n is the number of voxels on the zero level set
    {
      typename CSFLSLayer::iterator itz = m_lzIterVct[i];

      long ix = (*itz)[0];
      long iy = (*itz)[1];
      long iz = (*itz)[2];

      TIndex idx = {{ix, iy, iz}};

      kappaOnZeroLS[i] = this->computeKappa(ix, iy, iz);

      //std::vector<double> f(m_numberOfFeature);

      I=this->mp_img->GetPixel(idx);
      currBin=std::floor( ( I - this->m_inputImageIntensityMin)/this->binWidth);
      double a=-lookupCompute[currBin];


      //computeFeatureAt(idx, f);
      //double a = -kernelEvaluation(f);
      //double a = -kernelEvaluationUsingPDF(f);




      fmax = fmax>fabs(a)?fmax:fabs(a);
      kappaMax = kappaMax>fabs(kappaOnZeroLS[i])?kappaMax:fabs(kappaOnZeroLS[i]);

      cvForce[i] = a;
    }


  //std::cout<<"fmax = "<<fmax<<std::endl;

    this->m_force.resize(n);
    for (long i = 0; i < n; ++i)
      {
        //this->m_force.push_back(cvForce[i]/(fmax + 1e-10) +  (this->m_curvatureWeight)*kappaOnZeroLS[i]);
        this->m_force[i] = (1 - (this->m_curvatureWeight))*cvForce[i]/(fmax + 1e-10) \
          +  (this->m_curvatureWeight)*kappaOnZeroLS[i]/(kappaMax + 1e-10);
      }


    delete[] kappaOnZeroLS;
    delete[] cvForce;
}


#endif