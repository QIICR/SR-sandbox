// STL includes
#include <iostream>
#include <string>
#include <vector>

// DCMTK includes
#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/dcmsr/dsrdoc.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmsr/dsriodcc.h"

#define WARN_IF_ERROR(FunctionCall,Message) if(!FunctionCall) std::cout << "Return value is 0 for " << Message << std::endl;

int getReferencedInstances(DcmDataset* dataset,
                            std::vector<std::string> &classUIDs,
                            std::vector<std::string> &instanceUIDs);

void copyDcmElement(const DcmTag& tag, DcmDataset* dcmIn, DcmDataset* dcmOut);
std::string getDcmElementAsString(const DcmTag& tag, DcmDataset* dcmIn);
void addImageLibraryEntry(DSRDocument *doc, std::string imageFileName);
bool findAndGetCodedValueFromSequenceItem(DcmItem *item, DSRCodedEntryValue value);

int main(int argc, char** argv)
{
  char* segFileName = argv[1];
  char* imageFileName = argv[2];
  char* csvFileName = argv[3];

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

  std::string segStudyUIDStr, segSeriesUIDStr, segInstanceUIDStr;
  segStudyUIDStr = getDcmElementAsString(DCM_StudyInstanceUID, datasetSEG);
  segSeriesUIDStr = getDcmElementAsString(DCM_SeriesInstanceUID, datasetSEG);
  segInstanceUIDStr = getDcmElementAsString(DCM_SOPInstanceUID, datasetSEG);

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

  doc->setPatientName(getDcmElementAsString(DCM_PatientName, datasetSEG).c_str());
  doc->setPatientSex(getDcmElementAsString(DCM_PatientSex, datasetSEG).c_str());
  doc->setPatientID(getDcmElementAsString(DCM_PatientID, datasetSEG).c_str());


  // add incofmation about the template used - see CP-452
  //  ftp://medical.nema.org/medical/dicom/final/cp452_ft.pdf
  node = doc->getTree().addContentItem(DSRTypes::RT_isRoot, DSRTypes::VT_Container);
  doc->getTree().getCurrentContentItem().setTemplateIdentification(
              "1000", "99QIICR");

  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("10001","99QIICR", "Quantitative measurement report"));

  // TID1204: Language of content item and descendants
  doc->getTree().addContentItem(DSRTypes::RT_hasConceptMod, DSRTypes::VT_Code, DSRTypes::AM_belowCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121049", "DCM", "Language of Content Item and Descendants"));
  doc->getTree().getCurrentContentItem().setCodeValue(
              DSRCodedEntryValue("eng","RFC3066","English"));

  doc->getTree().addContentItem(DSRTypes::RT_hasConceptMod, DSRTypes::VT_Code, DSRTypes::AM_belowCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121046", "DCM", "Country of Language"));
  doc->getTree().getCurrentContentItem().setCodeValue(
              DSRCodedEntryValue("US","ISO3166_1","United States"));
  doc->getTree().goUp();

  // TODO: TID 1001 Observation context
  doc->getTree().addContentItem(DSRTypes::RT_hasObsContext, DSRTypes::VT_Code, DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121005","DCM","Observer Type"));
  doc->getTree().getCurrentContentItem().setCodeValue(
              DSRCodedEntryValue("121007","DCM","Device"));

  // TODO: need to decide what UIDs we will use
  doc->getTree().addContentItem(DSRTypes::RT_hasObsContext, DSRTypes::VT_UIDRef, DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121012","DCM","Device Observer UID"));
  doc->getTree().getCurrentContentItem().setStringValue("0.0.0.0");

  doc->getTree().addContentItem(DSRTypes::RT_hasObsContext, DSRTypes::VT_Text, DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121013","DCM","Device Observer Name"));
  doc->getTree().getCurrentContentItem().setStringValue("QIICR");

  doc->getTree().addContentItem(DSRTypes::RT_hasObsContext, DSRTypes::VT_Text, DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121014","DCM","Device Observer Manufacturer"));
  doc->getTree().getCurrentContentItem().setStringValue("QIICR");

  doc->getTree().addContentItem(DSRTypes::RT_hasObsContext, DSRTypes::VT_Text, DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121015","DCM","Device Observer Model Name"));
  doc->getTree().getCurrentContentItem().setStringValue("0.0.1");

  doc->getTree().addContentItem(DSRTypes::RT_hasObsContext, DSRTypes::VT_Text, DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121016","DCM","Device Observer Serial Number"));
  if(doc->getTree().getCurrentContentItem().setStringValue("NA").bad()){
    std::cout << "Failed to set serial number!" << std::endl;
  }

  // TID 4020: Image library
  //doc->getTree().goUp();
  node = doc->getTree().addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Container, DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("111028", "DCM", "Image Library"));
  std::cout << "Image library: " << node << std::endl;
  for(int i=0;i<referencedImages.size();i++){
    std::cout << "Adding image library for " << referencedImages[i] << std::endl;
    addImageLibraryEntry(doc, referencedImages[i]);
  }

  doc->getTree().addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Container, DSRTypes::AM_belowCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121070","DCM","Findings"));

  //
  node = doc->getTree().addContentItem(
              DSRTypes::RT_hasObsContext, DSRTypes::VT_Text, DSRTypes::AM_belowCurrent);

  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("112039","DCM","Tracking Identifier"));
  doc->getTree().getCurrentContentItem().setStringValue("Object1");

  //
  node = doc->getTree().addContentItem(
              DSRTypes::RT_hasObsContext, DSRTypes::VT_UIDRef, DSRTypes::AM_afterCurrent);
  std::cout << "node added: " << node << std::endl;

  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("112040","DCM","Tracking Unique Identifier"));
  char trackingUID[128];
  dcmGenerateUniqueIdentifier(trackingUID, SITE_SERIES_UID_ROOT);
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
                                DSRTypes::VT_Container,
                                DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setTemplateIdentification(
              "1419", "DCMR");

  doc->getTree().addContentItem(DSRTypes::RT_contains,
                                DSRTypes::VT_Num,
                                DSRTypes::AM_afterCurrent);
  if(doc->getTree().getCurrentContentItem().setNumericValue(measurement).bad()){
    std::cerr << "Failed to set measurement" << std::endl;
  }

  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("R-00317","SRT","Mean"));



  // Current Requested Procedure Evidence Sequence is required since
  //  composite SOP instances are referenced

  //datasetSR->putAndInsertString(DCM_StudyInstanceUID, segStudyUIDStr.c_str());

  DcmItem *seq1, *seq2, *seq3, *seq4;
  datasetSR->findOrCreateSequenceItem(DCM_CurrentRequestedProcedureEvidenceSequence,
                                      seq1);
  // unless we have access to the database, we cannot query the series
  // instance UID for the source images; so assume it is the same as for
  // the segmentation object
  // List all source images and the segmentation object in this sequence

  seq1->putAndInsertString(DCM_StudyInstanceUID, segStudyUIDStr.c_str());

  seq1->findOrCreateSequenceItem(DCM_ReferencedSeriesSequence, seq2, 0);
  std::cout << "inserted image instance uid " << imageSeriesInstanceUIDPtr << std::endl;
  seq2->putAndInsertString(DCM_SeriesInstanceUID, imageSeriesInstanceUIDPtr);
  for(int i=0;i<referencedClassUIDs.size();i++){
    seq2->findOrCreateSequenceItem(DCM_ReferencedSOPSequence, seq3, i);
    seq3->putAndInsertString(DCM_ReferencedSOPClassUID,
                             referencedClassUIDs[i].c_str());
    seq3->putAndInsertString(DCM_ReferencedSOPInstanceUID,
                             referencedInstanceUIDs[i].c_str());
   }

  // reference the segmentation object
  seq1->findOrCreateSequenceItem(DCM_ReferencedSeriesSequence, seq2, 1);
  std::cout << "inserted seg instance uid " << segInstanceUIDPtr << std::endl;
  seq2->putAndInsertString(DCM_SeriesInstanceUID, segSeriesUIDStr.c_str());
  seq2->findOrCreateSequenceItem(DCM_ReferencedSOPSequence, seq3);
  seq3->putAndInsertString(DCM_ReferencedSOPClassUID,
                           UID_SegmentationStorage);
  seq3->putAndInsertString(DCM_ReferencedSOPInstanceUID,
                           segInstanceUIDStr.c_str());

  // TODO: initialize series/study UID
  //       use custom root UID for initialization
  //seq->putAndInsertString(DCM_MappingResource, "DCMR");
  //seq->putAndInsertString(DCM_TemplateIdentifier, "1411");

  // Initialize Patient module
  //copyDcmElement(DCM_PatientName, datasetSEG, datasetSR);
  //copyDcmElement(DCM_PatientID, datasetSEG, datasetSR);
  //copyDcmElement(DCM_PatientBirthDate, datasetSEG, datasetSR);
  //copyDcmElement(DCM_PatientSex, datasetSEG, datasetSR);
  OFString contentDate, contentTime;
  DcmDate::getCurrentDate(contentDate);
  DcmTime::getCurrentTime(contentTime);

  datasetSR->putAndInsertString(DCM_ContentDate, contentDate.c_str());
  datasetSR->putAndInsertString(DCM_ContentTime, contentTime.c_str());
  datasetSR->putAndInsertString(DCM_SeriesDate, contentDate.c_str());
  datasetSR->putAndInsertString(DCM_SeriesTime, contentTime.c_str());
  datasetSR->putAndInsertString(DCM_StudyDate, contentDate.c_str());
  datasetSR->putAndInsertString(DCM_StudyTime, contentTime.c_str());
  //datasetSR->putAndInsertString(DCM_AcquisitionDate, contentDate.c_str());
  //datasetSR->putAndInsertString(DCM_AcquisitionTime, contentTime.c_str());

  doc->getCodingSchemeIdentification().addPrivateDcmtkCodingScheme();
  doc->write(*datasetSR);

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

void copyDcmElement(const DcmTag& tag, DcmDataset* dcmIn, DcmDataset* dcmOut)
{
  char *str;
  DcmElement* element;
  DcmTag copy = tag;
  OFCondition cond = dcmIn->findAndGetElement(tag, element);
  if(cond.good())
  {
    element->getString(str);
    dcmOut->putAndInsertString(tag, str);
  }
  else
  {
    dcmOut->putAndInsertString(tag, "");
  }
}

std::string getDcmElementAsString(const DcmTag& tag, DcmDataset* dcmIn)
{
  char *str = NULL;
  DcmElement* element;
  DcmTag copy = tag;
  OFCondition cond = dcmIn->findAndGetElement(tag, element);
  if(cond.good())
  {
    element->getString(str);
  }
  return std::string(str);
}

void addImageLibraryEntry(DSRDocument *doc, std::string imageFileName){
  // read the image
  DcmFileFormat *fileformat = new DcmFileFormat();
  fileformat->loadFile(imageFileName.c_str());
  DcmDataset *dataset = fileformat->getDataset();
  DcmElement *element;
  DcmItem *sequenceItem;

  std::string sopClassUID, sopInstanceUID;
  char* elementStr;
  float* elementFloat;
  OFString elementOFString;
  dataset->findAndGetElement(DCM_SOPClassUID, element);
  element->getString(elementStr);
  sopClassUID = std::string(elementStr);

  dataset->findAndGetElement(DCM_SOPInstanceUID, element);
  element->getString(elementStr);
  sopInstanceUID = std::string(elementStr);

  WARN_IF_ERROR(doc->getTree().addContentItem(DSRTypes::RT_contains,DSRTypes::VT_Image, DSRTypes::AM_belowCurrent),"Image library image");
  DSRImageReferenceValue imageReference = DSRImageReferenceValue(sopClassUID.c_str(), sopInstanceUID.c_str());
  doc->getTree().getCurrentContentItem().setImageReference(imageReference);

  DSRCodedEntryValue codedValue;

  // Image Laterality
  if(dataset->findAndGetSequenceItem(DCM_ImageLaterality,sequenceItem).good()){
     if(findAndGetCodedValueFromSequenceItem(sequenceItem, codedValue)){
       WARN_IF_ERROR(doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                     DSRTypes::VT_Code,
                                     DSRTypes::AM_afterCurrent),"Image laterality");
       doc->getTree().getCurrentContentItem().setConceptName(
                   DSRCodedEntryValue("111027","DCM","Image Laterality"));
       doc->getTree().getCurrentContentItem().setCodeValue(codedValue);
     }
  }

  // Image View
  if(dataset->findAndGetSequenceItem(DCM_ViewCodeSequence,sequenceItem).good()){
    if(findAndGetCodedValueFromSequenceItem(sequenceItem,codedValue)){
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Code,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111031","DCM","Image View"));
      doc->getTree().getCurrentContentItem().setCodeValue(codedValue);

      if(dataset->findAndGetSequenceItem(DCM_ViewModifierCodeSequence,sequenceItem).good()){
        if(findAndGetCodedValueFromSequenceItem(sequenceItem,codedValue)){
          doc->getTree().addContentItem(DSRTypes::RT_hasConceptMod,
                                        DSRTypes::VT_Code,
                                        DSRTypes::AM_belowCurrent);
          doc->getTree().getCurrentContentItem().setConceptName(
                      DSRCodedEntryValue("111032","DCM","Image View Modifier"));
          doc->getTree().getCurrentContentItem().setCodeValue(codedValue);
          doc->getTree().goUp();
        }
      }
    }
  }

  // Patient Orientation - Row and Column separately
  if(dataset->findAndGetElement(DCM_PatientOrientation, element).good()){
      element->getOFString(elementOFString, 0);
      WARN_IF_ERROR(doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                                  DSRTypes::VT_Text,
                                                  DSRTypes::AM_afterCurrent),"Patient orientation");
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111044","DCM","Patient Orientation Row"));
      doc->getTree().getCurrentContentItem().setStringValue(elementOFString.c_str());

      element->getOFString(elementOFString, 1);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Text,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111043","DCM","Patient Orientation Column"));
      doc->getTree().getCurrentContentItem().setStringValue(elementOFString.c_str());
  }

  // Study date
  std::cout << "Before adding study date" << std::endl;
  if(dataset->findAndGetElement(DCM_StudyDate, element).good()){
      std::cout << "Adding study date" << std::endl;
      element->getOFString(elementOFString, 0);
      WARN_IF_ERROR(doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                                  DSRTypes::VT_Date,
                                                  DSRTypes::AM_afterCurrent),"Study date");
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111060","DCM","Study Date"));
      doc->getTree().getCurrentContentItem().setStringValue(elementOFString.c_str());
  }

  // Study time
  if(dataset->findAndGetElement(DCM_StudyTime, element).good()){

      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Time,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111061","DCM","Study Time"));
      doc->getTree().getCurrentContentItem().setStringValue(elementOFString.c_str());
  }

  // Content date
  if(dataset->findAndGetElement(DCM_ContentDate, element).good()){

      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Date,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111018","DCM","Content Date"));
      doc->getTree().getCurrentContentItem().setStringValue(elementOFString.c_str());
  }

  // Content time
  if(dataset->findAndGetElement(DCM_ContentTime, element).good()){
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Time,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111019","DCM","Content Time"));
      doc->getTree().getCurrentContentItem().setStringValue(elementOFString.c_str());
  }

  // Pixel Spacing - horizontal and vertical separately
  if(dataset->findAndGetElement(DCM_PixelSpacing, element).good()){
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111026","DCM","Horizontal Pixel Spacing"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("mm","UCUM","millimeter")));

      element->getOFString(elementOFString, 1);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111066","DCM","Vertical Pixel Spacing"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("mm","UCUM","millimeter")));
  }

  // Positioner Primary Angle
  if(dataset->findAndGetElement(DCM_PositionerPrimaryAngle, element).good()){

      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("112011","DCM","Positioner Primary Angle"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("deg","UCUM","degrees of plane angle")));

  }

  // Positioner Secondary Angle
  if(dataset->findAndGetElement(DCM_PositionerSecondaryAngle, element).good()){

      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("112012","DCM","Positioner Secondary Angle"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("deg","UCUM","degrees of plane angle")));
  }

  // TODO
  // Spacing between slices: May be computed from the Image Position (Patient) (0020,0032)
  // projected onto the normal to the Image Orientation (Patient) (0020,0037) if present;
  // may or may not be the same as the Spacing Between Slices (0018,0088) if present.

  // Slice thickness/
  if(dataset->findAndGetElement(DCM_SliceThickness, element).good()){

      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("112225","DCM","Slice Thickness"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("mm","UCUM","millimeter")));
  }

  // Frame of reference
  if(dataset->findAndGetElement(DCM_FrameOfReferenceUID, element).good()){

      element->getOFString(elementOFString,0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_UIDRef,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("112227","DCM","Frame of Reference UID"));
      std::cout << "Setting Frame of reference UID to " << elementOFString.c_str() << std::endl;
      doc->getTree().getCurrentContentItem().setStringValue(elementOFString);
  }

  // Image Position Patient
  if(dataset->findAndGetElement(DCM_ImagePositionPatient, element).good()){
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110901","DCM","Image Position (Patient) X"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("mm","UCUM","millimeter")));

      element->getOFString(elementOFString, 1);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110902","DCM","Image Position (Patient) Y"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("mm","UCUM","millimeter")));

      element->getOFString(elementOFString, 2);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110903","DCM","Image Position (Patient) Z"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("mm","UCUM","millimeter")));
  }

  // Image Orientation Patient
  if(dataset->findAndGetElement(DCM_ImageOrientationPatient, element).good()){
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110904","DCM","Image Orientation (Patient) Row X"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{-1:1}","UCUM","{-1:1}")));

      element->getOFString(elementOFString, 1);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110905","DCM","Image Orientation (Patient) Row Y"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{-1:1}","UCUM","{-1:1}")));

      element->getOFString(elementOFString, 2);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110906","DCM","Image Orientation (Patient) Row Z"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{-1:1}","UCUM","{-1:1}")));

      element->getOFString(elementOFString, 3);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110907","DCM","Image Orientation (Patient) Column X"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{-1:1}","UCUM","{-1:1}")));

      element->getOFString(elementOFString, 4);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110908","DCM","Image Orientation (Patient) Column Y"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{-1:1}","UCUM","{-1:1}")));

      element->getOFString(elementOFString, 5);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110909","DCM","Image Orientation (Patient) Column Z"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{-1:1}","UCUM","{-1:1}")));

  }

  // Image Orientation Patient
  if(dataset->findAndGetElement(DCM_Rows, element).good()){
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110910","DCM","Pixel Data Rows"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{pixels}","UCUM","pixels")));

      dataset->findAndGetElement(DCM_Columns, element);
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110911","DCM","Pixel Data Columns"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{pixels}","UCUM","pixels")));
  }


  doc->getTree().goUp(); // up to image level
  doc->getTree().goUp(); // up to image library container level
}

bool findAndGetCodedValueFromSequenceItem(DcmItem *item, DSRCodedEntryValue value){
  char *elementStr;
  std::string codeMeaning, codeValue, codingSchemeDesignator;
  DcmElement *element;

  if(item->findAndGetElement(DCM_CodeValue, element).good()){
    element->getString(elementStr);
    codeValue = std::string(elementStr);
  }

  if(item->findAndGetElement(DCM_CodeMeaning, element).good()){
    element->getString(elementStr);
    codeMeaning = std::string(elementStr);
  }

  if(item->findAndGetElement(DCM_CodingSchemeDesignator, element).good()){
    element->getString(elementStr);
    codingSchemeDesignator = std::string(elementStr);
  }

  value.setCode(codeValue.c_str(), codingSchemeDesignator.c_str(), codeMeaning.c_str());
  return true;
}
