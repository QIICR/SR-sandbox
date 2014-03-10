// STL includes
#include <iostream>
#include <string>
#include <vector>

#define QIICR_UID_ROOT "1.3.6.1.4.1.43046.3"
#define QIICR_IMPLEMENTATION_CLASS_UID QIICR_UID_ROOT ".0.1"
#define QIICR_CODING_SCHEME_UID_ROOT QIICR_UID_ROOT ".0.0"

#define QIICR_DEVICE_OBSERVER_UID QIICR_IMPLEMENTATION_CLASS_UID ".99.1" // use .99 for sandbox code examples

#define SITE_UID_ROOT QIICR_UID_ROOT

// DCMTK includes
#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/dcmsr/dsrdoc.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmsr/dsriodcc.h"
#include "dcmHelpersCommon.h"

#define WARN_IF_ERROR(FunctionCall,Message) if(!FunctionCall) std::cout << "Return value is 0 for " << Message << std::endl;

int getReferencedInstances(DcmDataset* dataset,
                            std::vector<std::string> &classUIDs,
                            std::vector<std::string> &instanceUIDs);

int main(int argc, char** argv)
{
  char* segFileName = argv[1];

  char* imageFileName = argv[2];

  std::vector<std::string> referencedImages;
  for(int i=2;i<argc;i++){
    referencedImages.push_back(argv[i]);
  }

  DcmFileFormat *fileformatSR = new DcmFileFormat();
  DcmFileFormat *fileformatSEG = new DcmFileFormat();
  DcmFileFormat *fileformatImage = new DcmFileFormat();

  DcmElement *e;

  std::vector<std::string> referencedClassUIDs, referencedInstanceUIDs;
  std::string referencedStudyInstanceUID;

  // read the image
  fileformatImage->loadFile(imageFileName);
  DcmDataset *datasetImage = fileformatImage->getDataset();
  char *imageSeriesInstanceUIDPtr;
  datasetImage->findAndGetElement(DCM_SeriesInstanceUID,
                                  e);
  e->getString(imageSeriesInstanceUIDPtr);

  // read SEG and find out the study, series and instance UIDs
  //  of the source images used for segmentation
  fileformatSEG->loadFile(segFileName);
  DcmDataset *datasetSEG = fileformatSEG->getDataset();
  if(!getReferencedInstances(datasetSEG, referencedClassUIDs, referencedInstanceUIDs)){
      std::cerr << "Failed to find references to the source image" << std::endl;
      return -1;
  }

  char* segInstanceUIDPtr;
  datasetSEG->findAndGetElement(DCM_SOPInstanceUID, e);
  e->getString(segInstanceUIDPtr);

  DcmDataset *datasetSR = fileformatSR->getDataset();

  /*
   * Comprehensive SR IOD Modules
   *
   * Patient:
   *            Patient                     M
   *            Clinical Trial Subject      U
   * Study:
   *            General Study               M
   *            Patient Study               U
   *            Clinical Trial Study        U
   * Series:
   *            SR Document Series          M
   *            Clinical Trial Series       U
   * Frame of reference:
   *            Frame of Reference          U
   *            Synchronization             U
   * Equipment:
   *            General Equipment           M
   * Document:
   *            SR Document General         M
   *            SR Document Content         M
   *            SOP Common                  M
   */

  OFString reportUID;
  OFStatus status;

  DSRDocument *doc = new DSRDocument();

  size_t node;
  
  //

  // TODO: IF SOMETHING DOESN'T WORK - CHECK THE RETURN VALUE!!!

  // create root document
  doc->createNewDocument(DSRTypes::DT_ComprehensiveSR);
  doc->setSeriesDescription("ROI quantitative measurement");

  // add incofmation about the template used - see CP-452
  //  ftp://medical.nema.org/medical/dicom/final/cp452_ft.pdf
  node = doc->getTree().addContentItem(DSRTypes::RT_isRoot, DSRTypes::VT_Container);
  doc->getTree().getCurrentContentItem().setTemplateIdentification(
              "1000", "99QIICR");

  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("10001","99QIICR", "Quantitative measurement report"));

  // TID1204: Language of content item and descendants
  dcmHelpersCommon::addLanguageOfContent(doc);

  // TID 1001: Observation context
  //dcmHelpersCommon::addObservationContext(doc);
  // TODO: replace device observer name with git repository, and model name with the git hash
  dcmHelpersCommon::addObserverContext(doc, QIICR_DEVICE_OBSERVER_UID, "tid1411test",
                                      "QIICR", "0.0.1", "0");

  // TID 4020: Image library
  //  at the same time, add all referenced instances to CurrentRequestedProcedureEvidence sequence
  node = doc->getTree().addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Container, DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("111028", "DCM", "Image Library"));
  for(int i=0;i<referencedImages.size();i++){
    DcmFileFormat *fileFormat = new DcmFileFormat();
    fileFormat->loadFile(referencedImages[i].c_str());    
    dcmHelpersCommon::addImageLibraryEntry(doc, fileFormat->getDataset());

    doc->getCurrentRequestedProcedureEvidence().addItem(*fileFormat->getDataset());
  }
  doc->getCurrentRequestedProcedureEvidence().addItem(*datasetSEG);


  WARN_IF_ERROR(doc->getTree().addContentItem(DSRTypes::RT_contains,
                                              DSRTypes::VT_Container,
                                              DSRTypes::AM_afterCurrent),
                "Findings container");
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121070","DCM","Findings"));

  // TID 1411
  doc->getTree().addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Container, DSRTypes::AM_belowCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(DSRCodedEntryValue("125007","DCM","Measurement Group"));

  //
  node = doc->getTree().addContentItem(
              DSRTypes::RT_hasObsContext, DSRTypes::VT_Text, DSRTypes::AM_belowCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("112039","DCM","Tracking Identifier"));
  doc->getTree().getCurrentContentItem().setStringValue("Object1");

  //
  node = doc->getTree().addContentItem(
              DSRTypes::RT_hasObsContext, DSRTypes::VT_UIDRef, DSRTypes::AM_afterCurrent);

  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("112040","DCM","Tracking Unique Identifier"));
  char trackingUID[128];
  dcmGenerateUniqueIdentifier(trackingUID, SITE_INSTANCE_UID_ROOT);
  doc->getTree().getCurrentContentItem().setStringValue(trackingUID);

  node = doc->getTree().addContentItem(
              DSRTypes::RT_contains, DSRTypes::VT_Image, DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121191","DCM","Referenced Segment"));

  DSRImageReferenceValue segReference = DSRImageReferenceValue(UID_SegmentationStorage, segInstanceUIDPtr);
  segReference.getSegmentList().addItem(1);
  if(doc->getTree().getCurrentContentItem().setImageReference(segReference).bad()){
    std::cerr << "Failed to set segmentation image reference" << std::endl;
  }

  // Referenced series used for segmentation is not stored in the
  // segmentation object, so need to reference all images instead.
  // Can initialize if the source images are available.
  for(int i=0;i<referencedInstanceUIDs.size();i++){
    node = doc->getTree().addContentItem(
                DSRTypes::RT_contains, DSRTypes::VT_Image,
                DSRTypes::AM_afterCurrent);
    doc->getTree().getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("121233","DCM","Source image for segmentation"));
    DSRImageReferenceValue imageReference =
            DSRImageReferenceValue(referencedClassUIDs[i].c_str(),referencedInstanceUIDs[i].c_str());
    if(doc->getTree().getCurrentContentItem().setImageReference(imageReference).bad()){
      std::cerr << "Failed to set source image reference" << std::endl;
    }
  }

  // Measurement container: TID 1419
  DSRNumericMeasurementValue measurement =
    DSRNumericMeasurementValue("70.978",
    DSRCodedEntryValue("[hnsf'U]","UCUM","Hounsfield unit"));

  doc->getTree().addContentItem(DSRTypes::RT_contains,
                                DSRTypes::VT_Num,
                                DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setNumericValue(measurement);

  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("112031","DCM","Attenuation Coefficient"));

  WARN_IF_ERROR(doc->getTree().addContentItem(DSRTypes::RT_hasConceptMod,
                                DSRTypes::VT_Code,
                                DSRTypes::AM_belowCurrent), "Failed to add code");
  assert(doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121401","DCM","Derivation")).good());
  assert(doc->getTree().getCurrentContentItem().setCodeValue(
              DSRCodedEntryValue("R-00317","SRT","Mean")).good());

  OFString contentDate, contentTime;
  DcmDate::getCurrentDate(contentDate);
  DcmTime::getCurrentTime(contentTime);

  // Note: ContentDate/Time are populated by DSRDocument
  doc->setSeriesDate(contentDate.c_str());
  doc->setSeriesTime(contentTime.c_str());

  doc->getCodingSchemeIdentification().addItem("99QIICR");
  doc->getCodingSchemeIdentification().setCodingSchemeUID(QIICR_CODING_SCHEME_UID_ROOT);
  doc->getCodingSchemeIdentification().setCodingSchemeName("QIICR Coding Scheme");
  doc->getCodingSchemeIdentification().setCodingSchemeResponsibleOrganization("Quantitative Imaging for Cancer Research, http://qiicr.org");

  doc->write(*datasetSR);

  dcmHelpersCommon::copyPatientModule(datasetImage,datasetSR);
  dcmHelpersCommon::copyPatientStudyModule(datasetImage,datasetSR);
  dcmHelpersCommon::copyGeneralStudyModule(datasetImage,datasetSR);

  fileformatSR->saveFile("report.dcm", EXS_LittleEndianExplicit);

  return 0;
}


int getReferencedInstances(DcmDataset* dataset,
                            std::vector<std::string> &classUIDs,
                            std::vector<std::string> &instanceUIDs){
  DcmItem *item, *subitem1, *subitem2;

  if(dataset->findAndGetSequenceItem(DCM_SharedFunctionalGroupsSequence, item).bad()){
    std::cerr << "Input segmentation does not contain SharedFunctionalGroupsSequence" << std::endl;
    return 0;
  }
  if(item->findAndGetSequenceItem(DCM_DerivationImageSequence, subitem1).bad()){
      std::cerr << "Input segmentation does not contain DerivationImageSequence" << std::endl;
      return 0;
  }
  if(subitem1->findAndGetSequenceItem(DCM_SourceImageSequence, subitem2).bad()){
      std::cerr << "Input segmentation does not contain SourceImageSequence" << std::endl;
      return 0;
  }

  int itemNumber = 0;
  while(subitem1->findAndGetSequenceItem(DCM_SourceImageSequence, subitem2, itemNumber++).good()){
    DcmElement *e;
    char *str;
    subitem2->findAndGetElement(DCM_ReferencedSOPClassUID, e);
    e->getString(str);
    classUIDs.push_back(str);

    subitem2->findAndGetElement(DCM_ReferencedSOPInstanceUID, e);
    e->getString(str);
    instanceUIDs.push_back(str);
  }

  return classUIDs.size();
}
