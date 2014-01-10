// STL includes
#include <iostream>
#include <string>


// DCMTK includes
#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/dcmsr/dsrdoc.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcfilefo.h"

int main(int argc, char** argv)
{
	DcmFileFormat *fileformat = new DcmFileFormat();
  DcmDataset *dataset = fileformat->getDataset();

  OFString reportUID;

  DSRDocument *doc = new DSRDocument();
  
  doc->createNewDocument(DSRTypes::DT_ComprehensiveSR);
  doc->setStudyDescription("OFFIS Structured Reporting Test");
  doc->setSeriesDescription("Valid report with loop/cycle");

  doc->setPatientName("Loop^Mr");
  doc->setPatientSex("M");

  doc->getTree().addContentItem(DSRTypes::RT_isRoot, DSRTypes::VT_Container);
  doc->getTree().getCurrentContentItem().setConceptName(DSRCodedEntryValue("TST.01", OFFIS_CODING_SCHEME_DESIGNATOR, "Document Title"));

  size_t node1 = doc->getTree().addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Text, DSRTypes::AM_belowCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(DSRCodedEntryValue("TST.02", OFFIS_CODING_SCHEME_DESIGNATOR, "First Paragraph"));
  doc->getTree().getCurrentContentItem().setStringValue("Some text.");

  size_t node2 = doc->getTree().addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Text);
  doc->getTree().getCurrentContentItem().setConceptName(DSRCodedEntryValue("TST.03", OFFIS_CODING_SCHEME_DESIGNATOR, "Second Paragraph"));
  doc->getTree().getCurrentContentItem().setStringValue("Some more text.");

  doc->getTree().addByReferenceRelationship(DSRTypes::RT_inferredFrom, node1);
  doc->getTree().gotoNode(node1);
  doc->getTree().addByReferenceRelationship(DSRTypes::RT_inferredFrom, node2);

  // write the result
  doc->getCodingSchemeIdentification().addPrivateDcmtkCodingScheme();
  doc->write(*dataset);
  fileformat->saveFile("report.dcm", EXS_LittleEndianExplicit);

  return 0;
}
