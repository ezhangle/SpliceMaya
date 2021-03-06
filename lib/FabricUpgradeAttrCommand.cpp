//
// Copyright (c) 2010-2016, Fabric Software Inc. All rights reserved.
//

#include "FabricUpgradeAttrCommand.h"
#include "FabricSpliceConversion.h"

#include <maya/MStringArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MGlobal.h>
#include <maya/MFnAttribute.h>
#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>

#include "FabricSpliceBaseInterface.h"
#include "FabricDFGBaseInterface.h"
#include "FabricMayaAttrs.h"

#define kNodesFlag "-n"
#define kNodesFlagLong "-nodes"

MSyntax FabricUpgradeAttrCommand::newSyntax()
{
  MSyntax syntax;
  syntax.addFlag(kNodesFlag, kNodesFlagLong, MSyntax::kString);
  syntax.enableQuery(false);
  syntax.enableEdit(false);
  return syntax;
}

void* FabricUpgradeAttrCommand::creator()
{
  return new FabricUpgradeAttrCommand;
}

MStatus FabricUpgradeAttrCommand::doIt(const MArgList &args)
{
  MStatus status;
  MArgParser argData(syntax(), args, &status);

  MStringArray nodeNames;
  if(argData.isFlagSet("nodes"))
  {
    MString nodes = argData.flagArgumentString("nodes", 0);
    nodes.split(',', nodeNames);
  }
  else
  {
    MStringArray spliceMayaNodes, dfgMayaNodes;
    MGlobal::executeCommand("ls -type \"spliceMayaNode\";", spliceMayaNodes);
    MGlobal::executeCommand("ls -type \"dfgMayaNode\";", dfgMayaNodes);
    for(unsigned int i=0;i<spliceMayaNodes.length();i++)
      nodeNames.append(spliceMayaNodes[i]);
    for(unsigned int i=0;i<dfgMayaNodes.length();i++)
      nodeNames.append(dfgMayaNodes[i]);
  }

  for(unsigned int i=0;i<nodeNames.length();i++)
  {
    MString nodeName = nodeNames[i];
    MGlobal::displayInfo("Processing attributes on node '"+nodeName+"'...");

    MString nodeType;
    MGlobal::executeCommand("nodeType \""+nodeName+"\";", nodeType);

    FabricSpliceBaseInterface * spliceInterf = NULL;

    if(nodeType == "spliceMayaNode")
    {
      MGlobal::displayInfo("Processing attributes on Splice node '"+nodeName+"'...");
      spliceInterf = FabricSpliceBaseInterface::getInstanceByName(nodeName.asChar());
    }
    else
    {
      MGlobal::displayInfo("Skipping attributes on Canvas node '"+nodeName+"'");
      continue;
    }

    MStringArray attrNames;
    MGlobal::executeCommand("listAttr -userDefined \"" + nodeName + "\";", attrNames);

    MSelectionList selList;
    MGlobal::getSelectionListByName(nodeName, selList);
    MObject nodeObj;
    selList.getDependNode(0, nodeObj);

    MFnDependencyNode node(nodeObj);

    std::vector < MPlugArray >  srcConns, dstConns;
    MStringArray types;
    MStringArray values;

    // memorize values + connections
    for(unsigned j=0;j<attrNames.length();j++)
    {
      MString attrName = attrNames[j];

      MPlug plug = node.findPlug(attrName);

      MPlugArray srcConn, dstConn;
      plug.connectedTo(srcConn, true, false);
      plug.connectedTo(dstConn, false, true);
      srcConns.push_back(srcConn);
      dstConns.push_back(dstConn);

      if(plug.isCompound())
      {
        values.append("");
        types.append("compound");
      }
      else
      {
        MString attrType;
        MGlobal::executeCommand("getAttr -type \""+nodeName+"."+attrName+"\";", attrType);
        types.append(attrType);
        if(attrType == "string")
        {
          MString value;
          MGlobal::executeCommand("getAttr \""+nodeName+"."+attrName+"\";", value);
          values.append(value);
        }
        else if(attrType == "int" || attrType == "bool")
        {
          int value;
          MGlobal::executeCommand("getAttr \""+nodeName+"."+attrName+"\";", value);
          MString valueStr;
          valueStr.set(value);
          values.append(valueStr);
        }
        else if(attrType.length() > 0)
        {
          double value;
          MGlobal::executeCommand("getAttr \""+nodeName+"."+attrName+"\";", value);
          MString valueStr;
          valueStr.set(value);
          values.append(valueStr);
        }
        else // this happens for PolygonMesh + Lines etc..
        {
          values.append("");
        }
      }
    }

    // remove attributes
    for(unsigned j=0;j<attrNames.length();j++)
    {
      MString attrName = attrNames[j];
      MPlug plug = node.findPlug(attrName);
      if(plug.isNull())
        continue;
      MGlobal::executeCommand("deleteAttr -attribute \""+attrName+"\" \""+nodeName+"\";", true, false);
    }

    // recreate all attributes
    for(unsigned j=0;j<attrNames.length();j++)
    {
      MString attrName = attrNames[j];

      // skip the attributes which don't have matching ports
      if(spliceInterf)
      {
        FabricSplice::DGPort port = spliceInterf->getPort(attrName);
        if(port.isValid())
        {
          MGlobal::displayInfo("addAttr '" + attrName + "' ...");
          spliceInterf->createAttributeForPort(port);
        }
        else
          continue;
      }
    }

    // re-establish values + connections
    for(unsigned j=0;j<attrNames.length();j++)
    {
      MString attrName = attrNames[j];
      if(attrName.length() > 2)
      {
        MString rightBit = attrName.substring(attrName.length()-2, attrName.length()-1);
        if(rightBit == "_x")
          attrName = attrName.substring(0, attrName.length() - 3) + "X";
        else if(rightBit == "_y")
          attrName = attrName.substring(0, attrName.length() - 3) + "X";
        else if(rightBit == "_z")
          attrName = attrName.substring(0, attrName.length() - 3) + "X";
        else if(rightBit == "_r")
          attrName = attrName.substring(0, attrName.length() - 3) + "R";
        else if(rightBit == "_g")
          attrName = attrName.substring(0, attrName.length() - 3) + "G";
        else if(rightBit == "_b")
          attrName = attrName.substring(0, attrName.length() - 3) + "B";
      }

      MPlug plug = node.findPlug(attrName);
      if(plug.isNull())
        continue;

      // set the previous value
      if(!plug.isCompound() && values[j].length() > 0)
      {
        if(types[j] == "string")
          MGlobal::executeCommand("setAttr \""+nodeName+"."+attrName+"\" \""+values[j]+"\";", true, false);
        else
          MGlobal::executeCommand("setAttr \""+nodeName+"."+attrName+"\" "+values[j]+";", true, false);
      }

      // reconnect all plugs
      MPlugArray srcConn = srcConns[j];
      MPlugArray dstConn = dstConns[j];
      for(unsigned int k=0;k<srcConn.length();k++)
      {
        MString cmd = "connectAttr \"" + srcConn[k].name() + "\" ";
        cmd += "\"" + plug.name() +"\";";
        MGlobal::executeCommand(cmd, true, false);
      }
      for(unsigned int k=0;k<dstConn.length();k++)
      {
        MString cmd = "connectAttr \"" + plug.name() + "\" ";
        cmd += "\"" + dstConn[k].name() +"\";";
        MGlobal::executeCommand(cmd, true, false);
      }
    }

    MGlobal::displayInfo("Completed node '"+nodeName+"'.");
  }

  return status;
}
